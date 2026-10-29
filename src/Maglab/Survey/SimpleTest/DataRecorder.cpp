#include "DataRecorder.h"
#include <QFile>
#include <QTextStream>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QCoreApplication>
#include <QStringConverter>
#include <QUuid>

// ---------------------------------------------------------------------------
// Worker：QThread 子类。run() 在专属线程里持续消费队列、写文件。
// startInternal / enqueue / stopInternal 由主线程直接调用，mutex 保护跨线程安全。
// ---------------------------------------------------------------------------
class DataRecorderWorker : public QThread
{
    Q_OBJECT
public:
    DataRecorderWorker()
        : m_running(false), m_stop(false), m_file(nullptr), m_out(nullptr)
    {}

    ~DataRecorderWorker() {
        stop();
        wait();
    }

    // 在 worker 线程运行：持续等待队列，取出后写文件
    void run() override {
        int flushCount = 0;
        for (;;) {
            QString item;
            {
                QMutexLocker locker(&m_mutex);
                while (m_queue.isEmpty() && !m_stop) {
                    m_wait.wait(&m_mutex);
                }
                if (m_queue.isEmpty() && m_stop) {
                    break;   // 明确停止且队列清空 → 退出
                }
                item = m_queue.dequeue();

                // 写文件（在锁内，保证 m_file/m_out 生命周期安全）
                if (m_file && m_out) {
                    (*m_out) << item << "\n";
                    // 批量 flush：每 20 行刷一次盘，降低高频磁盘 IO
                    if (++flushCount >= 20) {
                        m_out->flush();
                        m_file->flush();
                        flushCount = 0;
                    }
                }
            }
        }

        // 清理（最后强制 flush 残余缓冲）
        QMutexLocker locker(&m_mutex);
        if (m_out) { m_out->flush(); delete m_out; m_out = nullptr; }
        if (m_file) { m_file->flush(); m_file->close(); delete m_file; m_file = nullptr; }
    }

    // 开始录制：打开新文件（主线程调用，锁保护）
    void startInternal(const QString &dirPath) {
        QMutexLocker locker(&m_mutex);
        if (m_running || m_stop) return;
        if (dirPath.isEmpty() || !QDir::isAbsolutePath(dirPath)) {
            qWarning() << "DataRecorderWorker: recording requires an absolute writable directory";
            return;
        }

        QDir dir(dirPath);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                qWarning() << "DataRecorderWorker: cannot create dir" << dirPath;
                return;
            }
        }

        const QString timeTag = QDateTime::currentDateTimeUtc().toString("yyyyMMdd_hhmmss_zzz");
        const QString filePath = dir.filePath(QString("data_log_%1_%2.txt")
                .arg(timeTag, QUuid::createUuid().toString(QUuid::WithoutBraces)));

        m_file = new QFile(filePath);
        if (!m_file->open(QIODevice::WriteOnly | QIODevice::NewOnly | QIODevice::Text)) {
            qWarning() << "DataRecorderWorker: open failed:" << filePath << m_file->errorString();
            delete m_file; m_file = nullptr;
            return;
        }
        m_out = new QTextStream(m_file);
        m_out->setEncoding(QStringConverter::Utf8);
        m_running = true;
        qDebug() << "DataRecorderWorker started, file:" << filePath;
    }

    // 入队一行（主线程调用，锁保护，非阻塞）
    void enqueue(const QString &line) {
        QMutexLocker locker(&m_mutex);
        if (!m_running || m_stop) return;
        m_queue.enqueue(line);
        m_wait.wakeAll();
    }

    // 停止录制（写完全部队列后线程仍运行，等待下一次 start）
    void stopInternal() {
        QMutexLocker locker(&m_mutex);
        m_running = false;
        // Drain under the same lock as the consumer before closing the session.
        // Otherwise a quick restart can write old queued rows into the next file.
        while (!m_queue.isEmpty()) {
            const QString line = m_queue.dequeue();
            if (m_out) (*m_out) << line << '\n';
        }
        if (m_out) {
            m_out->flush();
            if (m_out->status() != QTextStream::Ok) {
                qWarning() << "DataRecorderWorker: stream write failed";
            }
            delete m_out;
            m_out = nullptr;
        }
        if (m_file) {
            if (!m_file->flush()) {
                qWarning() << "DataRecorderWorker: flush failed:" << m_file->errorString();
            }
            m_file->close();
            delete m_file;
            m_file = nullptr;
        }
    }

    // 请求线程退出（析构时调用）
    void stop() {
        QMutexLocker locker(&m_mutex);
        m_stop = true;
        m_wait.wakeAll();
    }

    bool recording() const {
        QMutexLocker locker(&m_mutex);
        return m_running;
    }

private:
    mutable QMutex m_mutex;
    QWaitCondition m_wait;
    QQueue<QString> m_queue;
    bool m_running;
    bool m_stop;
    QFile *m_file;
    QTextStream *m_out;
};

// ---------------------------------------------------------------------------
// DataRecorder
// ---------------------------------------------------------------------------
DataRecorder* DataRecorder::s_instance = nullptr;

DataRecorder* DataRecorder::instance() {
    if (!s_instance) {
        s_instance = new DataRecorder(QCoreApplication::instance());
    }
    return s_instance;
}

DataRecorder::DataRecorder(QObject *parent)
    : QObject(parent), m_worker(new DataRecorderWorker())
{
    m_worker->start();   // 直接启动 worker 线程（run() 开始消费队列）
}

DataRecorder::~DataRecorder() {
    if (m_worker) {
        m_worker->stop();
        m_worker->wait(3000);
        delete m_worker;
        m_worker = nullptr;
    }
    if (s_instance == this) s_instance = nullptr;
}

void DataRecorder::startRecord(const QString &dirPath) {
    if (m_worker) m_worker->startInternal(dirPath);
}

void DataRecorder::stopRecord() {
    if (m_worker) m_worker->stopInternal();
}

void DataRecorder::push(const QString &line) {
    if (m_worker) m_worker->enqueue(line);
}

bool DataRecorder::isRecording() const {
    return m_worker ? m_worker->recording() : false;
}

#include "DataRecorder.moc"
