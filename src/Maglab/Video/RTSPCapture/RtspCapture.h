#include <QImage>
#include "Survey/SimpleTest/SimpleTest.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

class Vehicle;
class RTSPCapture : public QObject
{
    Q_OBJECT

public:
    explicit RTSPCapture();
    virtual ~RTSPCapture();
    Q_INVOKABLE  void startRecord(QString url, QString filePath);        //two
    Q_INVOKABLE  void stopRecord();                                      //two

    Q_INVOKABLE  bool rtsp_capture(QString url, const QString savePath);
private:
    std::atomic<bool> m_recording{false};                   //two
};
