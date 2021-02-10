#include "findsound.h"
#include "ffmpeg.h"
#include "execution_timer.h"
#include <QThreadPool>
#include <QObject>
#include <unordered_map>

void LoadSoundDataTask::run()
{
    QByteArray ba = this->path.toLocal8Bit();
    FloatSignal *signal = FindSound::getWavData(ba.constData(), SOURCE_START, SOURCE_END);
    FileSignal result = {
        signal,
        this->path
    };

    emit sendSoundData(result);
}

void FindSoundTask::run()
{
    std::vector<FileSignal> rest = fileSignals;
    std::unordered_map<QString, float> bestMatches;
    int badStreak = 0;
    int lastBestIntroIdx = 0;
    while (rest.size() > 1) {
        badStreak = 0;
        IntroInfo introInfo;
        lastBestIntroIdx = FindSound::nextBestIntro(rest, &introInfo, lastBestIntroIdx);
        if (lastBestIntroIdx < 0) {
            break;
        }

        FloatSignal *intro = introInfo.intro;
        rest.clear();
        for (size_t i = 0; i < fileSignals.size(); ++i) {
            FileSignal &fileSignal = fileSignals[i];
            auto bestIt = bestMatches.find(fileSignal.file);
            float bestValue = bestIt == bestMatches.end() ? 0 : bestIt->second;
            if (bestValue >= 0.9) {
                continue;
            }

            CorrelateResult find = FindSound::bestPatchPosition(
                        fileSignal.signal, intro);
            const float startTime = find.timestamp;
            const float endTime = startTime + introInfo.endTime - introInfo.startTime;
            FloatSignal *otherIntro = FindSound::signalSlice(
                        fileSignal.signal, startTime, endTime);
            CorrelateResult howClose = FindSound::howCloseAreSignals(otherIntro, intro);
            delete otherIntro;

            bool isBetter = false;
            bool isProgress = false;

            if (bestValue < howClose.value) {
                isBetter = true;
                bestMatches[fileSignal.file] = howClose.value;
            }

            if (bestValue < ACCEPTANCE_THRESHOLD && howClose.value >= ACCEPTANCE_THRESHOLD) {
                isProgress = true;
            } else if (howClose.value < ACCEPTANCE_THRESHOLD && bestValue < ACCEPTANCE_THRESHOLD) {
                rest.push_back(fileSignal);
            }

            const bool isSourceOfIntro = fileSignal.file == fileSignals[lastBestIntroIdx].file;
            const FindSoundResult result = {
                fileSignal.file,
                i,
                {
                    startTime,
                    endTime,
                    howClose.value
                },
                isProgress,
                isBetter,
                isSourceOfIntro
            };

            emit sendFindResult(result);

            if (howClose.value < 0.2 && bestValue == 0) {
                badStreak++;
            } else {
                badStreak = 0;
            }

            if (badStreak >= 5) {
                rest.clear();
                for (size_t j = 0; j < fileSignals.size(); ++j) {
                    FileSignal &fileSignal = fileSignals[j];
                    bestIt = bestMatches.find(fileSignal.file);
                    bestValue = bestIt == bestMatches.end() ? 0 : bestIt->second;

                    if (bestValue < ACCEPTANCE_THRESHOLD) {
                        rest.push_back(fileSignal);
                    }
                }
                break;
            }
        }

        delete introInfo.intro;
    }

    for (size_t i = 0; i < rest.size(); ++i) {
        FindSoundResult result;
        result.isProgress = true;
        result.isBetter = false;
        emit sendFindResult(result);
    }
}

FindSound::FindSound()
{

}

FindSound::~FindSound()
{
    for (auto &fileSignal : this->fileSignals) {
        delete fileSignal.signal;
    }
}

int FindSound::run()
{
    FindSoundTask *task = new FindSoundTask();
    task->fileSignals = fileSignals;
    QObject::connect(task, &FindSoundTask::sendFindResult, this, &FindSound::receiveFindSoundResult);
    QThreadPool::globalInstance()->start(task);

    return (int)fileSignals.size();
}

void FindSound::addFiles(std::vector<QString> filepaths)
{
    this->filepaths.insert(this->filepaths.begin(), filepaths.begin(), filepaths.end());
    this->fileSignals.resize(this->filepaths.size());
    for (auto &filepath : filepaths) {
        LoadSoundDataTask *task = new LoadSoundDataTask();
        task->path = filepath;
        QObject::connect(task, &LoadSoundDataTask::sendSoundData,
                         this, &FindSound::receiveSoundData);
        QThreadPool::globalInstance()->start(task);
    }
}

