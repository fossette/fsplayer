/*
 * File:        fsplayer.c
 *
 * Author:      fossette
 *
 * Date:        2019/05/29
 *
 * Version:     1.0
 *
 * Description: Play a video file using the VLC library in fullscreen mode.
 *              If the video size is bigger than the screen, it is shrunk
 *              to the screen size.  If the video size is smaller than the
 *              screen, the video is played as is surrounded by a black
 *              background.  Tested under:
 *                - FreeBSD 11.2 using libvlc 3.0.6, libX11 1.6.7
 *                  and qvwm 1.1.12_2015.
 *
 *              Does not work under:
 *                - Debian 10 using libvlc 3.0.6, libX11 1.6.7
 *                  and Gnome 3.30.2.
 *                    libvlc doesn't seem to open a video window,
 *                    although the sound is being played.  This
 *                    part of the project will be on the ice until
 *                    Debian Linux release a new VLC package.
 *
 *              Should be fairly easy to port because there are not
 *              that many dependencies.
 *
 *              Accepted keyboard keys are:
 *                - ESC          Stop the video
 *                - ARROW RIGHT  Jump forward 10 seconds
 *                - ARROW LEFT   Jump backward 10 seconds
 *                - ARROW UP     Jump forward 1 minutes
 *                - ARROW DOWN   Jump backward 1 minutes
 *                - PAGE UP      Jump forward 10 minutes
 *                - PAGE DOWN    Jump backward 10 minutes
 *                - HOME         Jump to the beginning
 *                - END          Jump 10 seconds before the end
 *                - SPACE        Pause the video
 *
 *              Accepted keypad keys are:
 *                - +            Increase the volume
 *                - -            Decrease the volume
 *                - *            Change the audio track
 *                - 1-9          Move a smaller view to the
 *                               specified area within the screen.
 *
 *              Note that since fullscreen mode is not well implemented
 *              and/or documented in X11 and window managers, the
 *              technique used here is simply to offset the window
 *              decorations out of the visible screen area.  However,
 *              minimal MOTIF wizardry is attempted first should a
 *              'modern' window manager be present.
 *
 *              Known issues:
 *                1) There's a 1 second short play before fullscreen is
 *                   kicked in.  This is because libvlc creates its
 *                   playing thread only at the PLAY command, and there's
 *                   a delay before video stats are made available.
 *
 *                2) Can't run fsplayer with other instances of fsplayer
 *                   or even have VLC open.  This is because libvlc
 *                   doesn't provide a way to get the window-id that it
 *                   creates.
 *
 *                3) There's a 0.4 second max delay when pressing the
 *                   video browsing keys.  This will get fixed when I'll
 *                   have a better understanding of X11.
 *
 * Parameter:   The video file to play.
 *
 * Web:         https://github.com/fossette/fsplayer/wiki
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vlc/vlc.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/extensions/xf86vmode.h>




/*
 *  Constants
 */

// # define FSPLAYER_DEBUG           1
#define FSPLAYER_10SEC           10000
#define FSPLAYER_1MIN            60000
#define FSPLAYER_10MIN           600000
#define FSPLAYER_LIBVLCWMNAME    "VLC media player"
#define FSPLAYER_WMNAME          "fsplayer"

#define ERROR_FSPLAYER_USAGE     1
#define ERROR_FSPLAYER_X11       2
#define ERROR_FSPLAYER_VLC       3
#define ERROR_FSPLAYER_FAIL      4
#define ERROR_FSPLAYER_MEM       5
#define ERROR_FSPLAYER_VLCRUN    6

#define LNSZ                     200




/*
 *  FilenameExist
 */

int
FilenameExist(const char *szFilename)
{
   int i;
   struct stat sStat;


   i = !stat(szFilename,     &sStat);
   if (i)
      i = (sStat.st_mode & S_IFREG);

   return(i);
}




/*
 *  MapState2sz
 */

void
MapState2sz(long iState,     char *pState)
{
   if (iState == IsUnmapped)
      strcpy(pState, "IsUnmapped");
   else if (iState == IsUnviewable)
      strcpy(pState, "IsUnviewable");
   else if (iState == IsViewable)
      strcpy(pState, "IsViewable");
   else
      sprintf(pState, "%ld", iState);
}




/*
 *  FindMaster
 */

void
FindMaster(Display *pX11Display, Window w,     Window *pMaster)
{
   int            iRet;
   unsigned int   nChildren;
   Window         wParent,
                  wRoot,
                  *pChildren;


   *pMaster = w;

   do
   {
      pChildren = NULL;
      iRet = XQueryTree(pX11Display, w,     &wRoot, &wParent,
                                            &pChildren, &nChildren);
      if (pChildren)
         XFree(pChildren);
      if (iRet)
      {
         if (wParent == wRoot)
            *pMaster = w;
         else
            w = wParent;
      }
   }
   while (iRet && wParent != wRoot);
}




/*
 *  FindX11Window
 */

