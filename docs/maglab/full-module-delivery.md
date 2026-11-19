# 全部旧模块交付范围

用户已要求最终 APK 覆盖全部旧模块，不以阶段测试包替代最终交付。内部仍逐模块增量构建验证，不代表逐模块交付。尚未完成全部集成和验收。

| 模块 | 已有进展 | 最终验收要求 |
| --- | --- | --- |
| Common / MagLabPaths | Qt6 私有可写存储已接入 | 失败反馈、历史数据保留、导入导出说明 |
| MagMap | 后端/地图页面已构建 | CSV、配准、图层、PIP、小屏交互 |
| Planning / LLM | 主项/策略/语音后端已构建 | 编辑/地图资源、创建加载、参数、指令序号、高度、保存重载、手动审核 |
| Planning / Sketch | 后端已构建 | 创建加载、编辑、转 Survey、保存重载 |
| Settings | LLM Facts/JSON 已构建；两个 Qt6 页面已打资源，类型注册及资源 Android 构建通过，qmllint exit 0（仍有警告），未开放设置入口 | LLM 设置页面、任务名称/时间页面接入 |
| Archive | 已修存储空路径/ID 遍历/提交失败反馈，Android 后端构建通过；QML 单例注册 Android 构建通过，未接页面入口 | 自动快照、加载、删除范围、执行记录、断链处理 |
| Survey / SimpleTest | 采集后端 Android 构建通过；生产解析 helper 的 macOS smoke 通过（88 种坏测量、坐标边界及原子输出），不等于设备链路验收 | 采集协议/设备链路、页面、背压/错误、停止销毁 |
| Survey / Handle | 已移除无用 QtCharts 依赖，修切换 CSV 复用旧缓存及空目录写入，Android 后端构建通过 | 异步生成、取消/销毁、输出原子性、CSV 与磁图一致 |
| Attitude / Yaw | Qt6 单例/通知/类型注册已迁移，Android Release 构建通过，未运行验收 | 类型注册、显示入口、多机切换、销毁和无效值 |
| Video / RTSPCapture | 待接入 | 采集功能完整迁移，不替换 GStreamer；线程退出、ABI/许可/native 页对齐 |
| Common / SingletonTest | 诊断辅助：已改为仅接收活动载具数据，未加入主构建 | 旧构造函数每 500ms 向全部载具注入伪造 GPS，已移除；验证只读与切换清理，不虚构用户功能 |
| Common / VehicleParamOverride | 不在原 MagLab.pri；发现自动修改飞控参数实现 | 不自动启用。以 spline 指令识别螺旋会误判其他任务，按 sysid 恢复参数存在身份冲突；须重设计并单独验证，不能为追求全接入直接启用 |

## 兼容性与安全边界

- “全部旧模块”包含迁移原功能，不意味着机械启用全部源码中的自动副作用。
- VehicleParamOverride 自动写 WPNAV/WP 参数与当前任务级速度隔离原则冲突；不允许在未审查、未明确用户操作的情况下自动修改持久飞控参数。须以安全实现替代或明确报告阻塞，不能悄悄遗漏后声称全部完成。
- 第三方 FFmpeg 需核对授权和二进制兼容；不能绕过 TLS、系统权限或以删减 GStreamer 达成构建。
- 无法从复制目录恢复的上游集成点需按当前 v5.1.5 重建。
- 最终包必须经过完整构建、相关项目测试、签名和设备验收。安装前仍需无飞行任务及断开飞控链路确认，不卸载、不清数据。

完整发布流程另见 [apk-release-checklist.md](apk-release-checklist.md)。
