#include "mainwindow.h"
#include "stream.h"

#include "QTimer"
#include "QUdpSocket"
#include <QBuffer>
#include <QGuiApplication>
#include <QScreen>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <chrono>

using namespace std;

int h;
int w;
int height;
int width;
uint8_t* buf;
AVFrame* frameFullScreen;
struct SwsContext* resize;
int num_bytesBUF;

#ifdef __linux__

#include <X11/Xutil.h>

#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>

struct ScreenShot {
    ScreenShot()
    {
        display = XOpenDisplay(nullptr);
        root = DefaultRootWindow(display);
        XGetWindowAttributes(display, root, &window_attributes);
        width = window_attributes.width;
        height = window_attributes.height;
        screen = window_attributes.screen;
        ximg = XShmCreateImage(display, DefaultVisualOfScreen(screen), DefaultDepthOfScreen(screen), ZPixmap, nullptr, &shminfo, width, height);
        shminfo.shmid = shmget(IPC_PRIVATE, static_cast<size_t>(ximg->bytes_per_line * ximg->height), IPC_CREAT | 0777);
        shminfo.shmaddr = ximg->data = static_cast<char*>(shmat(shminfo.shmid, nullptr, 0));
        shminfo.readOnly = False;
        if (shminfo.shmid < 0)
            puts("Fatal shminfo error!");
        ;
        Status s1 = XShmAttach(display, &shminfo);
        printf("XShmAttach() %s\n", s1 ? "success!" : "failure!");
        init = true;
    }

    void operator()(uint8_t* buf)
    {
        if (init)
            init = false;
        XShmGetImage(display, root, ximg, 0, 0, 0x00ffffff);
        memcpy(buf, ximg->data, num_bytesBUF);
    }

    ~ScreenShot()
    {
        if (!init)
            XDestroyImage(ximg);

        XShmDetach(display, &shminfo);
        shmdt(shminfo.shmaddr);
        XCloseDisplay(display);
    }

    Display* display;
    Window root;
    XWindowAttributes window_attributes;
    Screen* screen;
    XImage* ximg;
    XShmSegmentInfo shminfo;

    int x, y, width, height;

    bool init;
};

static ScreenShot screen;

#elif _WIN32

#include <windows.h>

#include <winuser.h>

class RDPCapture {
    int mx0;
    int my0;
    DEVMODE myDM;
    DISPLAY_DEVICE myDev;
    HDC m_driverDC;
    HDC m_cdc;
    BITMAPINFO m_BmpInfo;
    HBITMAP m_Bitmap;
    HBITMAP Old_bitmap;
    DEVMODE oldDM;

public:
    int mw;
    int mh;
    RDPCapture() {}
    RDPCapture(DISPLAY_DEVICE dev, DEVMODE dm)
    {
        myDev = dev;
        myDM = dm;
        oldDM = dm;
        m_driverDC = nullptr;
        m_cdc = nullptr;
    }
    ~RDPCapture()
    {
        SelectObject(m_cdc, Old_bitmap);
        DeleteObject(m_Bitmap);
        if (m_cdc != nullptr)
            DeleteDC(m_cdc);
        if (m_driverDC != nullptr)
            DeleteDC(m_driverDC);
        oldDM.dmDeviceName[0] = 0;
        ChangeDisplaySettingsEx(myDev.DeviceName, &oldDM, 0, 0, 0);
    }
    virtual bool Init(int x0, int y0, int width, int height)
    {
        mx0 = x0;
        my0 = y0;
        mw = width;
        mh = height;

        DEVMODE dm;
        dm = myDM;
        WORD drvExtraSaved = dm.dmDriverExtra;
        memset(&dm, 0, sizeof(DEVMODE));
        dm.dmSize = sizeof(DEVMODE);
        dm.dmDriverExtra = drvExtraSaved;
        dm.dmPelsWidth = 2048;
        dm.dmPelsHeight = 1280;
        dm.dmBitsPerPel = 24;
        dm.dmPosition.x = 0;
        dm.dmPosition.y = 0;
        dm.dmDeviceName[0] = '\0';
        dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_POSITION;
        if (ChangeDisplaySettingsEx(myDev.DeviceName, &dm, 0, CDS_UPDATEREGISTRY, 0)) {
            ChangeDisplaySettingsEx(myDev.DeviceName, &dm, 0, 0, 0);
        }
        //------------------------------------------------
        ZeroMemory(&m_BmpInfo, sizeof(BITMAPINFO));
        m_BmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        m_BmpInfo.bmiHeader.biBitCount = 24;
        m_BmpInfo.bmiHeader.biCompression = BI_RGB;
        m_BmpInfo.bmiHeader.biPlanes = 1;
        m_BmpInfo.bmiHeader.biWidth = mw;
        m_BmpInfo.bmiHeader.biHeight = -mh;
        m_BmpInfo.bmiHeader.biSizeImage = mw * mh * 3;
        m_driverDC = CreateDC(myDev.DeviceName, 0, 0, 0);
        m_cdc = CreateCompatibleDC(m_driverDC);
        m_Bitmap = CreateCompatibleBitmap(m_driverDC, mw, mh);
        Old_bitmap = (HBITMAP)SelectObject(m_cdc, m_Bitmap);
        return true;
    }
    virtual bool GetData(uint8_t* buf)
    {
        BitBlt(m_cdc, 0, 0, mw, mh, m_driverDC, mx0, my0, SRCCOPY | CAPTUREBLT);
        GetDIBits(m_cdc, m_Bitmap, 0, mh, buf, &m_BmpInfo, DIB_RGB_COLORS);
        return true;
    }
};

