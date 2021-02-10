#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "videolistitem.h"
#include "findsound.h"

struct ProgressContext {
    int max;
    int current;
};

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event);

private:
    ProgressContext progressContext;
    Ui::MainWindow *ui;
    std::unique_ptr<FindSound> findSound = nullptr;

    void maybeRenderVideoThumbnail();
    void setCategoryLabelsVisible(const bool visible);
    void setButtonsEnabled(const bool enabled);
    void beginProgress();
    void endProgress();
private slots:
    void addVideosButton();
    void findIntrosButton();
    void scrolled();
    void receiveProgress();
    void receiveFindSoundResult(FindSoundResult findSoundResult);
};
#endif // MAINWINDOW_H
