# Maglab 旧模块迁移记录

## 状态

`src/Maglab` 是用户提供的旧版 Qt 5/qmake 功能源码。公共存储、磁图后端及磁图 QML 页面、DataRecorder、Sketch 后端，以及 LLM 的四种几何策略和设置后端已接入 Maglab 专属 CMake 条件并通过 Android Release 构建；LLM 主项与语音策略也已加入后端构建，规划资源、创建/加载入口仍未开放。已安装 APK 仍不包含这些模块。当前 3D 功能保持不变。

## 本轮 LLM 策略与设置后端

- Spiral/Fom/Custom/Square 四种策略及 LLMSettings 加入定制构建，设置元数据资源使用 `/json/LLM.SettingsGroup.json`；尚未增加 SettingsManager 实例、设置页面或任务创建入口。
- CustomPlanStrategy 在修改任何 Fact/偏移量前校验输入类型、有限数值、正速度及高度模式 0/1/2，拒绝缺失 east/north 的偏移对象，避免无效 AI 数据静默变成原点；尚待行为测试。
- Android Release 构建 exit 0，`build-icon-zh.log` 明确包含四种策略及 LLMSettings 的对象编译记录；runner 为 `maglab-strategies-runner.log`。`git diff --check` 通过。未签名安装、未运行任务行为测试。
- 语音拆出独立 `VoiceAudioRecorder`：Qt6 QMediaCaptureSession/QAudioInput/QMediaRecorder，按需异步申请 QMicrophonePermission；权限弹窗期间松开按钮取消本次录音；等待 StoppedState 后才交付音频，不再固定等待 300 毫秒。使用对象专属临时目录与 UUID WAV 文件，销毁时先关闭后端再清理目录；无设备/无存储/不支持 WAV/录音失败均返回状态，不回退未知格式。
- VoicePlanStrategy 已改调用上述录音适配层，并移除旧 QGCToolbox/SettingsManager 依赖，直接通过 LLMSettings Facts 读取持久配置；环境变量优先级保持不变。LLMComplexItem/VoicePlanStrategy 已加入后端构建，但尚未开放入口。录音适配层已通过 Android arm64 Release 编译及 APK 打包（`maglab-voice-audio-runner.log` 对应 `build-icon-zh.exit` 为 0，日志有 `VoiceAudioRecorder.cc.o`）；设备权限、实际 WAV 收尾及录音中销毁尚待运行验证，未签名安装。

## LLM 主项与网络保护增量

- LLMComplexItem 使用当前 JsonParsing、全局 MissionFlightStatus_t 和 Survey canonicalName；转 Survey 前拒绝已移出任务的项，延迟移除用 QPointer 防止对象失效。
- LLM/STT/复核请求仅允许无内嵌凭据的 HTTPS endpoint，禁止自动重定向，3xx 明确提示直接配置最终 endpoint；不忽略 TLS 错误。STT 仅在同一 HTTPS origin 时复用 LLM Key，跨供应商必须单独配置。
- 每个通道拒绝重复请求，LLM/STT 回调校验当前 reply 身份；设置在用户请求时重读。仍需运行验证超时、销毁、重试及响应大小限制。
- AI 复核提示不再假定全球统一 120 米法定限高，不把建议冒充飞行许可，也不推断缺失的地形/高程基准。
- LLMComplexItem/VoicePlanStrategy 及最新网络保护增量 Android Release 构建均通过；最新 runner 为 `maglab-llm-network-guards-runner.log`，`build-icon-zh.exit` 为 0，日志有两个源文件的对象编译记录。未安装、未验证任务行为。

## 策略参数加载校验与离线检查