// These constants are missing on MinGW
#ifndef EDS_ROTATEDMODE
#define EDS_ROTATEDMODE 0x00000004
#endif
#ifndef DISPLAY_DEVICE_ACTIVE
#define DISPLAY_DEVICE_ACTIVE 0x00000001
#endif

int Detect_MirrorDriver(vector<DISPLAY_DEVICE>& devices, map<int, DEVMODE>& settings)
{
    WCHAR* all_mirror_divers[2] = {
        L"RDP Encoder Mirror Driver", //included in windows 7
        L"LEDXXX Mirror Driver" //my own mirror driver, used in Windows XP
    };
    DISPLAY_DEVICE dd;
    ZeroMemory(&dd, sizeof(dd));
    dd.cb = sizeof(dd);
    int n = 0;
    while (EnumDisplayDevices(NULL, n, &dd, EDD_GET_DEVICE_INTERFACE_NAME)) {
        n++;
        devices.push_back(dd);
    }
    for (int i = devices.size() - 1; i > -1; i--) {
        DEVMODE dm;
        ZeroMemory(&dm, sizeof(DEVMODE));
        dm.dmSize = sizeof(DEVMODE);
        dm.dmDriverExtra = 0;
        if (EnumDisplaySettings(devices[i].DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
            settings.insert(map<int, DEVMODE>::value_type(i, dm));
        }
    }
    for (int m = 0; m < 2; m++) {
        for (int i = 0; i < (int)devices.size(); i++) {
            WCHAR* drv = devices[i].DeviceString;
            if (!wcscmp(drv, all_mirror_divers[m])) {
                return m;
            }
        }
    }
    return -1; //can not use any mirror driver
}

RDPCapture* rdp_capture = nullptr;

static QScreen* screenM;

#endif

static QThread threWork, threStream;
static QTimer t;
static stream srt;
static Work work;
static QUdpSocket* Udp_Socket;

MainWindow::MainWindow()
{
    AVPixelFormat F;

#ifdef _WIN32

    F = AV_PIX_FMT_BGR24;
    screenM = QGuiApplication::primaryScreen();

    vector<DISPLAY_DEVICE> devices;
    map<int, DEVMODE> settings;
    int res = Detect_MirrorDriver(devices, settings);
    if (res == 0) {
        rdp_capture = new RDPCapture(devices[3], settings[3]);
        rdp_capture->Init(0, 0, screenM->geometry().width(), screenM->geometry().height());
        width = rdp_capture->mw;
        height = rdp_capture->mh;
    }

#elif __linux

    F = AV_PIX_FMT_BGRA;
    width = screen.width;
    height = screen.height;

#endif

    h = height / 2;
    while (h % 16 != 0)
        h = h + 1;
    w = width / 2;
    while (w % 16 != 0)
        w = w + 1;
    resize = sws_getContext(width, height, F, w, h, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
    frameFullScreen = avcodec_alloc_frame(); // this is your original frame
    frameFullScreen->format = F;
    frameFullScreen->quality = 1;
    num_bytesBUF = avpicture_get_size(F, width, height);
    buf = (uint8_t*)av_malloc((num_bytesBUF) * sizeof(uint8_t));
    avpicture_fill((AVPicture*)frameFullScreen, buf, F, width, height);

    work.moveToThread(&threWork);
    srt.moveToThread(&threStream);

    connect(&srt, &stream::emitKadr, &work, &Work::send);
    connect(this, &MainWindow::emitIm, &srt, &stream::zmain);
    connect(this, &MainWindow::emitStop, &srt, &stream::stop);

    connect(&work, &Work::emitStart, this, &MainWindow::timerStart);
    connect(&work, &Work::emitStop, this, &MainWindow::timerStop);
    connect(this, &MainWindow::emitSet, &work, &Work::set);

    connect(&t, &QTimer::timeout,
        [&]() {
            createScrean();
        });

    threWork.start();
    threStream.start();
    emit emitSet();
}

void MainWindow::timerStop()
{
    t.stop();
    emit emitStop();
}

void MainWindow::timerStart()
{
    t.start(30);
}

void MainWindow::createScrean()
{
    //auto begin = chrono::high_resolution_clock::now();
#ifdef __linux__
    screen(buf);
#elif _WIN32
    rdp_capture->GetData(buf);
#endif

    AVFrame* frame = avcodec_alloc_frame();
    frame->format = AV_PIX_FMT_YUV420P;
    frame->quality = 1;
    frame->height = h;
    frame->width = w;
    int num_bytes = avpicture_get_size(AV_PIX_FMT_YUV420P, w, h);
    uint8_t* frame2_buffer = (uint8_t*)av_malloc((num_bytes) * sizeof(uint8_t));
    avpicture_fill((AVPicture*)frame, frame2_buffer, AV_PIX_FMT_YUV420P, w, h);
    sws_scale(resize, frameFullScreen->data, frameFullScreen->linesize, 0, height, frame->data, frame->linesize);

    srt.v++;
    emit emitIm(frame);
    //auto end = chrono::high_resolution_clock::now();
    //qDebug()<<chrono::duration_cast<chrono::nanoseconds>(end-begin).count()<<"ns";
}

MainWindow::~MainWindow()
{
}

Work::Work()
{
}

static QTcpServer* tcpServer;
static QMap<QString, QTcpSocket*> tcpClients;

void Work::tcp()
{
    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection, this, &Work::newUser);
    if (!tcpServer->listen(QHostAddress::Any, 4322)) {
        qDebug() << QObject::tr("Unable to start the server: %1.").arg(tcpServer->errorString());
    } else {
        qDebug() << QString::fromUtf8("Сервер запущен!");
    }
}

void Work::udp()
{
    Udp_Socket = new QUdpSocket(this);
    Udp_Socket->bind(4321);
    connect(Udp_Socket, SIGNAL(readyRead()), this, SLOT(receiveUDP()));
}

void Work::set()
{
    tcp();
    udp();
}

void Work::receiveUDP()
{
    while (Udp_Socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(Udp_Socket->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort;

        Udp_Socket->readDatagram(datagram.data(), datagram.size(),
            &sender, &senderPort);

        QDataStream in(&datagram, QIODevice::ReadOnly);

        int c;
        in >> c;
        if (c == 0) {
            QByteArray addressBA = Udp_Socket->localAddress().toString().toUtf8();
            Udp_Socket->writeDatagram(addressBA.data(), addressBA.size(),
                sender, senderPort);
        }
    }
}

void Work::newUser()
{
    qDebug() << QString::fromUtf8("У нас новое соединение!");
    QTcpSocket* clientSocket = tcpServer->nextPendingConnection();
    connect(clientSocket, SIGNAL(readyRead()), this, SLOT(receiveTCP()));
    tcpClients[clientSocket->peerName()] = clientSocket;
    emit emitStart();
}

static int next_block_size = 0;

void Work::receiveTCP()
{ // slot connected to readyRead signal of QTcpSocket
    QTcpSocket* tcpSocket = static_cast<QTcpSocket*>(sender());
    QDataStream clientReadStream(tcpSocket);

    while (true) {
        if (!next_block_size) {
            if (tcpSocket->bytesAvailable() < static_cast<qint64>(sizeof(quint16))) { // are size data available
                break;
            }
            clientReadStream >> next_block_size;
        }

        if (tcpSocket->bytesAvailable() < next_block_size) {
            break;
        }

        int c;
        clientReadStream >> c;

        if (c == 1) {
            tcpClients.remove(tcpSocket->peerName());
            tcpSocket->close();
            tcpSocket->deleteLater();
            emit emitStop();
        }

        next_block_size = 0;
    }
}

void Work::send(AVPacket* packet, AVFrame* frame)
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << (int)(8 + packet->size);
    ds << packet->stream_index;
    ds.writeRawData(reinterpret_cast<char*>(packet->data), packet->size);
    ds << packet->flags;
    QMapIterator<QString, QTcpSocket*> i(tcpClients);
    while (i.hasNext()) {
        i.next();
        i.value()->write(ba.data(), ba.size());
    }
    srt.v1--;
    av_free(frame->data[0]);
    av_frame_free(&frame);
    av_packet_free(&packet);
}
