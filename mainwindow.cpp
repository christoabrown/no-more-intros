#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QDir>
#include <QScrollBar>
#include <QCoreApplication>
#include <QTimer>
#include <iostream>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->videoFilesContainer->layout()->setAlignment(Qt::AlignTop);
    ui->progressBarContainer->hide();
    findSound = std::make_unique<FindSound>();

    setCategoryLabelsVisible(false);

    QObject::connect(ui->addVideosButton, SIGNAL(clicked()), this, SLOT(addVideosButton()));
    QObject::connect(ui->findIntrosButton, SIGNAL(clicked()), this, SLOT(findIntrosButton()));
    QObject::connect(ui->scrollArea->verticalScrollBar(),
                     &QScrollBar::valueChanged, this, &MainWindow::scrolled);
    QObject::connect(ui->scrollArea->verticalScrollBar(),
                     &QScrollBar::sliderReleased, this, &MainWindow::scrolled);
    QObject::connect(findSound.get(), &FindSound::sendProgress, this, &MainWindow::receiveProgress);
    QObject::connect(findSound.get(), &FindSound::sendFindSoundResult,
                     this, &MainWindow::receiveFindSoundResult);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::addVideosButton() {
    QStringList files = QFileDialog::getOpenFileNames(
                this, "Select videos to add", QDir::currentPath(), "Videos (*.mkv *.mp4 *.webm *.mov *.avi)");
    if (files.size() == 0) {
        return;
    }

    QLayout *layout = this->ui->videoFilesContainer->layout();
    const int size = files.size();
    std::vector<QString> filepaths;
    this->ui->videoFilesContainer->setUpdatesEnabled(false);
    for (int i = 0; i < size; ++i) {
        QString path = files.at(i);
        VideoListItem *item = new VideoListItem(nullptr, path);
        layout->addWidget(item);
        filepaths.push_back(path);
    }
    ui->videoFilesContainer->setUpdatesEnabled(true);
    setButtonsEnabled(false);
    progressContext = { (int)filepaths.size(), 0 };
    beginProgress();
    ui->statusbar->showMessage("Getting sound data from videos...");
    findSound->addFiles(filepaths);

    // Give some time for the layout to calculate before checking for video thumbnail rendering
    QTimer::singleShot(500, this, &MainWindow::maybeRenderVideoThumbnail);
}

void MainWindow::findIntrosButton()
{
    setButtonsEnabled(false);
    const int count = findSound->run();
    progressContext = { count, 0 };
    beginProgress();
    ui->statusbar->showMessage("Finding intros in videos...");
}

void MainWindow::scrolled() {
    if (!ui->scrollArea->verticalScrollBar()->isSliderDown()) {
        maybeRenderVideoThumbnail();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    maybeRenderVideoThumbnail();
}

void MainWindow::maybeRenderVideoThumbnail() {
    const int scrollTop = ui->scrollArea->verticalScrollBar()->value();
    const QRect scrollRect = ui->scrollArea->frameGeometry();
    const int scrollBottom = scrollTop + scrollRect.height();

    const QLayout *layout = ui->videoFilesContainer->layout();
    const int count = layout->count();
    for (int i = 0; i < count; ++i) {
        VideoListItem *item = (VideoListItem*)layout->itemAt(i)->widget();
        const int itemTop = item->y();
        const int itemBottom = itemTop + item->height();
        const bool isOutside = itemTop > scrollBottom || itemBottom < scrollTop;
        const bool isInside = (itemTop > scrollTop || itemBottom < scrollBottom) && !isOutside;
        if (isInside) {
            item->isVisible = true;
            item->renderThumbnails();
        } else {
            item->isVisible = false;
        }
    }
}

void MainWindow::setCategoryLabelsVisible(const bool visible)
{
    ui->uncategorizedLabel->setVisible(visible);
    ui->goodFitLabel->setVisible(visible);
    ui->badFitLabel->setVisible(visible);
}

void MainWindow::setButtonsEnabled(const bool enabled)
{
    ui->findIntrosButton->setEnabled(enabled);
    ui->clearButton->setEnabled(enabled);
    ui->selectAllButton->setEnabled(enabled);
    ui->deselectAllButton->setEnabled(enabled);
    ui->addVideosButton->setEnabled(enabled);
    const QLayout *layout = ui->videoFilesContainer->layout();
    const int count = layout->count();
    for (int i = 0; i < count; ++i) {
        VideoListItem *item = (VideoListItem*)layout->itemAt(i)->widget();
        item->ui.findOthersButton->setEnabled(enabled);
        item->ui.endTime->setEnabled(enabled);
        item->ui.startTime->setEnabled(enabled);
    }
}

void MainWindow::beginProgress()
{
    ui->progressBar->setValue(0);
    ui->progressBarContainer->show();
}

void MainWindow::endProgress()
{
    if (progressContext.current == progressContext.max) {
        ui->progressBarContainer->hide();
        ui->statusbar->showMessage("");
    }
}

void MainWindow::receiveProgress()
{
    progressContext.current++;
    ui->progressBar->setValue((float)progressContext.current / progressContext.max * 100.0f);

    if (progressContext.current == progressContext.max) {
        setButtonsEnabled(true);
        ui->statusbar->showMessage("Done.");

        QTimer::singleShot(1000, this, &MainWindow::endProgress);
    }
}

void MainWindow::receiveFindSoundResult(FindSoundResult findSoundResult)
{
    VideoListItem *item = (VideoListItem*)ui->videoFilesContainer->layout()->
            itemAt((int)findSoundResult.index)->widget();
    item->updateWithResult(findSoundResult);
}
