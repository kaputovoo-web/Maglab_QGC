# macOS 模块验证记录

## 当前范围

使用官方 Qt 6.11.1、Apple Silicon、macOS deployment 13、隔离的 GStreamer 1.28.4，显式 `QGC_CUSTOM_DIR=maglab`。不关闭 TLS、飞控插件或 GStreamer。不安装 Android 包。

本轮已接入设置菜单入口：

- Maglab AI Settings：`qrc:/maglab/LLMSettingsPage.qml`
- Mission Execution Records：`qrc:/maglab/TaskNameTimeSettings.qml`
- Magnetic Survey Collection：`qrc:/maglab/CollectionPage.qml`，采集/停止录制、选择 CSV 生成磁图；UDP bind/存储失败反馈。该批 macOS 集成构建返回 0，尚未实际设备采集验收。
- Maglab Diagnostics：`qrc:/maglab/DiagnosticPage.qml`，只读姿态与 GPS 接收显示；SingletonTest 与 LLMPlanCreator 的 macOS 编译已通过（maglab-diagnostics-build.exit=0）。未注册规划创建菜单。

只有 Maglab 模块编译启用时追加，不改变上游设置页面。执行记录页只是查看入口；已接入活动载具事件监听与 FlyView 懒加载观察器；仍未做真实/模拟载具事件验收，空列表不能解释为记录功能已验收。LLM/Sketch 创建与上传入口尚未开放。

## 编译与静态验证

- 修复 MissionController 的输出预检提示缺少 `AppMessages.h` 导致的 Android 编译错误；该修复将随 macOS 构建验证，不将此前失败称为通过。
- macOS 既有 `build/macos-qt6111` 已从默认 custom 切换为 maglab，重配成功，完整构建通过（`maglab-build.exit=0`）。修复 QGCCorePlugin 中 Viewer3DSettings 仅 Android include 导致的 macOS 不完整类型错误。设置搜索修复后再次增量构建返回 0。
- 构建脚本：`build/macos-qt6111/rebuild-maglab.sh`。
- 日志：同目录 `maglab-configure.log`、`maglab-build.log`，结束状态 `maglab-build.exit`。构建完成前不使用旧 QGroundControl.app 代替新 Maglab.app。
- AppSettings.qml qmllint exit 0，仍需动态 ListModel 与 Loader 运行检查。
- macOS 参数 helper 和标量帧 helper 各 CTest 1/1 通过；非正式项目 Unit，也非完整策略验收。
- 扫描器提示旧 Handle.qml 导入 MagLab 找不到 qmldir，该旧页面仍待迁移，不以扫描提示冒充功能可用。

## 旧模块兼容补齐（2026-10-28）

- 规划类型已注册：规划工具增加 LLM 与 Sketch 创建项，`createComplexMissionItem` 可加载对应 JSON；输出预检仍在保存/上传前阻止无效任务。补 `entryCoordinate()`、`editableAlt()` 及策略 `altitude()` 以匹配当前 `VisualMissionItem` 抽象接口。尚未做真实策略/载具任务验收。
- 规划器图标改为 `/maglab` 资源；四份编辑/地图 QML 与归档面板已入资源。
- 归档面板可从规划页归档抽屉和设置页打开；无控制器时只读，打开按钮禁用。飞行进度卡片改为只读消费 C++ 观察器状态，避免面板未打开时不记录。
- GStreamer 截图由 `STATUS_NOT_IMPLEMENTED` 改为下一个解码帧转 RGB，`QSaveFile` 写盘后返回完成；多接收器可能各自报一次完成。
- 媒体页面使用已配置的 QGC 视频流进行截图/录像，替代旧 detached RTSP 线程；录像仍保存在应用设置指定的视频目录，不是旧 MagLab 目录。
- macOS 构建连续通过，隔离 simple-boot-test 返回 0。Android Release 全量重配构建通过（697 步，含 APK 打包）。
- Android 测试包：`build/android-maglab-arm64/test-apk/Maglab-5.1.5-arm64-full-modules-test.apk`，包名 `org.maglab.groundcontrol.test`，arm64-v8a，debug 测试签名；SHA256 `5b478051a41468a65df6460d4faf59f39a325df062b2de5f497b97447b9db734`。`zipalign -c -P 16 4`、`apksigner verify` 均通过。未安装、未连接/操作飞控。debug 签名不代表正式发布签名。
- 旧 `Handle.qml`/`SimpleTest.qml` 页面未再打包（由 CollectionPage/MediaCapturePage 取代）；`VehicleParamOverride` 自动写持久飞控参数仍禁止启用。

## 规划编辑器布局重做（2026-10-28）

