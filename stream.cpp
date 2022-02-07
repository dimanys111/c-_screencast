#include "stream.h"

#include <QImage>
#include <QDebug>
#include <QByteArray>

#include <chrono>

using namespace std;

static AVCodec *H264Codec=nullptr;
static AVCodecContext *H264Context=nullptr;
static uint8_t *pic_dat;
static int gotFrame;
static int wi;
static int hi;

void stream::stop()
{
    avcodec_close(H264Context);
    avcodec_free_context(&H264Context);
    H264Context=nullptr;
    qDebug()<<"stop";
}

void stream::zmain(AVFrame* frame)
{
//    auto begin = chrono::high_resolution_clock::now();
    v--;
    int c=0;
    if(!H264Context){
        wi = frame->width;
        hi = frame->height;
        H264Codec = avcodec_find_encoder(AV_CODEC_ID_H265);
        if (!H264Codec) {
            return;
        }
        H264Context = avcodec_alloc_context3(H264Codec);
        if (!H264Context) {
            return;
        }

        H264Context->pix_fmt = AV_PIX_FMT_YUV420P;
        H264Context->codec_id = AV_CODEC_ID_H265;
        H264Context->height = hi;
        H264Context->width = wi;
        H264Context->time_base.num = 1;
        H264Context->time_base.den = 25;

//        // Set Option
        AVDictionary *param = nullptr;
        //H.264
        if(H264Context->codec_id == AV_CODEC_ID_H265) {
            av_dict_set(&param, "crf", "25", 0);
            av_dict_set(&param, "preset", "ultrafast", 0);
            av_dict_set(&param, "tune", "zerolatency", 0);
        }

        if (avcodec_open2(H264Context, H264Codec, &param) < 0) {
            return;
        }

        c=3;
    }

    AVPacket* packet=av_packet_alloc();
    packet->data = nullptr;
    packet->size = 0;

    if (avcodec_encode_video2(H264Context, packet, frame, &gotFrame) < 0) {
        return;
    }

    packet->stream_index=c;

    emit emitKadr(packet,frame);
    v1++;

//    auto end = chrono::high_resolution_clock::now();
//    qDebug()<<chrono::duration_cast<chrono::nanoseconds>(end-begin).count()<<"ns";
}

stream::stream()
{
    v=10;
    v1=10;
    av_register_all();
}

