#ifndef MISC_UTIL_H
#define MISC_UTIL_H

#include "cute_files.h"
#include <vector>
#include <QTime>

std::vector<cf_file_t> get_files_in_directory(const char* path);
int qTimeToSeconds(const QTime &time);
QTime qTimeFromSeconds(const int seconds);

#endif // MISC_UTIL_H