int
FindX11Window(Display *pX11Display, Window w, Atom aName, Atom aType,
              char *szValue,     Window *pReturn, Window *pMaster)
{
   int            i,
                  iFormat,
                  iFound = 0,
                  iRet;
   unsigned int   nChildren;
   unsigned long  iNumItems,
                  iByteRemaining;
   unsigned char  *pProperty = NULL;
   Atom           aRet;
   Status         iStatus;
   Window         wParent,
                  wRoot,
                  *pChildren = NULL;


   iRet = XGetWindowProperty(pX11Display, w, aName, 0, 4, 0, aType,
             &aRet, &iFormat, &iNumItems, &iByteRemaining, &pProperty);
   if (iRet == Success && pProperty)
   {
      iFound = !strcmp((char *)pProperty, szValue);
      if (iFound)
         *pReturn = w;  // Note that pMaster was found earlier in the tree
   }

   // Check the window's children if needed
   if (!iFound && XQueryTree(pX11Display, w,     &wRoot, &wParent,
                                                 &pChildren, &nChildren))
   {
      if (pChildren && nChildren)
         for (i = 0 ; i < nChildren && !iFound ; i++)
            if (pChildren[i] != w && pChildren[i] != wRoot
                && pChildren[i] != wParent)
            {
               if (w == wRoot)
                  *pMaster = pChildren[i];
               iFound = FindX11Window(pX11Display, pChildren[i], aName,
                                      aType, szValue,
                                                        pReturn, pMaster);
            }
   }

   if (pChildren)
      XFree(pChildren);

   if (pProperty)
      XFree(pProperty);

   return(iFound);
}




/*
 *  FindVlcWindow
 */

int
FindVlcWindow(Display *pX11Display, Window wRoot,
                                       Window *pWinVlc, Window *pMaster)
{
   int  iErr = 0;
   Atom aName,
        aTypeStr;


   // WM_NAME(STRING) = "VLC media player"
   *pWinVlc = 0;  // The VLC window
   *pMaster = 0;  // The VLC parent window that has ROOT as its parent
   aName = XInternAtom(pX11Display, "WM_NAME", False);
   aTypeStr = XInternAtom(pX11Display, "STRING", False);
   if (aName != None && aTypeStr != None)
      FindX11Window(pX11Display, wRoot, aName, aTypeStr,
                    FSPLAYER_LIBVLCWMNAME,     pWinVlc, pMaster);
   if (!(*pWinVlc))
      iErr = ERROR_FSPLAYER_FAIL;

   return(iErr);
}




/*
 *  PositionWindow
 */

void
PositionWindow(Display *pX11Display, Window w, int iKeypadPos,
               unsigned int scrx, unsigned int scry)
{
   int            x,
                  y;
   unsigned int   iBorder,
                  iDepth,
                  iHeight,
                  iWidth;
   Window         wRoot;


   if (XGetGeometry(pX11Display, w,     &wRoot, &x, &y, &iWidth, &iHeight,
                                        &iBorder, &iDepth))
      if (iWidth < scrx && iHeight < scry)
      {
         if (iKeypadPos == 1 || iKeypadPos == 4 || iKeypadPos == 7)
            x = 0;
         else if (iKeypadPos == 3 || iKeypadPos == 6 || iKeypadPos == 9)
            x = scrx - iWidth;
         else
            x = (scrx - iWidth) / 2;

         if (iKeypadPos == 7 || iKeypadPos == 8 || iKeypadPos == 9)
            y = 0;
         else if (iKeypadPos == 1 || iKeypadPos == 2 || iKeypadPos == 3)
            y = scry - iHeight;
         else
            y = (scry - iHeight) / 2;

         XMoveWindow(pX11Display, w, x, y);
      }
}




/*
 *  TaskbarFindAndUnmap
 *
 *  Very environment dependant.  This code currently looks for the xload
 *  program.  Other toolbar programs could also be tested if needed.
 */

void
TaskbarFindAndUnmap(Display *pX11Display, Window wRoot,
                                                      Window *pTaskbar)
{
   int      iRet;
   Atom     aName,
            aTypeStr;
   Window   wMaster = 0,
            wXload = 0;
   XWindowAttributes attrib;

#ifdef FSPLAYER_DEBUG
   char     szState[LNSZ];
#endif // FSPLAYER_DEBUG


   *pTaskbar = 0;
   // WM_NAME(STRING) = "xload"
   aName = XInternAtom(pX11Display, "WM_NAME", False);
   aTypeStr = XInternAtom(pX11Display, "STRING", False);
   if (aName != None && aTypeStr != None)
      FindX11Window(pX11Display, wRoot, aName, aTypeStr, "xload",
                                                      &wXload, &wMaster);
   if (wXload)
      if (XGetWindowAttributes(pX11Display, wMaster,     &attrib))
         if (attrib.override_redirect && attrib.map_state == IsViewable)
         {
            *pTaskbar = wMaster;

            //
            // Oldschool fullscreen, get rid of the taskbar
            //
            iRet = XUnmapWindow(pX11Display, wMaster);
#ifdef FSPLAYER_DEBUG
            printf("%d=XUnmapWindow(w=0x%lX)\n", iRet, wMaster);
#endif // FSPLAYER_DEBUG

            // Wait for the unmap to occur (so it seems)
            if (XGetWindowAttributes(pX11Display, wMaster,     &attrib))
            {
#ifdef FSPLAYER_DEBUG
               MapState2sz(attrib.map_state,     szState);
               printf("XGetWindowAttributes: w=0x%lX, x=%d, y=%d,"
                      " width=%d, h=%d, state=%s, OvRedir=%d\n", wMaster,
                      attrib.x, attrib.y, attrib.width, attrib.height,
                      szState, attrib.override_redirect);
#endif // FSPLAYER_DEBUG
            }
         }
}