- 新增 `PlanParameterValidation.h`，Spiral/Fom/Square 在修改 Facts 前统一校验所有已知数字字段：拒绝字符串、布尔、null、非有限值、越界数值，以及小数枚举/小数圈数（FOM）/小数段数。缺失的可选字段保留原值；上下限与现有 Fact 元数据一致，不代表法规或飞行安全限值。
- FOM 同时校验 radius/loops 兼容别名（radius 乘二后的 sideLength 范围一致）。Custom 原有专门偏移量校验保留。
- `applyMissionIntent` 要求 planMode 为字符串、params 为对象；就绪状态要求至少两点、有限高度、正速度和合法高度模式。
- `build/maglab-parameter-smoke` 在 macOS Qt6.11.1 编译并通过 CTest 1/1，实际检查 19 项（类型、上下界、别名、三种高度模式 JSON roundtrip 等）。该程序测试生产校验函数，不覆盖完整策略 Facts、几何或任务保存重载，也不是正式项目 Unit 测试。
- 三种策略 Android Release 编译/打包通过（`maglab-parameter-guards-runner.log`）；就绪状态增量也已编译/打包通过（`maglab-readiness-guards-runner.log`，exit 0，日志有 LLMComplexItem.cc.o）。未签名安装。

## 优先问题

1. 用户确认问题出现在 LLM 规划末尾的“未知：20”（RTL），随后明确授权删除自动追加的这一项。已移除 LLMPlanCreator 的 insertLandItem 调用：只影响新建 LLM 计划，不修改已有文件或飞控任务，也不再保证任务末尾有返航指令。规划模块尚未编译验证。
2. 磁图与 FlyView 图层：区分栅格、航线、实际航迹、航点、载具及交互面板；检查 PIP、地图旋转、缩放和布局。只调 z 不能解决图像配准或不同父级的遮挡。

## 已发现与第一批源码调整（尚未编译）

- LLM/Sketch PlanCreator 使用旧构造接口的 QObject parent 实参；按当前 PlanCreator 改为支持载具类型列表，生命周期由 PlanMasterController 管理。LLM 创建计划后增加起飞项空值保护。
- LLM 策略直接重算几何时未通知 path/exitCoordinate/exitCoordinateSameAsEntry，可能使下一航段（含返航连接）停留在旧出口。补充通知；需在集成后验证，尚不能断言是用户返航报错的全部原因。
- 磁图图层使用当前版本航线/航迹常量推导下层位置，不再硬编码层级。旧代码已将图层设在航线以下，因此实际遮挡还需接入后的场景验证。

## 后续接入阻塞与审查

- Qt Quick Controls 1/Styles、旧 Qt5 Android JNI 不兼容 Qt6。
- `MagLab/...` include 与实际 `Maglab` 大小写不符。
- QML 类型注册、资源 URL、SettingsManager、任务创建/加载注册点均待迁移。
- 公共存储旧流程申请所有文件访问；应改为可写目录及明确导出，不自动迁移/删除历史数据。
- RTSP 带第三方 FFmpeg 预编译库，需核查平台/ABI/依赖/授权与 Android 页对齐，不能替换或破坏上游 GStreamer。
- 任务生成有非法项丢弃而静态序号计数未同步的风险；需要针对策略及上传序列测试。
- 语音/LLM 任务生成、远端审查不能提供飞行安全保证；需要明确用户审核，不自动上传/执行。

后续按少量文件增量接入与构建，不把当前上游成功构建当作旧模块通过验证。

## 采集记录后端修复

- DataRecorder 独立加入 Maglab CMake，暂不启用采集 UI／设备链路。
- Qt6 编码改为 QStringConverter::Utf8；录制文件采用 UTC 毫秒加 UUID 和 NewOnly，避免同秒合并及覆盖。
- 拒绝空／相对目录；未录制时不接受新队列数据。停止时在锁保护下排空队列并关闭文件，避免快速重启把旧数据写进下一文件；单例由应用管理销毁。
- SimpleTest 仅在记录器成功启动后设置 saveFlag，重复开始不会重复写表头；该调用方尚未接入构建。
- 独立 macOS Qt 6.11.1 smoke 程序运行通过（build/maglab-recorder-smoke）：空目录拒绝、两次同毫秒启动生成不同文件、每会话 100 行无串写、停止后数据不写入、UTF-8 和停止排空验证。不是完整项目测试或 Android 实机存储验收。
- 当前停止仍会同步排空队列；高频大积压时 UI 延迟、写入失败的 UI 状态传播及队列背压尚需后续改进。

## 公共存储及磁图后端第一次构建

