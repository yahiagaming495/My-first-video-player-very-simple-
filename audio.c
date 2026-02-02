/*
 * This is not connected to the main project,
 * and I created it to figure out
 * how to get audio working, but it is an
 * audio player on its own.
*/
#include <stdio.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int audioDcode(AVFormatContext *store);
int dcodeingLoop(AVCodecContext *codecContext, AVFormatContext *store, int st);

int main(int argc, char *argv[])
{
    const char *link = "Roa - Glory.mp3";
    AVFormatContext *store = NULL;

    int h = avformat_open_input(&store, link, NULL, NULL);

    if (h < 0)
    {
        fprintf(stderr,"avformat_open_input failed\n");
    }

    int f = avformat_find_stream_info(store, NULL);
    if (f < 0)
    {
        fprintf(stderr, "avformat_find_stream_info failed\n");
        return 1;
    }

    audioDcode(store);
    return 0;
}

int audioDcode(AVFormatContext *store)
{
    int audioStreamIndex = -1;

    for (int i = 0; i < store->nb_streams; i++)
    {
        if (store->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex < 0)
        {
            audioStreamIndex = i;
            //printf(("%i"), audioStreamIndex);
        }
    }

    const AVCodec *codec = avcodec_find_decoder(store->streams[audioStreamIndex]->codecpar->codec_id);

    AVCodecContext *codecContext = avcodec_alloc_context3(codec);
    if (!codecContext)
    {
        fprintf(stderr,"avcodec_alloc_context3 failed\n");

    }

    int per = avcodec_parameters_to_context(codecContext, store->streams[audioStreamIndex]->codecpar);

    if (per < 0)
    {
        fprintf(stderr,"avcodec_parameters_to_context failed\n");

    }

    int b = avcodec_open2(codecContext, codec, NULL);

    if (b < 0)
    {
        fprintf(stderr,"avcodec_open2 failed\n");
        goto errorOut;
    }

    dcodeingLoop(codecContext, store, audioStreamIndex);


    avformat_free_context(store);
    avcodec_free_context(&codecContext);
    return 0;


    errorOut:
       avformat_free_context(store);
       avcodec_free_context(&codecContext);
    return 1;


}

int dcodeingLoop(AVCodecContext *codecContext, AVFormatContext *store, int st)
{
    AVPacket *packet = av_packet_alloc();
    if (packet == NULL)
    {
        fprintf(stderr,"av_packet_alloc failed\n");
        return 1;
    }

    AVFrame *frame = av_frame_alloc();
    if (frame == NULL)
    {
        fprintf(stderr,"av_frame_alloc failed\n");
        av_packet_free(&packet);
        return 1;
    }

    AVCodecParameters *in_params = store->streams[st]->codecpar;

    // in
    enum AVSampleFormat in_fmt = in_params->format;
    int in_sample_rate = in_params->sample_rate;
    AVChannelLayout in_ch_layout = in_params->ch_layout;

    // out
    enum AVSampleFormat out_fmt = AV_SAMPLE_FMT_S16;
    int out_sample_rate = 44100;
    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, 2);

    SwrContext *swr_ctx = NULL;
    swr_alloc_set_opts2(&swr_ctx,
        &out_ch_layout,
        out_fmt,
        out_sample_rate,
        &in_ch_layout,
        in_fmt,
        in_sample_rate,
        0, NULL);

    int init = swr_init(swr_ctx);

    if (init < 0)
    {
        fprintf(stderr,"swr_init failed\n");
        goto errorOut;
    }

    printf("%d\n", init);

    uint8_t *out_buffer = NULL;
    int max_out_samples = 4096;
    int out_buffer0 =  av_samples_alloc(&out_buffer, NULL, out_ch_layout.nb_channels, max_out_samples, out_fmt, 0);

    if (out_buffer0 < 0)
    {
        fprintf(stderr,"av_samples_alloc failed\n");
        goto errorOut;
    }

    // sdl audio

    if (!SDL_Init(SDL_INIT_AUDIO))
    {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
    }

    // 1. Open the device (Default output)
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);

    // 2. Define the format you are sending FROM FFmpeg
    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_S16;      // Matches AV_SAMPLE_FMT_S16
    spec.channels = 2;                // Matches out_ch_layout.nb_channels
    spec.freq = 44100;                // Matches out_sample_rate

    // 3. Create and bind the stream
    SDL_AudioStream *stream = SDL_CreateAudioStream(&spec, &spec);
    SDL_BindAudioStream(dev, stream);
    SDL_ResumeAudioDevice(dev);

    while (av_read_frame(store, packet) >= 0)
    {

        if (packet->stream_index == st)
        {
            if (avcodec_send_packet(codecContext, packet) < 0)
            {
                fprintf(stderr,"avcodec_send_packet failed\n");
                goto errorOut;
            }

            while (1)
            {
                int receive = avcodec_receive_frame(codecContext, frame);

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


                int out_samples = av_rescale_rnd( swr_get_delay(swr_ctx, frame->sample_rate) + frame->nb_samples,
                    out_sample_rate, frame->sample_rate, AV_ROUND_UP);

                if (out_samples > max_out_samples)
                {
                    av_freep(&out_buffer);
                    max_out_samples = out_samples;

                    if (av_samples_alloc(&out_buffer, NULL,
                                         out_ch_layout.nb_channels,
                                         max_out_samples,
                                         out_fmt, 0))
                    {
                        fprintf(stderr,"av_samples_alloc failed\n");
                        goto errorOut;
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
        av_packet_unref(packet);
    }

    while (SDL_GetAudioStreamQueued(stream) > 0) {
        SDL_Delay(100);
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    swr_free(&swr_ctx);
    SDL_DestroyAudioStream(stream);
    SDL_CloseAudioDevice(dev);
    av_freep(&out_buffer);
    return 0;

    errorOut:
      swr_free(&swr_ctx);
      SDL_DestroyAudioStream(stream);
      SDL_CloseAudioDevice(dev);
      av_frame_free(&frame);
      av_packet_free(&packet);
      return 1;
}