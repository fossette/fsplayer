# fsplayer
Fullscreen Video Player Using LibVLC

## Purpose

Play a video file using the VLC library in fullscreen mode.  If the video size is bigger than the screen, it is shrunk to the screen size.  If the video size is smaller than the screen, the video is played as is surrounded by a black background.

Accepted keyboard keys are:
- ESC :         Stop the video
- ARROW RIGHT : Jump forward 10 seconds
- ARROW LEFT :  Jump backward 10 seconds
- ARROW UP :    Jump forward 1 minutes
- ARROW DOWN :  Jump backward 1 minutes
- PAGE UP :     Jump forward 10 minutes
- PAGE DOWN :   Jump backward 10 minutes
- HOME :        Jump to the beginning
- END :         Jump 10 seconds before the end
- SPACE :       Pause the video

Accepted keypad keys are:
- \+ :           Increase the volume
- \- :           Decrease the volume
- \* :           Change the audio track
- 1-9 :         Move a smaller view to the specified area within the screen.

## How to build and install fsplayer
1. Download the source files and store them in a directory
2. Go to that directory in a terminal window
3. To built the executable file, type `make`
4. To install the executable file, type `make install` as a superuser.  The Makefile will copy the executable file into the
`/usr/bin` directory.  If you want it elsewhere, feel free to copy it by hand instead.

## Version history
1.0 - 2019/05/29 - Initial release
1.1 - 2019/11/27 - Win32 release

## Compatibility
**fsplayer** has been tested under FreeBSD 11.2 using libX11 1.6.7 and qvwm 1.1.12_2015.  Should be easy to port because there are only X11 dependencies.  **fsplayer** has also been built with C++ Builder 10.3 and tested under Windows 7.

## Donations
Thanks for the support!  
Bitcoin: **1JbiV7rGE5kRKcecTfPv16SXag65o8aQTe**

# Have Fun!
