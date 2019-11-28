/*
 * File:        fsplayer  (main.cpp)
 *
 * Author:      fossette
 *
 * Date:        2019/11/27
 *
 * Version:     1.1
 *
 * Description: Play a video file using the VLC library in fullscreen mode.
 *              If the video size is bigger than the screen, it is shrunk
 *              to the screen size.  If the video size is smaller than the
 *              screen, the video is played as is surrounded by a black
 *              background.  Built with C++ Builder 10.3 and tested under
 *              Windows 7.
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

 //---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "main.h"
#include <vlc/vlc.h>


/*
 *  Constants
 */

#define ERROR_FSPLAYER_USAGE           1
#define ERROR_FSPLAYER_MEM             2
#define ERROR_FSPLAYER_VLC             3

#define FSPLAYER_10SEC                 10000
#define FSPLAYER_1MIN                  60000
#define FSPLAYER_10MIN                 600000

#define FSPLAYER_STATE_PLAYING         1

#define LNSZ                           200


//---------------------------------------------------------------------------

#pragma package(smart_init)
#pragma resource "*.dfm"
TrfMain *rfMain;

/*
 *  Global variables
 */

int                     giErr = 0,
                        giNumVlcAudioTracks = 0,
                        giState = 0,
                        giVlcAudioTrack = 0,
                        *gpVlcAudioTrackId = NULL;
char                    gszErr[LNSZ];
libvlc_instance_t       *gpVlcInst = NULL;
libvlc_media_player_t   *gpVlcPlayer = NULL;
libvlc_time_t           giEndTimeMs = 0;

//---------------------------------------------------------------------------

__fastcall TrfMain::TrfMain(TComponent* Owner)
   : TForm(Owner)
{
   *gszErr = 0;
}

//---------------------------------------------------------------------------

