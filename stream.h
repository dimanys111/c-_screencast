#ifndef STREAM_H
#define STREAM_H

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

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55,28,1)
 #define avcodec_alloc_frame() av_frame_alloc()
#else
 #define av_frame_alloc() avcodec_alloc_frame()
#endif

class stream : public QObject
{
    Q_OBJECT
public:
    volatile int v;
    volatile int v1;
    stream();
public slots:
    void zmain(AVFrame *frame);
    void stop();
signals:
    void emitKadr(AVPacket* packet,AVFrame *frame);
};

#endif // STREAM_H