void FindSound::receiveSoundData(FileSignal fileSignal)
{
    int index = -1;
    for (size_t i = filepaths.size() - 1; i >= 0; --i) {
        if (filepaths[i] == fileSignal.file) {
            index = (int)i;
            break;
        }
    }
    assert(index >= 0);
    if (index == -1) {
        std::cerr << "Unable to find index of " << fileSignal.file.toStdString() << std::endl;
        return;
    }

    this->fileSignals[index] = fileSignal;
    emit sendProgress();
}

void FindSound::receiveFindSoundResult(FindSoundResult findSoundResult)
{
    if (findSoundResult.isProgress) {
        emit sendProgress();
    }

    if (findSoundResult.isBetter) {
        emit sendFindSoundResult(findSoundResult);
    }
}

FloatSignal* FindSound::getWavData(const char* path, double start, double duration)
{
    float* data;
    int size;

    decode_audio_file(path, SAMPLE_RATE, &data, &size, start, duration);
    FloatSignal *result = new FloatSignal(data, size);
    free(data);

    return result;
}

CorrelateResult FindSound::bestPatchPosition(FloatSignal* source, FloatSignal* patch)
{
    assert(source->getSize() >= patch->getSize());

    OverlapSaveConvolver x(*source, *patch);
    x.executeXcorr();
    FloatSignal *xcorr = x.extractResult();

    const float* data = xcorr->getData();
    float max = 0;
    size_t maxIdx = 0;
    const size_t patchSize = patch->getSize();
    const size_t xcorrSize = xcorr->getSize();
    for (size_t i = patchSize; i < xcorrSize; ++i) {
        const float f = data[i];
        if (f > max) {
            max = f;
            maxIdx = i - patchSize;
        }
    }

    CorrelateResult result = {
        maxIdx, max, (float)maxIdx / SAMPLE_RATE };
    delete xcorr;

    return result;
}

CorrelateResult FindSound::howCloseAreSignals(FloatSignal* one, FloatSignal* two)
{
    // ExecutionTimer timer("howCloseAreSignals");

    size_t size = std::min(one->getSize(), two->getSize());
    FloatSignal* a = new FloatSignal(one->getData(), size);
    FloatSignal* b = new FloatSignal(two->getData(), size);
    assert(a->getSize() == b->getSize() && a->getSize() == size);
    *a -= a->mean();
    *a /= (a->std() * size);
    *b -= b->mean();
    *b /= b->std();

    CorrelateResult resultA = bestPatchPosition(a, b);
    delete a;
    delete b;

    a = new FloatSignal(two->getData(), size);
    b = new FloatSignal(one->getData(), size);
    assert(a->getSize() == b->getSize() && a->getSize() == size);
    *a -= a->mean();
    *a /= (a->std() * size);
    *b -= b->mean();
    *b /= b->std();

    CorrelateResult resultB = bestPatchPosition(a, b);
    delete a;
    delete b;

    CorrelateResult result;
    if (resultA.value > resultB.value) {
        result = resultA;
    } else {
        result = resultB;
    }

   return result;
}

FloatSignal* FindSound::signalSlice(FloatSignal* signal, float start, float end) {
    size_t startSampleIdx = (size_t)(start * SAMPLE_RATE);
    size_t endSampleIdx = (size_t)(end * SAMPLE_RATE);
    size_t size = endSampleIdx - startSampleIdx;
    float* data = signal->getData();
    assert(signal->getSize() > startSampleIdx);
    if (signal->getSize() < startSampleIdx + size) {
        float *newData = (float*)malloc(sizeof(float) * size);
        memset(newData, 0, sizeof(float) * size);
        memcpy(newData, data + startSampleIdx, sizeof(float) * (signal->getSize() - startSampleIdx));

        FloatSignal* slice = new FloatSignal(newData, size);
        free(newData);
        return slice;
    }

    FloatSignal* slice = new FloatSignal(data + startSampleIdx, size);

    return slice;
}

