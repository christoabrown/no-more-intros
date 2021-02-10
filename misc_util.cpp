#define CUTE_FILES_IMPLEMENTATION
#include "misc_util.h"
#include "cute_files.h"
#include <vector>
#include <QTime>

std::vector<cf_file_t> get_files_in_directory(const char* path) {
    std::vector<cf_file_t> result;
    cf_dir_t dir;
    cf_dir_open(&dir, path);

    while (dir.has_next)
    {
        cf_file_t file;
        cf_read_file(&dir, &file);
        printf("%s %d %d\n", file.path, file.is_dir, file.is_reg);
        if (!file.is_dir && file.is_reg) {
            result.push_back(file);
        }
        cf_dir_next(&dir);
    }

    cf_dir_close(&dir);

    return result;
}

int qTimeToSeconds(const QTime &time)
{
    const int hours = time.hour();
    const int minutes = time.minute();
    const int seconds = time.second();

    return (hours * 3600) + (minutes * 60) + seconds;
}

QTime qTimeFromSeconds(const int seconds)
{
    const int hours = seconds / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int remSeconds = (seconds % 3600) % 60;

    QTime qTime(hours, minutes, remSeconds);
    return qTime;
}
