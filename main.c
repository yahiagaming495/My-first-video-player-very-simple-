#include <stdio.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
//#include <libavutil/imgutils.h>
//#include <libswscale/swscale.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int dcode1(AVFormatContext *inputContext);

int main(int argc, char *argv[])
{
    // Open the input file
    const char *filename = "2025-07-11 13-24-22.mkv";
    AVFormatContext *inputContext = NULL;
    int result = avformat_open_input(&inputContext, filename, NULL, NULL);

    if (result < 0)
    {
        char errbuf[128];
        av_strerror(result, errbuf, sizeof(errbuf));
        fprintf(stderr, "Could not open file '%s': %s\n", filename, errbuf);
        return 1;
    }

    printf("SUCCESS: File opened correctly!\n");
    printf("\n");

    dcode1(inputContext);

    return 0;
}

int dcode1(AVFormatContext *inputContext)
{
    // Find the best video stream
    const AVCodec *codec = NULL;
    int stream_i = av_find_best_stream(inputContext, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);

    if (stream_i < 0)
    {
        fprintf(stderr, "Could not find a video stream in the input file\n");
    }

    // Allocate codec context
    AVCodecContext *vCodecCtx = avcodec_alloc_context3(codec);

    if (!vCodecCtx)
    {
        fprintf(stderr, "Could not allocate video codec context\n");
        return 1;
    }

    // Copy codec parameters from input stream to codec context
    int parameters = avcodec_parameters_to_context(vCodecCtx, inputContext->streams[stream_i]->codecpar);

    if (parameters < 0)
    {
        fprintf(stderr, "Could not copy codec parameters to context\n");
        avcodec_free_context(&vCodecCtx);
        return 1;
    }

    // Open the codec
    int open2 = avcodec_open2(vCodecCtx, codec, NULL);

    if (open2 < 0)
    {
        fprintf(stderr, "Could not open codec\n");
        avcodec_free_context(&vCodecCtx);
        return 1;
    }
    printf("Video Dimensions: %d x %d\n", vCodecCtx->width, vCodecCtx->height);

    printf("Success! Decoder %s is ready to work.\n", codec->name);

    // Allocate packet and frame
    AVPacket *packet = av_packet_alloc();
    if (!packet)
    {
        fprintf(stderr, "Could not allocate AVPacket\n");
        avcodec_free_context(&vCodecCtx);
        return 1;
    }

    AVFrame *frame = av_frame_alloc();
    if (!frame)
    {
        fprintf(stderr, "Could not allocate AVFrame\n");
        av_packet_free(&packet);
        avcodec_free_context(&vCodecCtx);
        return 1;
    }

    /*AVFrame *rgb = av_frame_alloc();
    if (!rgb)
    {
        fprintf(stderr, "Could not allocate RGB AVFrame\n");
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&vCodecCtx);
        return 1;
    }

    struct SwsContext *swsWctx = sws_getContext(
                    vCodecCtx->width,
                    vCodecCtx->height,
                    vCodecCtx->pix_fmt,
                    vCodecCtx->width,
                    vCodecCtx->height,
                    AV_PIX_FMT_RGB24,
                    SWS_BILINEAR,
                    NULL,
                    NULL,
                           NULL
                    );

    if (swsWctx == NULL) {
        printf("Could not allocate the SWS context\n");
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&vCodecCtx);
    }

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, vCodecCtx->width, vCodecCtx->height, 1);
    uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(rgb->data, rgb->linesize, buffer, AV_PIX_FMT_RGB24, vCodecCtx->width, vCodecCtx->height, 1);
    */

    // sdl

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("video_player",
                                       vCodecCtx->width,
                                        vCodecCtx->height,
                                     SDL_WINDOW_BORDERLESS);


    SDL_Renderer *renderer =  SDL_CreateRenderer(window, NULL);

    if (!renderer)
    {
        fprintf(stderr, "Could not create renderer - %s\n", SDL_GetError());
        return 1;
    }

    SDL_Texture *texture =  SDL_CreateTexture(renderer,
                                              SDL_PIXELFORMAT_IYUV,
                                              SDL_TEXTUREACCESS_STREAMING,
                                              vCodecCtx->width,
                                              vCodecCtx->height);

    SDL_Event event;
    // Read frames from the file
    while (av_read_frame(inputContext, packet) >= 0)
    {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                // Exit decoding early
                av_packet_unref(packet);
                goto cleanup;
            }
        }

        if (packet->stream_index == stream_i) {
            int send = avcodec_send_packet(vCodecCtx, packet);

            if (send < 0)
            {
                fprintf(stderr, "Error sending packet for decoding\n");
                break;
            }

            while(1)
            {
                int receive = avcodec_receive_frame(vCodecCtx, frame);

                if (receive == AVERROR(EAGAIN) || receive == AVERROR_EOF)
                {
                    // Need more packets or end of file
                    break;
                }
                else if (receive < 0)
                {
                    fprintf(stderr, "Error during decoding\n");
                    break;
                }

                if (receive == 0) {
                    /*sws_scale(
                             swsWctx,
                             (const uint8_t *const *) frame->data,
                             frame->linesize, 0,
                     vCodecCtx->height,
                             rgb->data,
                     rgb->linesize
                    );*/

                    // rendering
                    SDL_UpdateYUVTexture(texture, NULL,
                        frame->data[0], frame->linesize[0],
                        frame->data[1], frame->linesize[1],
                        frame->data[2], frame->linesize[2]);

                    SDL_RenderClear(renderer);

                    SDL_RenderTexture(renderer,
                                      texture,
                                      NULL, NULL);



                    SDL_RenderPresent(renderer);

                    SDL_Delay(30); // the speed at which the video runs
                }

            }
        }
        av_packet_unref(packet);
    }

    cleanup:
     av_packet_free(&packet);
     avcodec_free_context(&vCodecCtx);
     avformat_close_input(&inputContext);
     av_frame_free(&frame);
     SDL_DestroyTexture(texture);
     SDL_DestroyRenderer(renderer);
     SDL_DestroyWindow(window);

    return 0;
}






/*int stream(AVFormatContext *inputContext, const char *filename)
{
    // Retrieve stream information
    int deepS = avformat_find_stream_info(inputContext, NULL);

    if (deepS < 0)
    {
        fprintf(stderr, "Could not find stream information in file '%s'\n", filename);
        avformat_close_input(&inputContext);
        return 1;
    }

    for (int i = 0; i < inputContext->nb_streams; i++)
    {
        AVStream *stream = inputContext->streams[i];
        AVCodecParameters *codecpar = stream->codecpar;

        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            printf("Stream %d, Resolution: %dx%d, Codec ID: %d\n",
                   i, codecpar->width, codecpar->height, codecpar->codec_id);
        }
        else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            printf("Stream %d AUDIO, Codec ID: %d\n",
                   i, codecpar->codec_id);
        }
    }

    return 1;
}


int av_find_best_S(AVFormatContext *inputContext)
{
    const AVCodec *codec = NULL;
    int stream_i = -1;

    stream_i = av_find_best_stream(inputContext, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);

    if (stream_i < 0)
    {
        fprintf(stderr, "Could not find a video stream in the input file\n");
    }

    printf("Video stream index: %d, Codec name: %s\n", stream_i, codec->name);

    return 0;
}
*/