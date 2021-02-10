#ifndef VIDEOLISTITEM_H
#define VIDEOLISTITEM_H

#include <QWidget>
#include <QRunnable>
#include "ui_videolistitem.h"
#include "findsound.h"

struct Image {
    uint8_t *data;
    int size;
    int width;
    int height;
    int count;
};

Q_DECLARE_METATYPE(Image);

class ThumbnailRenderTask : public QObject, public QRunnable {
    Q_OBJECT
public:
    QString path;
    float startTime;
    float endTime;
    void run() override;
signals:
    void sendThumbnailImage(Image image);
};

class VideoListItem : public QWidget
{
    Q_OBJECT
public:
    Ui::VideoFileListItem ui;
    bool isVisible = false;
    explicit VideoListItem(QWidget *parent = nullptr, QString path = nullptr);
    void renderThumbnails();
    void updateWithResult(const FindSoundResult &findSoundResult);

private slots:
    void receiveThumbnailImage(Image image);
    void timeChanged();

private:
    QString path;
    bool needsToRender = true;
    float introStart = 0.0f;
    float introEnd = 60.0f;
    void setIntroTime(const float start, const float end, bool skipQt);
};

#endif // VIDEOLISTITEM_H