- LLM 编辑器改为紧凑表单：顶部一行模式按钮（方形占满末行），语音录音/文本/操作合并为一行区域，参数统一为“标签 + 数值 + 滑块”单行，超长参数说明收为小字灰色；高度统一为“高度 + 单位模式”。
- 删除重复的“输入框 + 滑块各占一行”以及重复统计标题；统计改动底部一行。安全复核按钮会根据 `readyForSaveState` 禁用，状态标签使用进度枚举值 0（ReadyForSave），修正原先误用 2（NotReadyForSaveData）的问题。
- 移除错误的固定“未上传”字样，改为“任务编辑”。
- Sketch 编辑器同步紧凑化：模式/绘制/清除两行，测绘参数和航点参数分区，航点选择行防负索引。
- 两个编辑器 qmllint exit 0，macOS 构建通过并已重启查看。未做小屏Android实际点击验收。

## 编辑器挤顶部根因（2026-10-29）

- 根因：LLM/Sketch 编辑器未声明 `missionItem`、`availableWidth`，`Loader.setSource(url, initialProperties)` 注入失败，宽度为 0、`missionItem` 未定义，所有内容堆在顶部。
- 已在两个编辑器根节点添加 `required property var missionItem` 与 `required property real availableWidth`。Sketch 内旧的 `object.` 引用统一改为 `missionItem.`。
- 日志确认不再出现这两个注入错误。仍保留 `vehicle` 非本模块警告。

## 响应式参数控件（2026-10-29）

- LLM：语音/螺旋/FOM/自定义/方形分段按钮；航线参数、飞行设置、姿态激励、航线摘要独立卡片。
- 每个连续参数使用“名称 + 数值输入 + 单位 + 滑动条”，通过 Fact 的 min/max/step 与 FactTextField 双向同步；窄于 340 px 时滑动条换行到参数下方。
- 方向等离散项使用分段按钮，窄屏自动改为下拉框；高度参考保持下拉框。
- AI 安全复核改为独立 Switch，开启后才显示复核按钮和结果；不自动请求。
- 模式按钮按宽度 3/5 列，统计使用 Flow 自动换行；外层 PlanTreeView 负责纵向滚动。
- LLM/Sketch qmllint exit 0，macOS 构建通过。未完成不同 DPI/安卓真机点击验收。

## MagLab 飞行页入口（2026-10-29）

- 飞行视图左侧工具条在 3D 按钮下方新增 `MagLab` 按钮，仅 MagLab 构建可见。
- 打开全屏 `MagLabPanel`：采集与磁图（SimpleTest 实时 S1/S2、开始/停止录制、选择 CSV 生成磁图）、磁图库（列目录、下拉选择、图片预览、刷新）、相机（截图/录像，复用 QGC 视频流）。
- 磁图库不再使用 `Qt.labs.folderlistmodel`，改由 `MagLabPaths::magMapFiles()` 用 `QDir` 按修改时间倒序列出 jpg/jpeg/png，避免 Android 额外 QML 插件依赖。
- 关闭面板会卸载 Loader；不自动开始采集、截图或录像。
- macOS 构建通过；Android Release 重建通过并签名测试包：`Maglab-5.1.5-arm64-maglab-workspace-test.apk`，SHA256 `e71e791695dbe921c313b43b52b2e46edecaa54c69b003621dd3832dcbca49d3`。尚未真机点击验收。

## 后续集成修复

- 四份规划编辑/地图 QML 已打入 `/maglab`，修正后端旧资源 URL；尚未开放规划入口。
- 磁图工作线程不再调用应用 QObject 单例解析存储路径，改主线程获取后传值。
- 异步磁图输出 UUID 文件名，QSaveFile 写图片/坐标，坐标写失败清理本次图片、不报告成功；两文件并非崩溃级原子事务。
- 磁图拒绝非有限/越界坐标及磁场、退化或跨反经线范围，删除默认静默抹平大于 100 的磁异常；缓存复用逐点比对坐标。该批修复仍待编译及真实数据验证。
- Handle 增加共享原子取消标志，采集页面提供取消按钮；读 CSV、逐点三角剖分、边界处理、栅格行与写文件前检查取消。析构先请求取消再等待，不使用 detached 线程；内部部分计算和文件提交仍不可抢占，尚需大数据页面销毁时延测试。
- 三角剖分结束移除辅助超级三角形顶点，避免污染结果/缓存。
- `build/maglab-handle-smoke/out` CTest 1/1 通过：真实生成函数、异常值保留、辅助顶点清理、唯一文件、预置取消、无效数据、存储失败。该临时 helper 不等于正式项目 Unit，不覆盖运行中取消与 GUI 销毁。
- 扩展临时 Handle smoke：20,000 行，工作启动后请求取消、真实 Handle 析构请求取消，CTest 1/1 通过（单次析构计时 0 ms）。并非 QML 页实际销毁/所有极端数据的时延保证。
- 执行观察：仅 armed + firmware missionFlightMode + 链路正常 + 已知指令列表/有效索引；独立载具任务条目，不冒充本地计划。当前索引不代表到达，不自动判完成；换机/断链/模式退出/解锁取消/新任务列表/退出应用结束为 interrupted。仍待模拟器事件测试与异常退出恢复。
- 新正式项目测试源码 `test/MissionManager/MaglabPlanStrategyTest.*` 调用四策略真实生成，覆盖三种 frame、count 稳定性、坐标/高度/速度、参数重载；Release testing OFF，尚未编译运行。并不覆盖整个 ComplexItem 序号/速度恢复/Home/Sketch/保存文件全链路，不称完整规划行为测试完成。
- 执行观察 macOS 构建两次返回 0，隔离 simple-boot-test 返回 0，无 QML error；qmllint exit 0 仍需页面运行验收。
- RTSP 审查和迁移边界见 `rtsp-migration.md`，现有 GStreamer 截图 STATUS_NOT_IMPLEMENTED 是真实阻塞，尚无完整替代实现。
- 以上取消修复的完整应用增量构建通过（maglab-cancel-build.exit=0），新应用隔离 simple-boot-test 返回 0（maglab-cancel-boot.exit）。RTSP 与自动参数覆盖仍未安全接入。