/*
 *  TaskbarRaise
 */

void
TaskbarRaise(Display *pX11Display, Window wTaskbar)
{
   int iRet;
   XWindowAttributes attrib;

#ifdef FSPLAYER_DEBUG
   char szState[LNSZ];
#endif // FSPLAYER_DEBUG


   //
   // Oldschool fullscreen, restore the taskbar
   //
   if (wTaskbar)
   {
      iRet = XMapRaised(pX11Display, wTaskbar);
#ifdef FSPLAYER_DEBUG
      printf("%d=XMapRaised(w=0x%lX)\n", iRet, wTaskbar);
#endif // FSPLAYER_DEBUG

      // Wait for the raise to occur (so it seems)
      if (XGetWindowAttributes(pX11Display, wTaskbar,     &attrib))
      {
#ifdef FSPLAYER_DEBUG
         MapState2sz(attrib.map_state,     szState);
         printf("XGetWindowAttributes: w=0x%lX, x=%d, y=%d, width=%d,"
                " h=%d, state=%s, OvRedir=%d\n", wTaskbar, attrib.x,
                attrib.y, attrib.width, attrib.height, szState,
                attrib.override_redirect);
#endif // FSPLAYER_DEBUG
      }
   }
}




/*
 *  SetWindowFullscreen
 */

void
SetWindowFullscreen(Display *pX11Display, Window w, Window wMaster,
                    Window wRoot, unsigned int scrx, unsigned int scry)
{
   int    iRet,
          x = 0,
          y = 0;
   Bool   iOverrideRedirect;
   Atom   aMotifWmHints,
          aWmState,
          aWmStateFs;
   Window w2;
   XEvent ev;
   XSetWindowAttributes attribSet;
   XWindowAttributes attribGet;


   //
   // Remove the window decorations
   //
   struct
   {
      unsigned long flags;
      unsigned long functions;
      unsigned long decorations;
      long input_mode;
      unsigned long status;
   } sMotifWmHints;

   memset(&sMotifWmHints, 0, sizeof(sMotifWmHints));
   sMotifWmHints.flags = 2;   // MWM_HINTS_DECORATIONS;
   sMotifWmHints.decorations = 0;
   aMotifWmHints = XInternAtom(pX11Display, "_MOTIF_WM_HINTS", False);
   if (aMotifWmHints != None)
      XChangeProperty(pX11Display, w, aMotifWmHints, aMotifWmHints, 32,
                      PropModeReplace, (unsigned char *)&sMotifWmHints,
                      sizeof(sMotifWmHints) / sizeof(long));

   iRet = XMoveResizeWindow(pX11Display, w, 0, 0, scrx, scry);

#ifdef FSPLAYER_DEBUG
   printf("%d=XMoveResizeWindow(0x%lX)\n", iRet, w);
#endif // FSPLAYER_DEBUG

   //
   // Notify the parent window to go fullscreen
   //
   aWmState = XInternAtom(pX11Display, "_NET_WM_STATE", False);
   aWmStateFs = XInternAtom(pX11Display, "_NET_WM_STATE_FULLSCREEN",
                            False);
   if (aWmState != None && aWmStateFs != None)
   {
      memset(&ev, 0, sizeof(ev));
      ev.type = ClientMessage;
      ev.xclient.window = w;
      ev.xclient.message_type = aWmState;
      ev.xclient.format = 32;
      ev.xclient.data.l[0] = 1;
      ev.xclient.data.l[1] = aWmStateFs;
      ev.xclient.data.l[2] = 0;
      if (!XSendEvent(pX11Display, wRoot, 0, SubstructureNotifyMask, &ev))
         printf("WARNING: XSendEvent(_NET_WM_STATE) aborted!\n");
   }

   //
   // Oldschool fullscreen, just move the window decorations out of view
   //
   iRet = XTranslateCoordinates(pX11Display, w, wRoot, 0, 0,
                                                            &x, &y, &w2);
#ifdef FSPLAYER_DEBUG
   printf("%d=XTranslateCoordinates(w=0x%lX,     x=%d, y=%d, w2=0x%lX)\n", iRet, w, x, y, w2);
#endif // FSPLAYER_DEBUG

   if (x > 0 || y > 0)
   {
      if (XGetWindowAttributes(pX11Display, wMaster,     &attribGet))
      {
         iOverrideRedirect = attribGet.override_redirect;
         if (!iOverrideRedirect)
         {
            memset(&attribSet, 0, sizeof(attribSet));
            attribSet.override_redirect = 1;
            iRet = XChangeWindowAttributes(pX11Display, wMaster,
                                           CWOverrideRedirect, &attribSet);

#ifdef FSPLAYER_DEBUG
            printf("%d=XChangeWindowAttributes(w=0x%lX, OvRedir=1)\n",
                   iRet, wMaster);
#endif // FSPLAYER_DEBUG
         }
      }
      else
         iOverrideRedirect = 1;

      iRet = XMoveWindow(pX11Display, wMaster, -x, -y);

#ifdef FSPLAYER_DEBUG
      printf("%d=XMoveWindow(w=0x%lX, x=%d, y=%d)\n",
             iRet, wMaster, -x, -y);
#endif // FSPLAYER_DEBUG

      // Wait for the move to occur (so it seems)
      if (XGetWindowAttributes(pX11Display, wMaster,     &attribGet))
      {
#ifdef FSPLAYER_DEBUG
         printf("XGetWindowAttributes: w=0x%lX, x=%d, y=%d, width=%d,"
                " h=%d, OvRedir=%d\n", wMaster, attribGet.x, attribGet.y,
                attribGet.width, attribGet.height,
                attribGet.override_redirect);
#endif // FSPLAYER_DEBUG
      }

      if (!iOverrideRedirect)
      {
         attribSet.override_redirect = 0;
         iRet = XChangeWindowAttributes(pX11Display, wMaster,
                                        CWOverrideRedirect, &attribSet);

#ifdef FSPLAYER_DEBUG
         printf("%d=XChangeWindowAttributes(w=0x%lX, OvRedir=0)\n",
                                            iRet, wMaster);
#endif // FSPLAYER_DEBUG
      }

      // Wait for the attribute to return to normal (so it seems)
      if (XGetWindowAttributes(pX11Display, wMaster,     &attribGet))
      {
#ifdef FSPLAYER_DEBUG
         printf("XGetWindowAttributes: w=0x%lX, x=%d, y=%d, width=%d,"
                " h=%d, OvRedir=%d\n", wMaster, attribGet.x, attribGet.y,
                attribGet.width, attribGet.height,
                attribGet.override_redirect);
#endif // FSPLAYER_DEBUG
      }
   }
}




