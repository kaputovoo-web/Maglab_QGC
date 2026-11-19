# RTSP 安全迁移审查

## 当前阻塞与实现边界

旧 RtspCapture.cpp 尚未参与主构建，不应恢复启用：

- detached std::thread 捕获 this，析构不取消/等待；阻塞 av_read_frame 没有 interrupt callback 或有效取消时限。
- 多条失败路径泄漏 format/codec/packet/frame/dictionary；输出上下文/视频流/写头/写包/收尾错误未完整检查。
- 同秒固定名称可覆盖文件，MP4 收尾未确认就可能销毁对象；无法准确区分录制请求与已开始/已完成。
- 截图同步阻塞 UI，以不存在的 extern g_simpleTest 读取数据，飞控/采集 QObject 在线程间访问不安全。
- 已带的 Android FFmpeg 静态库来源、许可证/构建配置、ABI 和 native 页对齐尚未核清；macOS 不可直接使用这些 Android 库。

不能直接以 VideoManager::grabImage 替代：当前 GstVideoReceiver::takeScreenshot 仅返回 STATUS_NOT_IMPLEMENTED。VideoManager::imageFileChanged 是请求通知，不是写盘成功。旧功能包含任意 RTSP URL 的独立截图、录像，以及截图对应的经纬度/姿态侧车，普通显示流录像不等价。

## 迁移路线

优先复用保留的 GStreamer，不链接未知旧库；需要独立接收实例，避免变更飞行界面流设置。

1. 明确 Idle/Connecting/Streaming/Recording/Finalizing/Failed 状态，重复请求拒绝；异步完成及失败信号，关闭页与应用退出取消。
2. RTSP URL 校验、显式用户发起，日志不得暴露 URL userInfo；连接/读流超时、无帧/断链反馈。
3. 录像复用 receiver 的 start/stopRecording 完成信号；EOS/mux 索引落盘后才成功，唯一私有路径，不覆盖已有文件。
4. 增加可用的 GStreamer 截图分支/帧获取，限制分辨率和内存，异步编码 QSaveFile；验证实际文件，不能只返回 STATUS_OK。
5. GUI 线程快照采集与飞控数据，附 UTC/有效性/来源和时间，不把不同时间的数据冒充帧同步定位；截图与侧车均写成功才报告完整成功。
6. 无服务器、迟到成功、销毁、连续截图/录像、磁场/载具断链及应用退出故障测试；macOS 和 Android 分别验证。

以上是已核对源代码后的迁移方案，不是功能完成声明。尚未开放旧 RTSP 页面入口。
