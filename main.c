#include <stdio.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int videoDcode(AVFormatContext *inputContext);
int  dcodingLoop(AVFormatContext *inputContext,
                 AVCodecContext *vCodecCtx, int stream_i,
                 AVCodecContext *audio, int audiostream);

int main(int argc, char *argv[])
{
    // Open the input file
    const char *filename = "s.mov";
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

    videoDcode(inputContext);

    return 0;
}

int videoDcode(AVFormatContext *inputContext)
{
    //                                     Video Decoder

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
        goto error;
    }

    // Open the codec
    int open2 = avcodec_open2(vCodecCtx, codec, NULL);

    printf("%i", vCodecCtx->sample_rate);
    if (open2 < 0)
    {
        fprintf(stderr, "Could not open codec\n");
        goto error;
    }
    printf("Video Dimensions: %d x %d\n", vCodecCtx->width, vCodecCtx->height);

    printf("Success! Decoder %s is ready to work.\n", codec->name);


    //                                          Audio Decoder


    const AVCodec *codec1 = NULL;
    int audiostream = av_find_best_stream(inputContext, AVMEDIA_TYPE_AUDIO, -1, -1, &codec1, 0);

    if (audiostream < 0)
    {
        fprintf(stderr, "Could not find a audio stream in the input file\n");
        goto error;
    }

    AVCodecContext *audio = avcodec_alloc_context3(codec1);

    if (!audio)
    {
       fprintf(stderr, "Could not allocate audio codec context\n");
       goto error;
    }

    int parameters1 = avcodec_parameters_to_context(audio, inputContext->streams[audiostream]->codecpar);

    if (parameters1 < 0) {
        fprintf(stderr, "Could not copy codec parameters to context\n");
        avcodec_free_context(&audio);
        goto error;
    }

    int open2Audio = avcodec_open2(audio, codec1, NULL);

    if (open2Audio < 0)
    {
        fprintf(stderr, "Could not open codec\n");
        avcodec_free_context(&audio);
        goto error;

    }

    dcodingLoop(inputContext, vCodecCtx, stream_i, audio, audiostream);

    avcodec_free_context(&vCodecCtx);
    avformat_free_context(inputContext);
    avcodec_free_context(&audio);
    return 0;

    error:
      avcodec_free_context(&vCodecCtx);
      avformat_free_context(inputContext);
      return 1;
}