## 待执行运行检查

已执行新产物 `build/macos-qt6111/Release/Maglab.app/Contents/MacOS/Maglab --simple-boot-test`，返回 0（`maglab-boot-smoke.exit`）。该模式使用 PID 隔离的测试设置，并检查视频后端初始化及 QML 根窗口。日志仅见 Qt Multimedia FFmpeg 后端信息和 PinchArea enabled 覆盖警告，未见 QML 根窗口加载错误；这不证明实际 RTSP 视频或两个设置页已运行验收。DataRecorder macOS helper smoke 重新运行通过。正式 GUI 验证前确认无真实飞控链路。

1. 品牌、简体中文、设置菜单和两个 Loader 页面无类型/资源错误。
2. AI 设置密码字段遮蔽、小屏布局、HTTPS 配置、Fact 持久化；不显示或记录密钥。
3. 执行记录页空列表、滚动、多次切换，无重复单例。
4. 磁图导入/图层/PIP、3D 开关与现有近似 Home 高度语义回归。
5. 录音权限、取消、收尾；UDP 采集、录制停止与磁图生成另行运行验收。
6. 四策略完整生成、保存重载及故障阻断测试通过前不开放规划上传入口。

## 磁测工作站 UI 重构（2026-10-29）

- 顶部导航：深海军蓝品牌栏，MagLab 品牌、当前模块、传感器/ UDP / 存储状态、设置与关闭入口。
- 指标卡：S1、S2、S1−S2 统一卡片，显示 3 位小数、nT、通道标识与最近更新时间。
- 实时曲线：5/20/100 秒时间窗，去均值、自动缩放、实时跟随/暂停、适配、可切换图例、悬停十字线与读数提示、无数据空状态；三序列按各自窗口均值去均值，最多 600 个降采样点。
- 采集后端：`SimpleTest` 改为 QML 单例，新增 UDP 监听/最近采样/累计采样/录制时长/录制目录属性；新增 `magSeries()` 固定 105 秒环形窗口。
- 录制卡：状态灯、持续时间、采样总数、保存目录摘要；停止前确认。
- 数据处理：CSV 选择/拖放、文件信息、前 4 行预览、独立“生成磁图”动作、取消/进度/错误、生成结果预览。`Handle::selectSource()` 只预览，`generate()` 才处理；旧的直接赋值仍保持自动生成语义。
- 磁图库：宽屏列表+预览，窄屏下拉+预览，统一空状态与刷新。
- 相机：统一卡片、视频配置状态、截图/录像结果与文件路径；停止录像确认。
- qmllint exit 0，macOS/Android 构建通过。
- Android 测试包：`Maglab-5.1.5-arm64-workstation-ui-test.apk`，SHA256 `66521f58712e6c3b26ed854c0e783d096ef1e83797ecd1f58c6f567f79dfc252`（`Maglab-5.1.5-arm64-workstation-ui-v2-test.apk`）。
- 尚未真机点击、UDP 实流、拖放、悬停和不同 DPI 验收；截图/录像仍依赖 QGC 已配置视频流。

## 正式发布签名（Android，2026-10-29）

- 已生成 Maglab 自有长期签名密钥，未使用上游 QGC 的 `deploy/android/android_release.keystore`。
- 密钥库：`~/.maglab-signing/maglab-release.jks`（仓库外，RSA 4096，有效期 10000 天）
- 口令文件：`~/.maglab-signing/credentials.txt`（权限 600；必须离线备份，丢失后无法覆盖升级）
- 证书 SHA256：`9f5df027a8d2d1bbcbe185ef66c3f424145234f349128614144afdc2d8756220`
- 发布 APK：`build/android-maglab-arm64/release/Maglab-5.1.5-arm64-release.apk`
- APK SHA256：`5ce4a72470dffb9cbcbe3c660addacedc2d14489708eafa7cd959b0a5de3235d`
- 包 ID 仍为 `org.maglab.groundcontrol.test`，versionCode 665105000；公开商店发布前必须决定最终包 ID。
- 新发布密钥与设备上现有 debug 测试包签名不同，不能用 `adb install -r` 覆盖；强行安装只能先卸载（会清数据），未执行。
