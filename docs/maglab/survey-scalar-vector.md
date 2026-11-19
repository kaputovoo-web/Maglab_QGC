# 航磁标量 / 矢量采集

## 目录

- 标量：`MagLabPaths::scalarSurveyDir()` → `<app data>/survey/scalar`
- 矢量：`MagLabPaths::vectorSurveyDir()` → `<app data>/survey/vector`
- 历史兼容：首次访问标量目录时，会把旧版 `<app data>/survey` 下的 `*.csv` / `*.txt`
  复制到新目录，不移动、不删除原文件。新采集只写入对应模式目录。

## 通信设置

设置页「航磁通信设置」（`/maglab/SurveySettingsPage.qml`）配置并持久化到
`QSettings` 的 `MagLab/Survey` 组：

- 模式：标量 / 矢量
- 传输：UDP / TCP
- 本地监听 IP、端口
- 远程服务器 IP、端口
- 请求频率（毫秒）
- 心跳开关、自动采集、自动保存（统一点击“应用并保存”后持久化）

矢量 UDP 模式必须保持心跳；Maglab 在连接后会按频率持续发送 `F8`，不会只发送一次。默认值：标量 UDP `0.0.0.0:14551`；矢量 UDP `0.0.0.0:13551`，按请求频率向
`127.0.0.1:13551` 发送 `F8`。切换到矢量时，如果本地端口仍是标量默认值
14551，会自动改为 13551；切回标量时反向处理。录制过程中修改设置会被拒绝，
停止录制后自动应用。

## 协议

- 标量：保留原 `ScalarFrameParser` 的 24 字段 `UU...` 帧。
- 矢量：`VectorFrameParser` 解析
  `UU + len + time + lon + lat + alt_msl + fix_type + lidar +
  fg1_x/y/z + fg2_x/y/z + gyro_x/y/z + acc_x/y/z + roll + pitch + yaw +
  count + CRC16`。
- 矢量保存原始 XYZ 分量和姿态，同时用 `sqrt(x²+y²+z²)` 计算两路磁通门
  幅值供实时曲线和磁图使用。

## 磁图与历史

- 标量 CSV 使用 1/2 号磁传感器列（索引 6/7）计算差值。
- 矢量 CSV 自动识别表头，使用两路磁通门三轴分量计算幅值后作差。
- `Handle` 生成磁图、`MagneticMapController` 预览/历史加载都支持两种表头。
- 采集页仅在矢量模式显示 XYZ 分量卡片；磁场曲线和磁图仍使用标量/矢量统一
  的幅值字段。
- 磁图导入/生成在后台线程执行，UI 只负责显示 busy/结果；导入超过 60000 个点会
  提示拆分，避免 Android 内存不足。
- 实时曲线把 UDP 采样合并为约 5 Hz 重绘，C++ 曲线数据按采样修订号缓存，避免在
  Canvas 绘制时反复遍历全部样本。