int dcodingLoop(AVFormatContext *inputContext, AVCodecContext *vCodecCtx, int stream_i, AVCodecContext *audio, int audiostream)
{
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

    // Audio

    //AVCodecParameters *in_para = inputContext->streams[audiostream]->codecpar;

    //in

    enum AVSampleFormat in_f = audio->sample_fmt;
    int in_sample_rate = audio->sample_rate;
    AVChannelLayout in_ch_layout = audio->ch_layout;

    // out

    enum AVSampleFormat out_f = AV_SAMPLE_FMT_S16;
    int out_sample_rate = 44100;
    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, 2);

    SwrContext *swr_ctx = NULL;

    swr_alloc_set_opts2(&swr_ctx, &out_ch_layout,
                         out_f,
                         out_sample_rate,
                         &in_ch_layout,
                         in_f,
                         in_sample_rate,
                         0, NULL);

    int init = swr_init(swr_ctx);

    if (init < 0)
    {
        fprintf(stderr, "Could not initialize swr\n");
        swr_free(&swr_ctx);
        av_packet_free(&packet);
        av_frame_free(&frame);
        return 1;
    }

    uint8_t *out_buffer = NULL;
    int max_out_samples = 4096;
    int out_buffer0 = av_samples_alloc(&out_buffer, NULL, out_ch_layout.nb_channels, max_out_samples, out_f, 0);

    if (out_buffer0 < 0)
    {
        fprintf(stderr, "Could not allocate %d bytes of memory\n", out_buffer0);
        swr_free(&swr_ctx);
        av_packet_free(&packet);
        av_frame_free(&frame);
        return 1;
    }

    // sdl

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        swr_free(&swr_ctx);
        av_packet_free(&packet);
        av_frame_free(&frame);
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
        swr_free(&swr_ctx);
        av_packet_free(&packet);
        av_frame_free(&frame);
        return 1;
    }

    SDL_Texture *texture =  SDL_CreateTexture(renderer,
                                              SDL_PIXELFORMAT_IYUV,
                                              SDL_TEXTUREACCESS_STREAMING,
                                              vCodecCtx->width,
                                              vCodecCtx->height);

    // sdl audio

    if (!SDL_Init(SDL_INIT_AUDIO))
    {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
    }

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);

    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = 44100;

    // 3. Create and bind the stream
    SDL_AudioStream *stream = SDL_CreateAudioStream(&spec, &spec);
    SDL_BindAudioStream(dev, stream);
    SDL_ResumeAudioDevice(dev);

    SDL_Event event;

    // time
    Uint64 start_time = SDL_GetTicks();
    double v_time_base = av_q2d(inputContext->streams[stream_i]->time_base);

    // Read frames from the file
    while (av_read_frame(inputContext, packet) >= 0)
    {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
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
                    goto cleanup;
                }


                if (receive == 0) {

                    // sync logic

                    double pts_in_seconds = frame->pts * v_time_base;
                    Uint64 target_time = (Uint64)(pts_in_seconds * 1000);

                    static int first_Frame = 1;
                    if (first_Frame)
                    {
                        start_time = SDL_GetTicks() - target_time;
                        first_Frame = 0;
                    }

                    Uint64 current_time = SDL_GetTicks();
                    Uint64 elapsed_ms = current_time - start_time;

                    if (target_time > elapsed_ms)
                    {
                        Uint64 delay = (Uint64)(target_time - elapsed_ms);
                        if (delay < 1000)
                        {
                            SDL_Delay(delay);
                        }
                    }
                    else if (elapsed_ms > target_time + 50)
                    {
                        continue;
                    }

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

                    SDL_Delay(1);
                }
            }
        }
        else if (packet->stream_index == audiostream) {

            if (avcodec_send_packet(audio, packet) < 0) {
                fprintf(stderr, "Error sending packet for decoding\n");
                goto cleanup;
            }

            while (1) {

                int receive = avcodec_receive_frame(audio, frame);

                if (receive == AVERROR(EAGAIN) || receive == AVERROR_EOF)
                {
                    // Need more packets or end of file
                    break;
                }
                else if (receive < 0)
                {
                    fprintf(stderr, "Error during decoding\n");
                    goto cleanup;
                }

                if (receive == 0) {

                    // audio logic

                    int out_samples = av_rescale_rnd( swr_get_delay(swr_ctx, frame->sample_rate) + frame->nb_samples,
                   out_sample_rate, frame->sample_rate, AV_ROUND_UP);

                    if (out_samples > max_out_samples)
                    {
                        av_freep(&out_buffer);
                        max_out_samples = out_samples;

                        if (av_samples_alloc(&out_buffer, NULL,
                                             out_ch_layout.nb_channels,
                                             max_out_samples,
                                             out_f, 0))
                        {
                            fprintf(stderr,"av_samples_alloc failed\n");
                            goto cleanup;;
                        }

                    }

                    int sample = swr_convert(swr_ctx, &out_buffer,
                                             out_samples,
                                            (const uint8_t**)frame->data,
                                     frame->nb_samples);

                    if (sample > 0)
                    {
                        int byte_size = sample * out_ch_layout.nb_channels * 2;
                        SDL_PutAudioStreamData(stream, out_buffer, byte_size);
                    }

                }
            }
        }
        av_packet_unref(packet);
    }

    av_packet_free(&packet);
    av_frame_free(&frame);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_DestroyAudioStream(stream);
    SDL_CloseAudioDevice(dev);
    av_freep(&out_buffer);
    return 0;

    cleanup:
     av_packet_free(&packet);
     av_frame_free(&frame);
     SDL_DestroyTexture(texture);
     SDL_DestroyRenderer(renderer);
     SDL_DestroyWindow(window);
     SDL_DestroyAudioStream(stream);
     SDL_CloseAudioDevice(dev);
     av_freep(&out_buffer);
     return 1;

}
