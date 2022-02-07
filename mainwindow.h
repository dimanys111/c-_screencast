#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QObject>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace Ui {
class MainWindow;
}

class Work : public QObject
{
    Q_OBJECT
private:
    int Mause_main();
    void tcp();
public:
    explicit Work();
    void udp();

public slots:
    void send(AVPacket *packet, AVFrame *frame);
    void set();
signals:
    void emitStart();
    void emitStop();
private slots:
    void newUser();
    void receiveTCP();
    void receiveUDP();
};

class MainWindow : public QObject
{
    Q_OBJECT

public:
    explicit MainWindow();
    ~MainWindow();
public slots:
    void timerStart();
    void timerStop();
private slots:

private:
    void createScrean();
signals:
    void emitSet();
    void emitIm(AVFrame* frame);
    void emitStop();
};

#endif // MAINWINDOW_H