void __fastcall TrfMain::FormCreate(TObject *Sender)
{
   int            i,
                  iScrX = 0,
                  iScrY = 0;
   unsigned int   iVidX,
                  iVidY;
   char           sz[LNSZ];
   libvlc_media_t *pVlcMedia;
   libvlc_track_description_t *pVlcAudioTrackDesc, *pVlcATD;


   // Init
   *sz = 0;
   iScrX = GetSystemMetrics(SM_CXSCREEN);
   iScrY = GetSystemMetrics(SM_CYSCREEN);

   if (ParamCount() == 1)
      if (FileExists(ParamStr(1), true) && ParamStr(1).Length() < LNSZ)
          strcpy(sz, AnsiString(ParamStr(1)).c_str());

   if (!(*sz))
      if (rfOpen->Execute())
         strcpy(sz, AnsiString(rfOpen->FileName).c_str());

   if (!(*sz))
      giErr = ERROR_FSPLAYER_USAGE;

   if (!giErr)
   {
      // Load the VLC engine
      gpVlcInst = libvlc_new(0, NULL);
      if (!gpVlcInst)
      {
         giErr = ERROR_FSPLAYER_VLC;
         strcat(gszErr, "libvlc_new() failed!");
      }
   }
   if (!giErr)
   {
      // Create a new item
      pVlcMedia = libvlc_media_new_path(gpVlcInst, sz);
      if (pVlcMedia)
      {
         // Create a media player playing environement
         gpVlcPlayer = libvlc_media_player_new_from_media(pVlcMedia);
         if (!gpVlcPlayer)
         {
            giErr = ERROR_FSPLAYER_VLC;
            strcat(gszErr, "libvlc_media_player_new_from_media() failed!");
         }

         // No need to keep the media now
         libvlc_media_release(pVlcMedia);
      }
      else
      {
         giErr = ERROR_FSPLAYER_VLC;
         strcat(gszErr, "libvlc_media_new_path() failed!");
      }
   }
   if (!giErr)
   {
      libvlc_media_player_set_hwnd(gpVlcPlayer, rrView->Handle);
      libvlc_video_set_key_input(gpVlcPlayer, 0);
      libvlc_media_player_play(gpVlcPlayer);
      libvlc_media_player_set_pause(gpVlcPlayer, 1);
      Sleep(1000);  // Wait for the video to be loaded in memory
      libvlc_video_get_size(gpVlcPlayer, 0,     &iVidX, &iVidY);
      giEndTimeMs = libvlc_media_player_get_length(gpVlcPlayer);
      if (giEndTimeMs <= 0)
      {
         giErr = ERROR_FSPLAYER_VLC;
         strcat(gszErr, "libvlc_media_player_get_length() failed!");
      }
   }
   if (!giErr)
   {
      giNumVlcAudioTracks = libvlc_audio_get_track_count(gpVlcPlayer);
      if (giNumVlcAudioTracks > 1)
      {
         gpVlcAudioTrackId = (int *)malloc(giNumVlcAudioTracks * sizeof(int));
         if (gpVlcAudioTrackId)
         {
            memset(gpVlcAudioTrackId, 0,
                   sizeof(giNumVlcAudioTracks * sizeof(int)));
            pVlcAudioTrackDesc = pVlcATD =
               libvlc_audio_get_track_description(gpVlcPlayer);
            if (pVlcAudioTrackDesc)
            {
               for (i = 0 ; pVlcATD && i < giNumVlcAudioTracks ; i++)
               {
                  gpVlcAudioTrackId[i] = pVlcATD->i_id;
                  pVlcATD = pVlcATD->p_next;
               }
               libvlc_track_description_list_release(pVlcAudioTrackDesc);
            }
            else
               giNumVlcAudioTracks = 0;
         }
         else
            giErr = ERROR_FSPLAYER_MEM;
      }
   }
   if (!giErr)
   {
      if ((int)iVidX >= iScrX || (int)iVidY >= iScrY)
      {
         rrView->Left = 0;
         rrView->Top = 0;
         rrView->Width = iScrX;
         rrView->Height = iScrY;
      }
      else
      {
         rrView->Left = (iScrX-iVidX)/2;
         rrView->Top = (iScrY-iVidY)/2;
         rrView->Width = iVidX;
         rrView->Height = iVidY;
      }

      libvlc_media_player_set_time(gpVlcPlayer, 0);
      libvlc_media_player_play(gpVlcPlayer);
      giState = FSPLAYER_STATE_PLAYING;
   }

   if (giErr)
   {
      switch (giErr)
      {
         case ERROR_FSPLAYER_USAGE:
            strcpy(sz, "USAGE: fsplayer <filename>");
            break;

         case ERROR_FSPLAYER_MEM:
            strcpy(sz, "ERROR: Out Of Memory!");
            break;

         case ERROR_FSPLAYER_VLC:
       	   sprintf(sz, "VLC ERROR: %s", gszErr);
            break;

         default:
       	   sprintf(sz, "ERROR # %d", giErr);
            break;
      }

      giState = 0;
      Hide();
      Application->MessageBox(UnicodeString(sz).w_str(), L"fsplayer", MB_OK);
      Application->Terminate();
      giErr = 0;
   }
}

//---------------------------------------------------------------------------

void __fastcall TrfMain::rrTimerTimer(TObject *Sender)
{
   libvlc_state_t stateVlcPlayer;

   static int iInTimer = 0;


   if (!iInTimer)
   {
      iInTimer = 1;

      switch (giState)
      {
         case FSPLAYER_STATE_PLAYING:
            stateVlcPlayer = libvlc_media_player_get_state(gpVlcPlayer);
            if (stateVlcPlayer == libvlc_Stopped
                || stateVlcPlayer == libvlc_Ended
                || stateVlcPlayer == libvlc_Error)
            {
               giState = 0;
               Hide();
               Application->Terminate();
            }
            break;
      }

      iInTimer = 0;
   }
}

//---------------------------------------------------------------------------

