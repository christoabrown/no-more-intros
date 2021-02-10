#include "videolistitem.h"
#include "ui_mainwindow.h"
#include "ffmpeg.h"
#include "execution_timer.h"
#include "misc_util.h"
#include <QThreadPool>
#include <QBuffer>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>

void ThumbnailRenderTask::run() {
    Image *images;
    int height = 100;

    QByteArray ba = path.toLocal8Bit();
    get_video_frames(&images, ba.constData(), startTime, endTime, 5, height);

    for (int i = 0; i < 5; ++i) {
        Image image = *(images + i);
        QImage qimage(image.data, image.width,
                      image.height, image.width*3, QImage::Format_RGB888);
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::ReadWrite);
        qimage.save(&buffer, "PNG");
        free(image.data);
        image.data = (uint8_t*)malloc(ba.size());
        memcpy(image.data, ba.data(), ba.size());
        image.size = ba.size();
        emit sendThumbnailImage(image);
    }

    free(images);
}

VideoListItem::VideoListItem(QWidget *parent, QString path)
    : QWidget(parent), path(path)
{
    ui.setupUi(this);

    QFileInfo fileInfo(path);
    QFontMetrics fontMetrics = QFontMetrics(ui.filename->font());
    QString elidedFilename = fontMetrics.elidedText(
                fileInfo.baseName(), Qt::ElideRight, ui.filename->maximumWidth());
    ui.filename->setText(elidedFilename);
    ui.filename->setToolTip(fileInfo.baseName());
    ui.match->hide();
    setIntroTime(introStart, introEnd, false);

    QGraphicsDropShadowEffect *dropShadow = new QGraphicsDropShadowEffect();
    dropShadow->setBlurRadius(8.0);
    dropShadow->setXOffset(0.0);
    dropShadow->setYOffset(2.0);
    dropShadow->setColor(QColor(0, 0, 0, 61));
    ui.videoListItemFrame->setGraphicsEffect(dropShadow);

    QObject::connect(ui.startTime, &QTimeEdit::timeChanged, this, &VideoListItem::timeChanged);
    QObject::connect(ui.endTime, &QTimeEdit::timeChanged, this, &VideoListItem::timeChanged);
}

void VideoListItem::renderThumbnails()
{
    if (!needsToRender || introStart > introEnd) {
        return;
    }
    needsToRender = false;
    ThumbnailRenderTask *task = new ThumbnailRenderTask();
    task->path = this->path;
    task->startTime = introStart;
    task->endTime = introEnd;
    QObject::connect(task,
                     &ThumbnailRenderTask::sendThumbnailImage,
                     this,
                     &VideoListItem::receiveThumbnailImage);
    QThreadPool::globalInstance()->start(task);
}

void VideoListItem::receiveThumbnailImage(Image image)
{
    QLabel *thumbnailLabel;
    switch (image.count) {
    case 0:
        thumbnailLabel = ui.introThumbnail0;
        break;
    case 1:
        thumbnailLabel = ui.introThumbnail1;
        break;
    case 2:
        thumbnailLabel = ui.introThumbnail2;
        break;
    case 3:
        thumbnailLabel = ui.introThumbnail3;
        break;
    case 4:
        thumbnailLabel = ui.introThumbnail4;
        break;
    }
    QPixmap pixmap = QPixmap();

    pixmap.loadFromData(image.data, image.size);
    thumbnailLabel->setPixmap(pixmap);
    thumbnailLabel->setMinimumSize(pixmap.width(), pixmap.height());


    free(image.data);
}

void VideoListItem::updateWithResult(const FindSoundResult &findSoundResult)
{
    const float matchPercent = findSoundResult.introInfo.matchPercent;
    std::string s = std::to_string((int)(matchPercent*100)) + "%";
    ui.match->setText(s.c_str());

    if (findSoundResult.isSourceOfIntro) {
        ui.match->setStyleSheet("#match { color: #dbc900; }");
    } else if (matchPercent >= ACCEPTANCE_THRESHOLD) {
        ui.match->setStyleSheet("#match { color: #007e33; }");
    } else {
        ui.match->setStyleSheet("#match { color: #cc0000; }");
    }
    ui.match->show();

    setIntroTime(findSoundResult.introInfo.startTime, findSoundResult.introInfo.endTime, false);
}

void VideoListItem::timeChanged()
{
    const int start = qTimeToSeconds(ui.startTime->time());
    const int end = qTimeToSeconds(ui.endTime->time());
    setIntroTime(start, end, true);

    if (isVisible) {
        renderThumbnails();
    }
}

void VideoListItem::setIntroTime(const float start, const float end, bool skipQt)
{
    introStart = start;
    introEnd = end;
    needsToRender = true;

    if (!skipQt) {
        QTime qStart = qTimeFromSeconds(start);
        QTime qEnd = qTimeFromSeconds(end);
        ui.startTime->setTime(qStart);
        ui.endTime->setTime(qEnd);
    }
}
