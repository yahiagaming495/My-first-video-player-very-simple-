Libraries used:

SDL3:
#include <libavformat/avformat.h>,
#include <libavcodec/avcodec.h>,
#include <libavutil/avutil.h>,

FFmpeg:
#include <SDL3/SDL.h>,
#include <SDL3/SDL_main.h>,

The thing i couldn't do:
I couldn't convert YUV to RGB24 using #include <libswscale/swscale.h> libraries function.
but SDL3 automatically convert YUV to RGB.