void __fastcall TrfMain::FormKeyDown(TObject *Sender, WORD &Key,
                                     TShiftState Shift)
{
   int            i;
   libvlc_time_t  iTimeMs;


   switch(Key)
   {
      case vkDown:
         iTimeMs = libvlc_media_player_get_time(gpVlcPlayer);
         if (iTimeMs <= FSPLAYER_1MIN)
            iTimeMs = 0;
         else
            iTimeMs -= FSPLAYER_1MIN;

         libvlc_media_player_set_time(gpVlcPlayer, iTimeMs);
         break;

      case vkEnd:
         iTimeMs = giEndTimeMs - FSPLAYER_10SEC;
         libvlc_media_player_set_time(gpVlcPlayer, iTimeMs);
         break;

      case vkEscape:
         giState = 0;
         Hide();
         libvlc_media_player_stop(gpVlcPlayer);
         Application->Terminate();
         break;

      case vkHome:
         libvlc_media_player_set_time(gpVlcPlayer, 0);
         break;

      case vkSubtract:
         i = libvlc_audio_get_volume(gpVlcPlayer) - 10;
         if (i < 1)
            i = 1;

         libvlc_audio_set_volume(gpVlcPlayer, i);
         break;

      case vkAdd:
         i = libvlc_audio_get_volume(gpVlcPlayer) + 10;
         if (i > 100)
            i = 100;

         libvlc_audio_set_volume(gpVlcPlayer, i);
         break;

      case vkLeft:
         iTimeMs = libvlc_media_player_get_time(gpVlcPlayer);
         if (iTimeMs <= FSPLAYER_10SEC)
            iTimeMs = 0;
         else
            iTimeMs -= FSPLAYER_10SEC;

         libvlc_media_player_set_time(gpVlcPlayer, iTimeMs);
         break;

      case vkPrior:
         iTimeMs = libvlc_media_player_get_time(gpVlcPlayer);
         if (iTimeMs < giEndTimeMs - FSPLAYER_10SEC)
         {
            if (iTimeMs < giEndTimeMs - FSPLAYER_10MIN)
               iTimeMs += FSPLAYER_10MIN;
            else
               iTimeMs = giEndTimeMs - FSPLAYER_10SEC;

            libvlc_media_player_set_time(gpVlcPlayer, iTimeMs);
         }
         break;

      case vkMultiply:
         if (giNumVlcAudioTracks > 1)
         {
            giVlcAudioTrack++;
            if (giVlcAudioTrack >= giNumVlcAudioTracks)
               giVlcAudioTrack = 0;

            libvlc_audio_set_track(gpVlcPlayer,
                                   gpVlcAudioTrackId[giVlcAudioTrack]);
         }
         break;

      case vkNext:
         iTimeMs = libvlc_media_player_get_time(gpVlcPlayer);
         if (iTimeMs <= FSPLAYER_10MIN)
            iTimeMs = 0;
         else
            iTimeMs -= FSPLAYER_10MIN;

         libvlc_media_player_set_time(gpVlcPlayer, iTimeMs);
         break;

      case vkRight:
         iTimeMs = libvlc_media_player_get_time(gpVlcPlayer) + FSPLAYER_10SEC;
         if (iTimeMs < giEndTimeMs)
            libvlc_media_player_set_time(gpVlcPlayer, iTimeMs);
         break;

      case vkSpace:
         libvlc_media_player_pause(gpVlcPlayer);
         break;

      case vkUp:
         iTimeMs = libvlc_media_player_get_time(gpVlcPlayer);
         if (iTimeMs < giEndTimeMs - FSPLAYER_10SEC)
         {
            if (iTimeMs < giEndTimeMs - FSPLAYER_1MIN)
               iTimeMs += FSPLAYER_1MIN;
            else
               iTimeMs = giEndTimeMs - FSPLAYER_10SEC;
            libvlc_media_player_set_time(gpVlcPlayer, iTimeMs);
         }
         break;
   }
}

//---------------------------------------------------------------------------

void __fastcall TrfMain::FormClose(TObject *Sender, TCloseAction &Action)
{
   if (gpVlcPlayer)
      libvlc_media_player_stop(gpVlcPlayer);
}

//---------------------------------------------------------------------------

__fastcall TrfMain::~TrfMain(void)
{
   if (gpVlcPlayer)
   {
      libvlc_media_player_release(gpVlcPlayer);
      gpVlcPlayer = NULL;
   }

   if (gpVlcInst)
   {
      libvlc_release(gpVlcInst);
      gpVlcInst = NULL;
   }
}

//---------------------------------------------------------------------------
