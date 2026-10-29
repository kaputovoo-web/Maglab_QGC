#include <QImage>
#include "RtspCapture.h"
#include <QDebug>
#include <thread>
#include <QDir>
#include <QTime>
#include "QGCApplication.h"
#include "QGCToolbox.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "Fact.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
//#include <Yaw/AttitudeAnglesController.h>
}

RTSPCapture::RTSPCapture()
{
    //    AttitudeAnglesController* controller = new AttitudeAnglesController(this);
    //    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
}

RTSPCapture::~RTSPCapture()
{

}

extern SimpleTest* g_simpleTest;

void RTSPCapture::startRecord(QString url, QString filePath)
{
    if (m_recording) return;
    m_recording = true;

    std::thread([=]() {

        avformat_network_init();

        std::string urlStr = url.toStdString();
        std::string pathStr = filePath.toStdString();

        AVFormatContext *inFmt = nullptr;
        AVDictionary *opts = nullptr;

        av_dict_set(&opts, "rtsp_transport", "tcp", 0);

        if (avformat_open_input(&inFmt, urlStr.c_str(), nullptr, &opts) != 0) {
            qDebug() << "打开RTSP失败";
            return;
        }

        avformat_find_stream_info(inFmt, nullptr);

        // 输出上下文（MP4）
        AVFormatContext *outFmt = nullptr;
        avformat_alloc_output_context2(&outFmt, nullptr, nullptr, pathStr.c_str());

        int videoIndex = -1;

        // 找视频流
        for (unsigned i = 0; i < inFmt->nb_streams; i++) {
            if (inFmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoIndex = i;

                AVStream *inStream = inFmt->streams[i];
                AVStream *outStream = avformat_new_stream(outFmt, nullptr);

                // ★关键：直接拷贝参数（不重新编码）
                avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
                break;
            }
        }

        if (!(outFmt->oformat->flags & AVFMT_NOFILE)) {
            avio_open(&outFmt->pb, pathStr.c_str(), AVIO_FLAG_WRITE);
        }

        const int writeRet = avformat_write_header(outFmt, nullptr);
        if (writeRet < 0) {
            qWarning() << "avformat_write_header failed:" << writeRet;
        }


        AVPacket pkt;

        while (m_recording && av_read_frame(inFmt, &pkt) >= 0) {

            if (pkt.stream_index == videoIndex) {

                AVStream *inStream = inFmt->streams[videoIndex];
                AVStream *outStream = outFmt->streams[0];

                // ★时间戳转换（非常重要）
                pkt.pts = av_rescale_q_rnd(pkt.pts,
                                           inStream->time_base,
                                           outStream->time_base,
                                           (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));

                pkt.dts = av_rescale_q_rnd(pkt.dts,
                                           inStream->time_base,
                                           outStream->time_base,
                                           (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));

                pkt.duration = av_rescale_q(pkt.duration,
                                            inStream->time_base,
                                            outStream->time_base);

                pkt.pos = -1;

                av_interleaved_write_frame(outFmt, &pkt);
            }

            av_packet_unref(&pkt);
        }

        // 收尾
        av_write_trailer(outFmt);

        avio_close(outFmt->pb);
        avformat_free_context(outFmt);
        avformat_close_input(&inFmt);

        qDebug() << "录像完成";

    }).detach();
}

void RTSPCapture::stopRecord()
{
    m_recording = false;
}

bool RTSPCapture::rtsp_capture(QString url1, QString savePath)
{
    MultiVehicleManager* vehicleManager = qgcApp()->toolbox()->multiVehicleManager();
    Vehicle* vehicle = vehicleManager->activeVehicle();
    avformat_network_init();

    AVFormatContext *fmt = nullptr;
    AVDictionary *opts = nullptr;

    std::string urlStr = url1.toStdString();
    const char* url = urlStr.c_str();

    av_dict_set(&opts, "rtsp_transport", "tcp", 0);

    if (avformat_open_input(&fmt, url, nullptr, &opts) != 0) {
        qDebug() << "打开RTSP失败";
        return false;
    }

    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        qDebug() << "流信息失败";
        return false;
    }

    int vIndex = -1;
    for (unsigned i = 0; i < fmt->nb_streams; i++) {
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            vIndex = i;
            break;
        }
    }

    if (vIndex == -1) return false;

    const AVCodec *codec = avcodec_find_decoder(fmt->streams[vIndex]->codecpar->codec_id);
    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(ctx, fmt->streams[vIndex]->codecpar);

    if (avcodec_open2(ctx, codec, nullptr) < 0) return false;

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    SwsContext *sws = nullptr;

    int count = 0;
    bool success = false;

    while (av_read_frame(fmt, pkt) >= 0 && count < 300) {
        count++;

        if (pkt->stream_index == vIndex) {

            if (avcodec_send_packet(ctx, pkt) == 0) {

                // ★关键：循环取帧
                while (avcodec_receive_frame(ctx, frame) == 0) {

                // ★关键：只要关键帧
#ifdef AV_FRAME_FLAG_KEY
                    if (!(frame->flags & AV_FRAME_FLAG_KEY)) {
#else
                    if (!frame->key_frame) {
#endif
                        continue;
                    }

                    qDebug() << "拿到关键帧";

                    sws = sws_getContext(
                        ctx->width, ctx->height, ctx->pix_fmt,
                        ctx->width, ctx->height, AV_PIX_FMT_RGB24,
                        SWS_BILINEAR, nullptr, nullptr, nullptr);

                    if (!sws) break;

                    QImage img(ctx->width, ctx->height, QImage::Format_RGB888);

                    uint8_t *dst[1] = { img.bits() };
                    int linesize[1] = { img.bytesPerLine() };

                    sws_scale(sws, frame->data, frame->linesize,
                              0, ctx->height, dst, linesize);

                    // ★建议路径（Android可写）

                    QDir dir(savePath);
                    QString timeTag = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
                    QString fileName = QString("vis%1.jpg").arg(timeTag);
                    //    QString filePath = basePath + "/" + fileName;
                    QString Path = dir.filePath(fileName);
                    if (img.save(Path, "JPG", 90)) {
                        qDebug() << "截图成功:" << Path;

                            QString txtFilename = QString("txtvis%1.txt").arg(timeTag);
                        QString txtPath = dir.filePath(txtFilename);
                        QFile file(txtPath);
                        if(file.open(QIODevice::WriteOnly | QIODevice::Text))
                        {
                            QTextStream out (&file);
                            out.setRealNumberNotation(
                                QTextStream::FixedNotation);

                            out.setRealNumberPrecision(7);
                            out<<g_simpleTest->rlon()<<","<<g_simpleTest->rlat()<<"\n";
                            if (vehicle) {
                                double pitch = vehicle->pitch()->rawValue().toDouble();
                                double roll  = vehicle->roll()->rawValue().toDouble();
                                double yaw   = vehicle->heading()->rawValue().toDouble();
                                out<<pitch<<","<<roll<<","<<yaw<<"\n";
                                qDebug() << "pitch:" << pitch
                                         << "roll:"  << roll
                                         << "yaw:"   << yaw;
                            }
                            file.close();
                        }

                        success = true;
                    } else {
                        qDebug() << "保存失败:" << Path;
                    }

                    sws_freeContext(sws);
                    break;
                }
            }
        }

        av_packet_unref(pkt);

        if (success) break;
    }

    if (!success) {
        qDebug() << "未获取到关键帧，读取包数:" << count;
    }

    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&ctx);
    avformat_close_input(&fmt);

    return success;
}
