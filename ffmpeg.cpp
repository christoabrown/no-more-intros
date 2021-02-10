#include "ffmpeg.h"
extern "C"
{
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}
#include <cmath>
#include <iostream>
#include "videolistitem.h"

int decode_audio_file(const char* path, const int sample_rate, float** data, int* size, double start, double duration) {
    // get format from audio file
    AVFormatContext* format = avformat_alloc_context();
    if (avformat_open_input(&format, path, NULL, NULL) != 0) {
        fprintf(stderr, "Could not open file '%s'\n", path);
        return -1;
    }
    if (avformat_find_stream_info(format, NULL) < 0) {
        fprintf(stderr, "Could not retrieve stream info from file '%s'\n", path);
        return -1;
    }

    // Find the stream and its codec
    AVCodec* audio_codec;
    int stream_index = av_find_best_stream(
        format,              // The media stream
        AVMEDIA_TYPE_AUDIO,   // The type of stream we are looking for - audio for example
        -1,                   // Desired stream number, -1 for any
        -1,                   // Number of related stream, -1 for none
        &audio_codec,          // Gets the codec associated with the stream, can be NULL
        0                     // Flags - not used currently
                                           );

    if (stream_index == AVERROR_STREAM_NOT_FOUND || !audio_codec) {
        fprintf(stderr, "Could not retrieve audio stream from file '%s'\n", path);
        return -1;
    }

    // Get the codec context
    AVCodecContext* codec_context = avcodec_alloc_context3(audio_codec);
    if (!codec_context) {
        fprintf(stderr, "Failed to alloc codec context for stream #%u in file '%s'\n", stream_index, path);
        avformat_close_input(&format);
        return -1;
    }

    // Set the parameters of the codec context from the stream
    int result = avcodec_parameters_to_context(
        codec_context,
        format->streams[stream_index]->codecpar
                                               );
    if (result < 0) {
        fprintf(stderr, "Failed to make codec context from paramers for stream #%u in file '%s'\n", stream_index, path);
        avformat_close_input(&format);
        avcodec_free_context(&codec_context);
        return -1;
    }

    if (avcodec_open2(codec_context, audio_codec, NULL) < 0) {
        fprintf(stderr, "Failed to open decoder for stream #%u in file '%s'\n", stream_index, path);
        avformat_close_input(&format);
        avcodec_free_context(&codec_context);
        return -1;
    }

    // prepare resampler
    struct SwrContext* swr = swr_alloc();
    av_opt_set_int(swr, "in_channel_count", format->streams[stream_index]->codecpar->channels, 0);
    av_opt_set_int(swr, "out_channel_count", 1, 0);
    av_opt_set_channel_layout(swr, "in_channel_layout", format->streams[stream_index]->codecpar->channel_layout, 0);
    av_opt_set_channel_layout(swr, "out_channel_layout", AV_CH_LAYOUT_MONO, 0);
    av_opt_set_int(swr, "in_sample_rate", format->streams[stream_index]->codecpar->sample_rate, 0);
    av_opt_set_int(swr, "out_sample_rate", sample_rate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt", codec_context->sample_fmt, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
    swr_init(swr);
    if (!swr_is_initialized(swr)) {
        fprintf(stderr, "Resampler has not been properly initialized\n");
        return -1;
    }

    // prepare to read data
    AVPacket packet;
    av_init_packet(&packet);
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Error allocating the frame\n");
        return -1;
    }
    packet.data = NULL;
    packet.size = 0;

    // iterate through frames
    size_t capacity = sample_rate * ((size_t)duration + 1);
    *data = (float *)malloc(sizeof(float) * capacity);
    *size = 0;
    int seek_ts = (int)std::round(start * format->streams[stream_index]->time_base.den / format->streams[stream_index]->time_base.num);
    av_seek_frame(format, stream_index, seek_ts, AVSEEK_FLAG_ANY);

    while (av_read_frame(format, &packet) >= 0) {
        if (packet.stream_index != stream_index) {
            av_packet_unref(&packet);
            continue;
        }
        double t = (double)packet.pts * format->streams[stream_index]->time_base.num / format->streams[stream_index]->time_base.den;
        // fprintf(stdout, "Time: %f\n", t);
        if (t > start + duration - 1) {
            break;
        }

        // decode one frame
        int ret = avcodec_send_packet(codec_context, &packet);
        if (ret < 0) {
            av_packet_unref(&packet);
            fprintf(stderr, "Failed to send packet for stream #%u in file '%s'\n", stream_index, path);
            break;
        }

        ret = avcodec_receive_frame(codec_context, frame);
        if (ret != 0) {
            av_packet_unref(&packet);
            break;
        }
        // resample frames
        double* buffer;
        av_samples_alloc((uint8_t**)&buffer, NULL, 1, frame->nb_samples, AV_SAMPLE_FMT_FLT, 0);
        int frame_count = swr_convert(swr, (uint8_t**)&buffer, frame->nb_samples, (const uint8_t**)frame->data, frame->nb_samples);
        if (frame_count < 0) {
            av_freep(&buffer);
            av_packet_unref(&packet);
            fprintf(stderr, "Failed to convert #%u in file '%s'\n", stream_index, path);
            break;
        }
        // append resampled frames to data
        memcpy(*data + *size, buffer, frame_count * sizeof(float));
        *size += frame_count;
        av_freep(&buffer);
        av_packet_unref(&packet);
    }
    if (*size > 0) {
        float *new_data = (float*)realloc(*data, sizeof(float) * (*size));
        if (new_data == NULL) {
            free(*data);
            *data = NULL;
            size = 0;
        }
        else {
            *data = new_data;
        }
    }

    // clean up
    avformat_close_input(&format);
    av_frame_free(&frame);
    swr_free(&swr);
    avcodec_close(codec_context);
    avcodec_free_context(&codec_context);
    avformat_free_context(format);

    // success
    return 0;
}

AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts)
{
    const AVPixelFormat *p;
    AVPixelFormat *opaque = (AVPixelFormat*)ctx->opaque;
    AVPixelFormat hw_pix_fmt = *opaque;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }

    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}


int get_video_frames(Image **images, const char* path, double start, double end, int count, int height) {
    /*enum AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
    while((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) {
        std::cout << av_hwdevice_get_type_name(type) << std::endl;
    }*/

    AVFormatContext* format = avformat_alloc_context();
    if (avformat_open_input(&format, path, NULL, NULL) != 0) {
        fprintf(stderr, "Could not open file '%s'\n", path);
        return -1;
    }
    if (avformat_find_stream_info(format, NULL) < 0) {
        fprintf(stderr, "Could not retrieve stream info from file '%s'\n", path);
        return -1;
    }

    // Find the stream and its codec
    AVCodec* video_codec;
    int stream_index = av_find_best_stream(
        format,              // The media stream
        AVMEDIA_TYPE_VIDEO,   // The type of stream we are looking for - audio for example
        -1,                   // Desired stream number, -1 for any
        -1,                   // Number of related stream, -1 for none
        &video_codec,          // Gets the codec associated with the stream, can be NULL
        0                     // Flags - not used currently
                                           );

    if (stream_index == AVERROR_STREAM_NOT_FOUND || !video_codec) {
        fprintf(stderr, "Could not retrieve video stream from file '%s'\n", path);
        return -1;
    }

    const AVCodecHWConfig *hwconfig;
    for (int i = 0;; ++i) {
        hwconfig = avcodec_get_hw_config(video_codec, i);
        if (!hwconfig) {
            break;
        }
        if (!(hwconfig->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
            continue;
        }
        std::cout << "Chosen hw type: " << av_hwdevice_get_type_name(hwconfig->device_type) << std::endl;
        break;
    }

    // Get the codec context
    AVCodecContext* codec_context = avcodec_alloc_context3(video_codec);
    if (!codec_context) {
        fprintf(stderr, "Failed to alloc codec context for stream #%u in file '%s'\n", stream_index, path);
        avformat_close_input(&format);
        return -1;
    }
    AVPixelFormat hwpixformat = hwconfig->pix_fmt;
    codec_context->opaque = malloc(sizeof(AVPixelFormat));
    AVPixelFormat *opaque = (AVPixelFormat*)codec_context->opaque;
    *opaque = hwpixformat;

    // Set the parameters of the codec context from the stream
    int result = avcodec_parameters_to_context(
        codec_context,
        format->streams[stream_index]->codecpar
                                               );
    if (result < 0) {
        fprintf(stderr, "Failed to make codec context from paramers for stream #%u in file '%s'\n", stream_index, path);
        avformat_close_input(&format);
        avcodec_free_context(&codec_context);
        return -1;
    }

    codec_context->get_format = get_hw_format;

    AVBufferRef *hw_device_ctx = NULL;
    int hw_init_err = av_hwdevice_ctx_create(&hw_device_ctx, hwconfig->device_type, NULL, NULL, 0);
    if (hw_init_err < 0) {
        fprintf(stderr, "Failed to create hwdevice context for stream #%u in file '%s'\n", stream_index, path);
        return -1;
    }

    codec_context->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    if (avcodec_open2(codec_context, video_codec, NULL) < 0) {
        fprintf(stderr, "Failed to open decoder for stream #%u in file '%s'\n", stream_index, path);
        av_buffer_unref(&hw_device_ctx);
        avformat_close_input(&format);
        avcodec_free_context(&codec_context);
        return -1;
    }

    // prepare resampler
    const float aspect_ratio = (float)format->streams[stream_index]->codecpar->width / format->streams[stream_index]->codecpar->height;
    const int dst_height = height;
    const int dst_width = height * aspect_ratio;
    const AVPixelFormat dst_format = AV_PIX_FMT_RGB24;
    SwsContext *sws = NULL;

    // prepare to read data
    AVPacket packet;
    av_init_packet(&packet);
    AVFrame* frame = av_frame_alloc();
    AVFrame* swframe = av_frame_alloc();
    if (!frame || !swframe) {
        fprintf(stderr, "Error allocating the frame\n");
        avformat_close_input(&format);
        avcodec_free_context(&codec_context);
        sws_freeContext(sws);
        return -1;
    }
    packet.data = NULL;
    packet.size = 0;

    // allocate frame for output
    AVFrame *output_frame = av_frame_alloc();
    output_frame->format = dst_format;
    output_frame->width = dst_width;
    output_frame->height = dst_height;
    int output_frame_buffer = av_frame_get_buffer(output_frame, 0);
    if (output_frame_buffer < 0) {
        fprintf(stderr, "Failed allocate frame buffer for stream #%u in file '%s'\n", stream_index, path);
        char *err = (char *)malloc(sizeof(char) * 500);
        av_strerror(output_frame_buffer, err, 500);
        std::cout << err << std::endl;
        free(err);
        avformat_close_input(&format);
        avcodec_free_context(&codec_context);
        sws_freeContext(sws);
        return -1;
    }

    const int output_size = sizeof(uint8_t) * 3 * dst_width * dst_height;
    int seek_ts = (int)std::round(start * format->streams[stream_index]->time_base.den / format->streams[stream_index]->time_base.num);
    av_seek_frame(format, stream_index, seek_ts, AVSEEK_FLAG_BACKWARD);
    *images = (Image*)malloc(sizeof(Image) * count);
    int current_idx = 0;
    float next_time = start;
    float delta_time = (end - start)/double(count - 1);
    while (av_read_frame(format, &packet) >= 0) {
        if (packet.stream_index != stream_index) {
            av_packet_unref(&packet);
            continue;
        }

        // decode one frame
        int ret = avcodec_send_packet(codec_context, &packet);
        if (ret < 0) {
            av_packet_unref(&packet);
            fprintf(stderr, "Failed to send packet for stream #%u in file '%s'\n", stream_index, path);
            break;
        }

        ret = avcodec_receive_frame(codec_context, frame);

        if (ret == AVERROR(EAGAIN)) {
            av_packet_unref(&packet);
            continue;
        }
        if (ret != 0) {
            char *err = (char *)malloc(sizeof(char) * 500);
            av_strerror(output_frame_buffer, err, 500);
            std::cout << err << std::endl;
            free(err);
            av_packet_unref(&packet);
            break;
        }
        double t = (double)frame->pts * format->streams[stream_index]->time_base.num / format->streams[stream_index]->time_base.den;
        if (t < next_time) {
            av_packet_unref(&packet);
            continue;
        }

        if (frame->format == hwconfig->pix_fmt) {
            ret = av_hwframe_transfer_data(swframe, frame, 0);
            if (ret < 0) {
                av_packet_unref(&packet);
                fprintf(stderr, "Error transferring date to system memory\n");
                break;
            }
        }

        sws = sws_getCachedContext(
                    sws,
                    format->streams[stream_index]->codecpar->width,
                    format->streams[stream_index]->codecpar->height,
                    AVPixelFormat(swframe->format),
                    /*AVPixelFormat(format->streams[stream_index]->codecpar->format) hwconfig->pix_fmt*/
                    dst_width,
                    dst_height,
                    dst_format,
                    SWS_BILINEAR,
                    NULL,
                    NULL,
                    NULL
                    );
        if (sws == NULL) {
            fprintf(stderr, "Could not create sws context\n");
            av_packet_unref(&packet);
            break;
        }

        sws_scale(sws, swframe->data, swframe->linesize, 0, swframe->height, output_frame->data, output_frame->linesize);
        const int linesize = output_frame->linesize[0];

        uint8_t *data = (uint8_t*)malloc(output_size);
        uint8_t *p = data;
        for (int y = 0; y < dst_height; ++y) {
            memcpy(p, output_frame->data[0] + (y*linesize), dst_width*3);
            p += dst_width*3;
        }
        av_packet_unref(&packet);
        (*images)[current_idx] = {
            data,
            output_size,
            dst_width,
            dst_height,
            current_idx
        };
        current_idx++;
        if (current_idx >= count) {
            break;
        }

        next_time = start + current_idx * delta_time;
        seek_ts = (int)std::round(next_time * format->streams[stream_index]->time_base.den / format->streams[stream_index]->time_base.num);
        av_seek_frame(format, stream_index, seek_ts, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(codec_context);
    }

    // clean up
    av_buffer_unref(&hw_device_ctx);
    avformat_close_input(&format);
    av_frame_free(&frame);
    av_frame_free(&swframe);
    av_frame_free(&output_frame);
    sws_freeContext(sws);
    avcodec_close(codec_context);
    avcodec_free_context(&codec_context);
    avformat_free_context(format);

    // success
    return 0;
}
