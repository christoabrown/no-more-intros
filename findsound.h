#ifndef FINDSOUND_H
#define FINDSOUND_H
#define SAMPLE_RATE 1024
#define SOURCE_START 0
#define SOURCE_END 600
#define ACCEPTANCE_THRESHOLD 0.8

#include <QString>
#include <QObject>
#include <QRunnable>
#include "signals.h"

struct CorrelateResult {
    size_t sampleIdx;
    float value;
    float timestamp;
};

struct IntroChunkSearchResult {
    float startTime;
    float endTime;
    size_t patchStart;
    size_t patchEnd;
};

struct IntroInfo {
    float startTime;
    float endTime;
    float matchPercent;
    FloatSignal *intro = nullptr;
    float otherStartTime = 0;
    float otherEndTime = 0;
};

struct FileSignal {
    FloatSignal* signal;
    QString file;
};

Q_DECLARE_METATYPE(FileSignal);

struct FindSoundResult {
    QString file;
    size_t index;
    IntroInfo introInfo;
    bool isProgress;
    bool isBetter;
    bool isSourceOfIntro;
};

Q_DECLARE_METATYPE(FindSoundResult);

class LoadSoundDataTask : public QObject, public QRunnable
{
    Q_OBJECT
public:
    QString path;
    void run() override;
signals:
    void sendSoundData(FileSignal fileSignal);
};

class FindSoundTask : public QObject, public QRunnable
{
    Q_OBJECT
public:
    std::vector<FileSignal> fileSignals;
    void run() override;
signals:
    void sendFindResult(FindSoundResult findSoundResult);
};


class FindSound : public QObject
{
    Q_OBJECT
public:
    FindSound();
    ~FindSound();

    void addFiles(std::vector<QString> filepaths);
    int run();
    static FloatSignal* getWavData(const char* path, double start, double duration);
    static IntroInfo getIntroFromPair(FloatSignal* one, FloatSignal* two);
    static FloatSignal* signalSlice(FloatSignal* signal, float start, float end);
    static CorrelateResult howCloseAreSignals(FloatSignal* one, FloatSignal* two);
    static CorrelateResult bestPatchPosition(FloatSignal* source, FloatSignal* patch);
    static int nextBestIntro(const std::vector<FileSignal> &fileSignals, IntroInfo *result, int start);
private:
    std::vector<QString> filepaths;
    std::vector<FileSignal> fileSignals;

    static IntroChunkSearchResult doChunkScan(FloatSignal* one, FloatSignal* two, size_t patchStart, size_t patchEnd, int patchDuration);
    static IntroChunkSearchResult getChunkSearchResults(std::vector<CorrelateResult>& sound_find_results, int patch_duration);
private slots:
    void receiveSoundData(FileSignal fileSignal);
    void receiveFindSoundResult(FindSoundResult findSoundResult);
signals:
    void sendProgress();
    void sendFindSoundResult(FindSoundResult findSoundResult);
};

#endif // FINDSOUND_H