IntroChunkSearchResult
FindSound::getChunkSearchResults(std::vector<CorrelateResult>& soundFindResults, int patchDuration) {
    // ExecutionTimer timer("getChunkSearchResults");
    float valueSum = 0;
    for (auto& soundFindResult : soundFindResults) {
        valueSum += soundFindResult.value;
    }

    const float valueMean = valueSum / soundFindResults.size();
    std::vector<IntroChunkSearchResult> contiguousBlocks;
    size_t currentBlockIdx = 0;
    CorrelateResult first = soundFindResults[0];
    contiguousBlocks.push_back({ first.timestamp, first.timestamp, 0, 0});
    // NOTE: the patch start and end time only give the time from the start time of the
    // results. So if the first patch chunk started at time=0 this will be correct, otherwise
    // it needs to be adjusted to the start time of the first patch chunk.
    for (size_t i = 0; i < soundFindResults.size(); ++i) {
        std::cout << soundFindResults[i].value << ", " << soundFindResults[i].timestamp << std::endl;
        if (soundFindResults[i].value < valueMean) {
            continue;
        }
        float timespan = fabs(soundFindResults[i].timestamp -
                              contiguousBlocks[currentBlockIdx].endTime);
        if (timespan < (patchDuration + 1)) {
            contiguousBlocks[currentBlockIdx].endTime = std::max(
                        soundFindResults[i].timestamp, contiguousBlocks[currentBlockIdx].endTime);
            contiguousBlocks[currentBlockIdx].patchEnd = i * patchDuration;
        }
        else if (soundFindResults[i].timestamp > contiguousBlocks[currentBlockIdx].startTime &&
                 soundFindResults[i].timestamp < contiguousBlocks[currentBlockIdx].endTime) {
            // NOTE: this could mean that the patch occurs multiple times in the intro
            // and it just matched better to itself earlier in the intro
            contiguousBlocks[currentBlockIdx].endTime = contiguousBlocks[currentBlockIdx].endTime + patchDuration;
            contiguousBlocks[currentBlockIdx].patchEnd = i * patchDuration;
        }
        else {
            currentBlockIdx++;
            contiguousBlocks.push_back({ soundFindResults[i].timestamp, soundFindResults[i].timestamp,
                                          i * patchDuration, i * patchDuration });
        }
    }

    // The block with the largest timespan becomes our found intro
    IntroChunkSearchResult maxBlock = { 0, 0, 0, 0 };
    for (auto& block : contiguousBlocks) {
        float max_timespan = maxBlock.endTime - maxBlock.startTime;
        float timespan = block.endTime - block.startTime;
        if (timespan > max_timespan) {
            maxBlock = block;
        }
    }

    assert(maxBlock.startTime < maxBlock.endTime);
    // NOTE: the endTime is at the start of the patch. Adjusted here to include
    // the full patch duration
    maxBlock.endTime += patchDuration;

    return maxBlock;
}

IntroChunkSearchResult
FindSound::doChunkScan(FloatSignal* one, FloatSignal* two,
                       size_t patchStart, size_t patchEnd, int patchDuration) {
    assert(patchEnd > patchStart);
    std::vector<FloatSignal*> patches;
    for (size_t i = patchStart; (i + patchDuration) < patchEnd &&
         i < SOURCE_END; i += patchDuration) {
        patches.push_back(FindSound::signalSlice(two, i, i + patchDuration));
    }

    std::vector<CorrelateResult> results(patches.size());

    for (size_t i = 0; i < results.size(); ++i) {
        results[i] = bestPatchPosition(one, patches.at(i));
    }

    for (auto patch : patches) {
        delete patch;
    }

    IntroChunkSearchResult scanResult = getChunkSearchResults(results, patchDuration);

    return scanResult;
}

IntroInfo FindSound::getIntroFromPair(FloatSignal* one, FloatSignal* two) {
    // ExecutionTimer timer("FindSound::getIntroFromPair");
    const int patchDuration = 4;
    IntroChunkSearchResult scanResult = doChunkScan(one, two, 0, SOURCE_END, patchDuration);
    float startTime = scanResult.startTime;
    float endTime = scanResult.endTime;
    std::cout << "Intro start time: " << startTime
              << ", Intro end time: " << endTime << std::endl;

    FloatSignal *introOne = FindSound::signalSlice(one, scanResult.startTime, scanResult.endTime);
    CorrelateResult find = FindSound::bestPatchPosition(
                        two, introOne);
    const float twoStartTime = find.timestamp;
    const float twoEndTime = twoStartTime + endTime - startTime;
    FloatSignal *introTwo = FindSound::signalSlice(
                two, twoStartTime, twoEndTime);
    CorrelateResult howClose = howCloseAreSignals(introOne, introTwo);

    delete introOne;
    delete introTwo;

    const IntroInfo result = {
        startTime,
        endTime,
        howClose.value,
        nullptr,
        twoStartTime,
        twoEndTime
    };

    return result;
}

int FindSound::nextBestIntro(const std::vector<FileSignal> &fileSignals, IntroInfo *result, int start)
{
    for (size_t i = start; i < fileSignals.size() - 1; ++i) {
        IntroInfo introInfo = FindSound::getIntroFromPair(
                    fileSignals[i].signal, fileSignals[i+1].signal);

        const int minLength = 20;
        const bool tooCloseToEnd = introInfo.endTime >= (SOURCE_END - minLength)
                || introInfo.otherEndTime >= (SOURCE_END - minLength);
        const bool tooShort = (introInfo.endTime - introInfo.startTime) <= minLength;

        if (introInfo.matchPercent >= ACCEPTANCE_THRESHOLD && !tooCloseToEnd && !tooShort) {
            FloatSignal *intro = FindSound::signalSlice(
                        fileSignals[i].signal, introInfo.startTime, introInfo.endTime);
            introInfo.intro = intro;
            *result = introInfo;
            return (int)i;
        }
    }

    return -1;
}
