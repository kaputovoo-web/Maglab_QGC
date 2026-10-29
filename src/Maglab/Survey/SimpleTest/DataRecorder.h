#pragma once
#include <QObject>
#include <QString>

class DataRecorderWorker;

class DataRecorder : public QObject
{
    Q_OBJECT
public:
    static DataRecorder* instance();

    // 开始录制（传入目录路径）
    Q_INVOKABLE void startRecord(const QString &dirPath);

    // 停止录制（会把队列中残留写完再关闭）
    Q_INVOKABLE void stopRecord();

    // 推送一行数据到队列（非阻塞）
    Q_INVOKABLE void push(const QString &line);

    Q_INVOKABLE bool isRecording() const;

private:
    explicit DataRecorder(QObject *parent = nullptr);
    ~DataRecorder();

    DataRecorder(const DataRecorder&) = delete;
    DataRecorder& operator=(const DataRecorder&) = delete;

    DataRecorderWorker *m_worker = nullptr;   // 本身就是一个 QThread 子类
    static DataRecorder* s_instance;
};