/*
 *  main
 */

int
main(int argc, char* argv[])
{
   int                        iDotClock,
                              iErr = 0,
                              iNumVlcAudioTracks,
                              iPlay = 0,
                              iRet,
                              iRunning = 1,
                              iVlcAudioTrack,
                              iX11DefaultScreen,
                              iX11fd,
                              *pVlcAudioTrackId = NULL,
                              x,                y;
   unsigned int               scrx,             scry,
                              vidx,             vidy;
   unsigned long              iX11Black;
   char                       szErr[LNSZ];
   struct timeval             sEventLoopTimeout;
   Atom                       aString,
                              aWmName;
   Display                    *pX11Display = NULL;
   fd_set                     readfds;
   KeyCode                    kcDown,           kcEnd,
                              kcEsc,            kcHome,
                              kcKpBegin,        kcKpDown,
                              kcKpEnd,          kcKpHome,
                              kcKpLeft,         kcKpMinus,
                              kcKpMult,         kcKpPlus,
                              kcKpPageDown,     kcKpPageUp,
                              kcKpUp,           kcKpRight,
                              kcLeft,           kcPgDown,
                              kcPgUp,           kcRight,
                              kcSpace,          kcUp;
   libvlc_time_t              iEndTimeMs,
                              iTimeMs;
   libvlc_instance_t          *pVlcInst = NULL;
   libvlc_media_t             *pVlcMedia;
   libvlc_media_player_t      *pVlcPlayer = NULL;
   libvlc_track_description_t *pVlcAudioTrackDesc,
                              *pVlcATD;
   Status                     iStatus;
   Window                     w,
                              wInput,
                              wInputMaster,
                              wMaster,
                              wRoot,
                              wTaskbar = 0,
                              wVlc;
   XEvent                     loopEvent;
   XF86VidModeModeLine        modeLine;
   XSetWindowAttributes       attribSet;

 
   *szErr = 0;
   memset(&modeLine, 0, sizeof(modeLine));

   if (argc != 2)
      iErr = ERROR_FSPLAYER_USAGE;
   if (!iErr)
   {
      if (!FilenameExist(argv[1]))
         iErr = ERROR_FSPLAYER_USAGE;
   }
   if (!iErr)
   {

      iStatus = XInitThreads();
#ifdef FSPLAYER_DEBUG
      if (iStatus)
         printf("X11 Thread Support Active!\n");
      else
         printf("X11 Thread Support Unavailable!\n");
#endif // FSPLAYER_DEBUG

      pX11Display = XOpenDisplay(NULL);
      if (!pX11Display)
      {
         iErr = ERROR_FSPLAYER_X11;
         strcat(szErr, "XOpenDisplay() failed!");
      }
   }
   if (!iErr)
   {
      iX11DefaultScreen = XDefaultScreen(pX11Display);
      wRoot = XDefaultRootWindow(pX11Display);
      iX11Black = XBlackPixel(pX11Display, iX11DefaultScreen);
      XF86VidModeGetModeLine(pX11Display, iX11DefaultScreen,
                                  &iDotClock, &modeLine);
      scrx=modeLine.hdisplay;
      scry=modeLine.vdisplay;
      if (modeLine.private)
      {
         XFree(modeLine.private);
         modeLine.privsize = 0;
         modeLine.private = NULL;
      }
      printf("X11 Screen Size: %dx%d.\n", scrx, scry);

      XF86VidModeSetViewPort(pX11Display, iX11DefaultScreen, 0, 0);

      // Check if VLC is already running, and if so, stop!
      //
      // This check is required because libvlc doesn't provide us
      // the window-id that it creates.
      iErr = FindVlcWindow(pX11Display, wRoot,     &wVlc, &wMaster);
      if (iErr)
         iErr = 0;   // All good, not supposed to run yet
      else
         iErr = ERROR_FSPLAYER_VLCRUN;

#ifdef FSPLAYER_DEBUG
      printf("TESTRUN FindVlcWindow: root=0x%lX, vlc=0x%lX, master=0x%lX\n",
             wRoot, wVlc, wMaster);
#endif // FSPLAYER_DEBUG
   }
   if (!iErr)
   {
      // Create the keyboard input/background window
      memset(&attribSet, 0, sizeof(attribSet));
      attribSet.background_pixel = iX11Black;
      attribSet.event_mask = KeyPressMask|ButtonReleaseMask;
      wInput = XCreateWindow(pX11Display, wRoot, 0, 0, scrx, scry,
                             0, 0, InputOutput, CopyFromParent,
                             CWBackPixel|CWEventMask,     &attribSet);
      if (!wInput)
      {
         iErr = ERROR_FSPLAYER_X11;
         strcat(szErr, "XCreateWindow(wInput) failed!");
      }
   }
   if (!iErr)
   {
      aString = XInternAtom(pX11Display, "STRING", False);
      aWmName = XInternAtom(pX11Display, "WM_NAME", False);
      if (aString == None || aWmName == None)
      {
         iErr = ERROR_FSPLAYER_X11;
         strcat(szErr, "XInternAtom() failed!");
      }
   }
   if (!iErr)
   {
      XChangeProperty(pX11Display, wInput, aWmName, aString, 8,
                      PropModeReplace, (unsigned char *)FSPLAYER_WMNAME, 8);

      iRet = XMapWindow(pX11Display, wInput);
#ifdef FSPLAYER_DEBUG
      printf("%d=XMapWindow(w=0x%lX)\n", iRet, wInput);
#endif // FSPLAYER_DEBUG

      // Initialize the relevant keycodes
      kcDown =       XKeysymToKeycode(pX11Display, XK_Down);
      kcEnd =        XKeysymToKeycode(pX11Display, XK_End);
      kcEsc =        XKeysymToKeycode(pX11Display, XK_Escape);
      kcHome =       XKeysymToKeycode(pX11Display, XK_Home);
      kcKpBegin =    XKeysymToKeycode(pX11Display, XK_KP_Begin);
      kcKpDown =     XKeysymToKeycode(pX11Display, XK_KP_Down);
      kcKpEnd =      XKeysymToKeycode(pX11Display, XK_KP_End);
      kcKpHome =     XKeysymToKeycode(pX11Display, XK_KP_Home);
      kcKpLeft =     XKeysymToKeycode(pX11Display, XK_KP_Left);
      kcKpMinus =    XKeysymToKeycode(pX11Display, XK_KP_Subtract);
      kcKpMult =     XKeysymToKeycode(pX11Display, XK_KP_Multiply);
      kcKpPageDown = XKeysymToKeycode(pX11Display, XK_KP_Page_Down);
      kcKpPageUp =   XKeysymToKeycode(pX11Display, XK_KP_Page_Up);
      kcKpPlus =     XKeysymToKeycode(pX11Display, XK_KP_Add);
      kcKpRight =    XKeysymToKeycode(pX11Display, XK_KP_Right);
      kcKpUp =       XKeysymToKeycode(pX11Display, XK_KP_Up);
      kcLeft =       XKeysymToKeycode(pX11Display, XK_Left);
      kcPgDown =     XKeysymToKeycode(pX11Display, XK_Page_Down);
      kcPgUp =       XKeysymToKeycode(pX11Display, XK_Page_Up);
      kcRight =      XKeysymToKeycode(pX11Display, XK_Right);
      kcSpace =      XKeysymToKeycode(pX11Display, XK_space);
      kcUp =         XKeysymToKeycode(pX11Display, XK_Up);
      if (!(kcDown && kcEnd && kcEsc && kcHome && kcKpBegin && kcKpDown
            && kcKpEnd && kcKpHome && kcKpLeft && kcKpMinus && kcKpMult
            && kcKpPageDown && kcKpPageUp && kcKpPlus && kcKpRight
            && kcKpUp && kcLeft && kcPgDown && kcPgUp && kcRight
            && kcSpace && kcUp))
         printf("WARNING: X11 keycodes weren't all found so some video"
                " browsing features may be missing at this time.\n");
                                 
      //Load the VLC engine
      printf("LibVLC Version %s, %s\n",
             libvlc_get_version(), libvlc_get_compiler());
      pVlcInst = libvlc_new(0, NULL);
      if (!pVlcInst)
      {
         iErr = ERROR_FSPLAYER_VLC;
         strcat(szErr, "libvlc_new() failed!");
      }
   }
   if (!iErr)
   {
      /* Create a new item */
      pVlcMedia = libvlc_media_new_path(pVlcInst, argv[1]);
      if (pVlcMedia)
      {
         /* Create a media player playing environement */
         pVlcPlayer = libvlc_media_player_new_from_media(pVlcMedia);
         if (!pVlcPlayer)
         {
            iErr = ERROR_FSPLAYER_VLC;
            strcat(szErr, "libvlc_media_player_new_from_media() failed!");
         }

         /* No need to keep the media now */
         libvlc_media_release(pVlcMedia);
      }
      else
      {
         iErr = ERROR_FSPLAYER_VLC;
         strcat(szErr, "libvlc_media_new_path() failed!");
      }
   }
   if (!iErr)
   {
      iPlay = 1;
      iErr = libvlc_media_player_play(pVlcPlayer);
      if (iErr)
         printf("Warning: VLC Play Failed!\n");
      libvlc_media_player_pause(pVlcPlayer);
      sleep(1); /* wait until the tracks are created */

#ifdef FSPLAYER_DEBUG
      printf("0x%X=libvlc_media_player_get_xwindow()\n",
             libvlc_media_player_get_xwindow(pVlcPlayer));
#endif // FSPLAYER_DEBUG

      iErr = libvlc_video_get_size(pVlcPlayer, 0,     &vidx, &vidy);
      if (iErr)
      {
         iErr = ERROR_FSPLAYER_VLC;
         strcat(szErr, "libvlc_video_get_size() didn't find the video!");
      }
   }
   if (!iErr)
   {
      iEndTimeMs = libvlc_media_player_get_length(pVlcPlayer);
      if (iEndTimeMs <= 0)
      {
         iErr = ERROR_FSPLAYER_VLC;
         strcat(szErr, "libvlc_media_player_get_length() failed!");
      }
   }
   if (!iErr)
   {
      iNumVlcAudioTracks = libvlc_audio_get_track_count(pVlcPlayer);
      if (iNumVlcAudioTracks > 1)
      {
         pVlcAudioTrackId = malloc(iNumVlcAudioTracks * sizeof(int));
         if (pVlcAudioTrackId)
         {
            memset(pVlcAudioTrackId, 0,
                   sizeof(iNumVlcAudioTracks * sizeof(int)));
            pVlcAudioTrackDesc = pVlcATD
               = libvlc_audio_get_track_description(pVlcPlayer);
            if (pVlcAudioTrackDesc)
            {
               for (iRet = 0 ; pVlcATD && iRet < iNumVlcAudioTracks
                             ; iRet++)
               {
                  printf("Audio track found: %s\n\n", pVlcATD->psz_name);
                  pVlcAudioTrackId[iRet] = pVlcATD->i_id;
                  pVlcATD = pVlcATD->p_next;
               }
               libvlc_track_description_list_release(pVlcAudioTrackDesc);
            }
         }
         else
            iErr = ERROR_FSPLAYER_MEM;
      }
   }
   if (!iErr)
   {      
      printf("Video %dx%d, length: %li sec., %d audio tracks.\n",
             vidx, vidy, iEndTimeMs/1000, iNumVlcAudioTracks);
      iErr = FindVlcWindow(pX11Display, wRoot,     &wVlc, &wMaster);
#ifdef FSPLAYER_DEBUG
      printf("FindVlcWindow: root=0x%lX, vlc=0x%lX, master=0x%lX\n",
             wRoot, wVlc, wMaster);

      // A stop here in Debian to check what libvlc has launched
      //gets(szErr);
#endif // FSPLAYER_DEBUG
   }
   if (!iErr)
   {
      FindMaster(pX11Display, wInput,     &wInputMaster);
      
      if (vidx >= scrx || vidy >= scry)
         SetWindowFullscreen(pX11Display, wVlc, wMaster, wRoot, scrx, scry);
      else
      {
         SetWindowFullscreen(pX11Display, wInput, wInputMaster, wRoot,
                             scrx, scry);

         // Test for VLC's fullscreen feature.  It dosen't work on all
         // X11 window managers (i.e. old ones) for libvlc v3.0.6.
         //
         //libvlc_set_fullscreen(pVlcPlayer, 1);

         PositionWindow(pX11Display, wMaster, 5 /* center */, scrx, scry);
      }
      TaskbarFindAndUnmap(pX11Display, wRoot,     &wTaskbar);

      // Play the media_player
      libvlc_media_player_set_time(pVlcPlayer, 0);
      iErr = libvlc_media_player_play(pVlcPlayer);
      if (iErr)
      {
         iErr = ERROR_FSPLAYER_VLC;
         strcat(szErr, "libvlc_media_player_play() failed!");
      }
   }

   //
   // X11 Event Loop
   //
   iX11fd = ConnectionNumber(pX11Display);
   FD_ZERO(&readfds);
   FD_SET(iX11fd, &readfds);
   while (iRunning && !iErr)
   {
      sEventLoopTimeout.tv_usec = 400000; // maximum sleep of 0.4 second
      sEventLoopTimeout.tv_sec = 1;
      select(iX11fd + 1, &readfds, 0, 0, &sEventLoopTimeout);

      while (XPending(pX11Display))
      {
         loopEvent.type = 0;
         XNextEvent(pX11Display,     &loopEvent);
         if (loopEvent.type == KeyPress)
         {
            if (kcDown && loopEvent.xkey.keycode == kcDown)
            {
               iTimeMs = libvlc_media_player_get_time(pVlcPlayer);
               if (iTimeMs <= FSPLAYER_1MIN)
                  iTimeMs = 0;
               else
                  iTimeMs -= FSPLAYER_1MIN;
               libvlc_media_player_set_time(pVlcPlayer, iTimeMs);
            }
            else if (kcEnd && loopEvent.xkey.keycode == kcEnd)
            {
               iTimeMs = iEndTimeMs - FSPLAYER_10SEC;
               libvlc_media_player_set_time(pVlcPlayer, iTimeMs);
            }
            else if (kcEsc && loopEvent.xkey.keycode == kcEsc)
            {
               iRunning = 0;
#ifdef FSPLAYER_DEBUG
               printf("Final time: %ld, IsPlaying:%d, State:%d\n",
                      libvlc_media_player_get_time(pVlcPlayer),
                      libvlc_media_player_is_playing(pVlcPlayer),
                      libvlc_media_player_get_state(pVlcPlayer));
#endif // FSPLAYER_DEBUG
            }
            else if (kcHome && loopEvent.xkey.keycode == kcHome)
            {
               iTimeMs = 0;
               libvlc_media_player_set_time(pVlcPlayer, iTimeMs);
            }
            else if (kcKpBegin && loopEvent.xkey.keycode == kcKpBegin)
               PositionWindow(pX11Display, wMaster, 5, scrx, scry);
            else if (kcKpDown && loopEvent.xkey.keycode == kcKpDown)
               PositionWindow(pX11Display, wMaster, 2, scrx, scry);
            else if (kcKpEnd && loopEvent.xkey.keycode == kcKpEnd)
               PositionWindow(pX11Display, wMaster, 1, scrx, scry);
            else if (kcKpHome && loopEvent.xkey.keycode == kcKpHome)
               PositionWindow(pX11Display, wMaster, 7, scrx, scry);
            else if (kcKpLeft && loopEvent.xkey.keycode == kcKpLeft)
               PositionWindow(pX11Display, wMaster, 4, scrx, scry);
            else if (kcKpMinus && loopEvent.xkey.keycode == kcKpMinus)
            {
               iRet = libvlc_audio_get_volume(pVlcPlayer) - 10;
               if (iRet < 0)
                  iRet = 0;
               libvlc_audio_set_volume(pVlcPlayer, iRet);
            }
            else if (kcKpMult && loopEvent.xkey.keycode == kcKpMult
                     && iNumVlcAudioTracks > 1)
            {
               iVlcAudioTrack++;
               if (iVlcAudioTrack == iNumVlcAudioTracks)
                  iVlcAudioTrack = 0;
               libvlc_audio_set_track(pVlcPlayer,
                                      pVlcAudioTrackId[iVlcAudioTrack]);
            }
            else if (kcKpPageDown && loopEvent.xkey.keycode == kcKpPageDown)
               PositionWindow(pX11Display, wMaster, 3, scrx, scry);
            else if (kcKpPageUp && loopEvent.xkey.keycode == kcKpPageUp)
               PositionWindow(pX11Display, wMaster, 9, scrx, scry);
            else if (kcKpPlus && loopEvent.xkey.keycode == kcKpPlus)
            {
               iRet = libvlc_audio_get_volume(pVlcPlayer) + 10;
               if (iRet > 100)
                  iRet = 100;
               libvlc_audio_set_volume(pVlcPlayer, iRet);
            }
            else if (kcKpRight && loopEvent.xkey.keycode == kcKpRight)
               PositionWindow(pX11Display, wMaster, 6, scrx, scry);
            else if (kcKpUp && loopEvent.xkey.keycode == kcKpUp)
               PositionWindow(pX11Display, wMaster, 8, scrx, scry);
            else if (kcLeft && loopEvent.xkey.keycode == kcLeft)
            {
               iTimeMs = libvlc_media_player_get_time(pVlcPlayer);
               if (iTimeMs <= FSPLAYER_10SEC)
                  iTimeMs = 0;
               else
                  iTimeMs -= FSPLAYER_10SEC;
               libvlc_media_player_set_time(pVlcPlayer, iTimeMs);
            }
            else if (kcPgDown && loopEvent.xkey.keycode == kcPgDown)
            {
               iTimeMs = libvlc_media_player_get_time(pVlcPlayer);
               if (iTimeMs <= FSPLAYER_10MIN)
                  iTimeMs = 0;
               else
                  iTimeMs -= FSPLAYER_10MIN;
               libvlc_media_player_set_time(pVlcPlayer, iTimeMs);
            }
            else if (kcPgUp && loopEvent.xkey.keycode == kcPgUp)
            {
               iTimeMs = libvlc_media_player_get_time(pVlcPlayer);
               if (iTimeMs < iEndTimeMs - FSPLAYER_10SEC)
               {
                  if (iTimeMs < iEndTimeMs - FSPLAYER_10MIN)
                     iTimeMs += FSPLAYER_10MIN;
                  else
                     iTimeMs = iEndTimeMs - FSPLAYER_10SEC;
                  libvlc_media_player_set_time(pVlcPlayer, iTimeMs);
               }
            }
            else if (kcRight && loopEvent.xkey.keycode == kcRight)
            {
               iTimeMs = libvlc_media_player_get_time(pVlcPlayer)
                         + FSPLAYER_10SEC;
               if (iTimeMs < iEndTimeMs)
                  libvlc_media_player_set_time(pVlcPlayer, iTimeMs);
            }
            else if (kcSpace && loopEvent.xkey.keycode == kcSpace)
               libvlc_media_player_pause(pVlcPlayer);
            else if (kcUp && loopEvent.xkey.keycode == kcUp)
            {
               iTimeMs = libvlc_media_player_get_time(pVlcPlayer);
               if (iTimeMs < iEndTimeMs - FSPLAYER_10SEC)
               {
                  if (iTimeMs < iEndTimeMs - FSPLAYER_1MIN)
                     iTimeMs += FSPLAYER_1MIN;
                  else
                     iTimeMs = iEndTimeMs - FSPLAYER_10SEC;
                  libvlc_media_player_set_time(pVlcPlayer, iTimeMs);
               }
            }

#ifdef FSPLAYER_DEBUG
            else
               printf("KeySym=0x%lX\n", XKeycodeToKeysym(pX11Display,
                                          loopEvent.xkey.keycode, 0));
#endif // FSPLAYER_DEBUG
         }
         else if (loopEvent.type == ButtonRelease)
         {
            XRaiseWindow(pX11Display, wMaster);
            XSetInputFocus(pX11Display, wInput, RevertToNone, 0);
         }
      }

      if (iRunning)
      {
         if (libvlc_media_player_get_state(pVlcPlayer) >= libvlc_Stopped)
            iRunning = 0;
      }
      if (iRunning)
      {
         iTimeMs = libvlc_media_player_get_time(pVlcPlayer);
         if (iTimeMs < 0)
         {
            iErr = ERROR_FSPLAYER_VLC;
            strcat(szErr, "libvlc_media_player_get_time() failed!");
         }
         if (!iErr)
         {
            if (iTimeMs >= iEndTimeMs)
               iRunning = 0;
         }
      }      
      if (iRunning)
      {
         XGetInputFocus(pX11Display,     &w, &iRet);
         if (w == wVlc || w == wMaster)
         {
            XRaiseWindow(pX11Display, wInputMaster);
            XRaiseWindow(pX11Display, wMaster);
            XSetInputFocus(pX11Display, wInput, RevertToNone, 0);
         }
      }
   }

   /* Stop playing */
   if (iPlay)
      libvlc_media_player_stop(pVlcPlayer);

   TaskbarRaise(pX11Display, wTaskbar);

   switch (iErr)
   {
      case ERROR_FSPLAYER_USAGE:
         printf("USAGE: fsplayer <filename>\n");
         break;

      case ERROR_FSPLAYER_X11:
         printf("X11 ERROR: %s\n", szErr);
         break;

      case ERROR_FSPLAYER_VLC:
         printf("VLC ERROR: %s\n", szErr);
         break;
   
      case ERROR_FSPLAYER_FAIL:
         printf("ERROR: LibVLC's Video Window Not Found!\n");
         break;
   
      case ERROR_FSPLAYER_MEM:
         printf("ERROR: Out Of Memory!\n");
         break;
   
      case ERROR_FSPLAYER_VLCRUN:
         printf("ERROR: VLC Already Running!\n");
         break;
   }
 
   if (pVlcPlayer)
   {
#ifdef FSPLAYER_DEBUG
      printf("libvlc_media_player_release()\n");
#endif // FSPLAYER_DEBUG
      libvlc_media_player_release(pVlcPlayer);
   }

   if (pVlcInst)
   {
#ifdef FSPLAYER_DEBUG
      printf("libvlc_release()\n");
#endif // FSPLAYER_DEBUG
      libvlc_release(pVlcInst);
   }

   if (pVlcAudioTrackId)
      free(pVlcAudioTrackId);

   if (wInput)
   {
#ifdef FSPLAYER_DEBUG
      printf("XDestroyWindow(0x%lX)\n", wInput);
#endif // FSPLAYER_DEBUG
      XDestroyWindow(pX11Display, wInput);
   }

   if (pX11Display)
   {
#ifdef FSPLAYER_DEBUG
      printf("XCloseDisplay()\n");
#endif // FSPLAYER_DEBUG
      XCloseDisplay(pX11Display);
   }

   return(0);
}
