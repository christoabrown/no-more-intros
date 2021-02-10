#ifndef FFMPEG_H
#define FFMPEG_H

#include <cstdint>
#include "videolistitem.h"

int decode_audio_file(const char* path, const int sample_rate, float** data, int* size, double start, double duration);
int get_video_frames(Image **images, const char* path, double start, double end, int count, int height);

#endif // FFMPEG_H