- 新增 `src/Maglab/CMakeLists.txt`，仅自定义目录为 maglab 时编译公共存储和磁图后端，不改上游默认功能。
- 移除公共存储 Qt5 JNI/所有文件访问权限申请，Android 使用应用私有目录。桌面保留可写 Documents/MagLab。旧目录不搬移、不删除；后续 UI 须说明导入导出及卸载会移除应用私有数据。
- 可写探测改临时文件，避免截断固定 `.write_probe`；失败返回空路径、不回退当前工作目录；生成文件名加入 UUID，后缀禁止路径分隔符。
- 磁图修复 include 大小写、Qt6 容器长度类型不匹配及经纬度 NaN/Inf 校验。
- Android 构建 exit 0（`maglab-backends-runner.log`），两个后端确实有编译记录；`git diff --check` 通过。未做运行或自动化行为验证，未签名安装。
- 规划第一批改动仍未编译；图层问题尚未真机验收。

## Sketch 后端第一次接入

- SketchComplexItem 和 SketchPlanCreator 加入 Maglab 构建，但不注册创建／加载入口，不开放上传。
- 更新 MissionFlightStatus_t 类型、显式 Vehicle include，以及原生 Survey 的 canonicalName 创建接口；包含此前的延迟回调 QPointer 修复。Android arm64 构建 exit 0（maglab-sketch-compat-runner.log），不是任务行为验收。
- 后续增加保护：Sketch 不在当前任务列表时拒绝转 Survey，避免错误追加到任务末尾；SurveyArea 未转换时不标为 ReadyForSave，因为该模式 appendMissionItems 不生成指令。航点模式检查速度、高度有限及高度模式范围。此后续保护正在增量构建。
- LLM 语音策略仍含 QAudioRecorder/QtAndroid/QGCToolbox 等旧接口，需迁移录音权限、配置和请求生命周期后再接入；不移除 TLS 校验或自动执行生成任务。

## 规划 QML 第一批 Qt6 兼容修复（尚未接入运行）

- LLM/Sketch 四个 editor/map visual 改 versionless Qt6 imports，移除旧 ScreenTools/Palette/FactSystem 独立 URI，地图标记显式导入 PlanView。
- LLM 八个滑块改 from/to/onMoved，删除旧 updateValueWhileDragging/tickmarksEnabled，避免初始化或绑定更新触发回写任务 Facts；滑块高度使用当前控件 implicitHeight。
- AltitudeFactTextField 改当前 altitudeFrame 属性；模式 2 对应生成代码的 MAV_FRAME_GLOBAL_TERRAIN_ALT，因此显示 AltitudeFrameTerrain，而不是地面站计算绝对高程模式。仅修正 UI 标签，不转换或修改任务高度。
- 语音和选项事件使用显式函数参数；LLM 中心拖动通过 mapToItem 转为地图坐标，避免把局部坐标直接当地图像素；方向箭头创建增加空值防护，支持两点路径。
- 四份 qmllint exit 0，无本次检索的 Cannot assign/Failed to import/Error，但仍有动态上下文及类型解析警告。未纳入运行资源／规划后端构建，不能算完整页面或任务验证。

## 磁图页面与后续兼容修复

- 磁图已通过 corePlugin 可选资源 URL 接入 FlyView；仅 Maglab 构建含后端和资源，上游默认无额外页面。
- 磁图面板限制可用高度并允许滚动，考虑顶部工具区和底部避让，PIP 时隐藏；删除旧版文件对话框属性和无用屏幕坐标绑定。
- 栅格及采样统一使用 Web Mercator，避免地图缩放/旋转时依赖屏幕两角宽高；拒绝不支持的跨反经线区域，限定投影纬度。航线、航迹仍使用上游图层常量并在栅格之上，实际交互待验收。
- 此批 Android arm64 构建 exit 0（maglab-ui-compat-runner.log）；qmllint exit 0 但仍有 QGeoCoordinate 类型解析警告，不视为全量 lint 通过。未签名安装。
- 后续源码修正：CSV 保留空列，防止坐标和磁场列错位；Sketch 转 Survey 的延迟删除改用 QPointer，避免对象提前销毁后的悬空指针；旧模块 include 改为匹配 Maglab include root 的相对路径。这些后续改动须分别验证，尚未接入的模块不因磁图构建通过而视为可用。
