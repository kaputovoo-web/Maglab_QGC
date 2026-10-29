# Maglab 开发与原版验证

## 已确认范围

- 名称：Maglab；用户提供的彩色校徽已从 `/Users/kaput0/LIB/Code/icon/校徽（彩色）.svg` 导入 `maglab/res/maglab-emblem.svg`。Android 品牌测试包使用该校徽启动图标及启动页图标，内部 QGC 图形尚未全面替换。
- 品牌配置位于 `maglab/`，通过 `-DQGC_CUSTOM_DIR=maglab` 显式启用，不改变默认原版构建。Android 本地测试包名为 `org.maglab.groundcontrol.test`，正式发布包名待确认。
- 飞控：以 ArduPilot 为主，原版验证阶段保留全部上游插件。
- 平台：macOS Apple Silicon + Android；目标遥控器为云卓 H16、G30，具体硬件/固件待确认。
- 3D View：先验证桌面原版，再单独适配 Android，不在原版验证中解除上游限制。

## Application 设置英文残留修复（最新）

- 用户反馈设置详情仍有英文后，检查实际源码覆盖率，发现原翻译目录漏收当前源码字符串。补入 51 条设置 JSON 上下文消息，以及 25 条 AppSettings QML 消息，涉及内置串口、视频自动重连/RTP 抖动延迟、NTRIP GGA/TLS、Remote ID、遥测流率、定位来源与模拟设备设置。
- 当前源码目录 4072 条、JSON 目录 1872 条。设置 JSON 的标题/标签/说明/枚举字面量覆盖检查，以及 AppSettings 顶层 QML 简单 qsTr 字面量覆盖检查通过；此检查不等于整个项目字符串覆盖率证明。
- 已重新构建并签名 `build/android-maglab-arm64/test-apk/Maglab-5.1.5-arm64-settings-zh-test.apk`；构建成功，签名和对齐检查通过。SHA-256：`27c2cc4456d62836c4783ebc902d7202765c672afc72522bc9a8d5b3eae31399`。用户在安全确认提示后要求覆盖安装，已通过 `adb -s 192.168.137.224:42567 install -r` 安装此修复包，返回 Success；重新启动返回 Status: ok，随后 PID 为 23715。Maglab 更新时间为 2026-10-27 13:16:43，原 QGC 仍为 5.1.4、更新时间不变。近期日志未匹配致命崩溃标记，证据为 `settings-zh-installed-logcat.log` 和 `settings-zh-installed-screen.png`（位于 Android 构建目录）。设置逐页显示效果仍待用户真机核对。

## 新图标与中文测试包（此前打包）

- 按用户指定使用 `/Users/kaput0/LIB/Code/MagLab/图标.png`，原图逐字节复制为 `maglab/res/maglab-icon.png`；替换 Android 六档启动图标、启动页图标及自适应图标前景。自适应前景保留安全边距，避免圆形遮罩裁掉主体。旧 SVG 仅保留为历史素材。
- 生成 `maglab/res/maglab.icns` 并通过品牌 overlay 设置 `QGC_MACOS_ICON_PATH`；本轮只重新打包 Android，尚未重建 macOS App。
- 全目录检查发现并修复 22 条历史枚举译文的非英文分隔符及部分术语（如开伞、释放、真假值）；修复串口权限提示丢失 HTML 和误译 shell 命令的问题。上游虽有兼容中文分隔符的逻辑，目录仍统一采用英文逗号。罗盘说明按实际机头朝上行为修正英文源文及中文译文。
- 全部 4047 条源码消息和 1821 条 JSON 消息均无 unfinished；所有非空源文有译文，全目录 Qt/地图占位符、HTML 标签（含 pre）和枚举数量检查通过。构建实际重新编译并嵌入两份中文 QM。
- Android 配置、增量构建与 Gradle 打包成功：`configure-icon-zh.exit=0`、`build-icon-zh.exit=0`；日志位于 `build/android-maglab-arm64/`。
- 新 APK：`build/android-maglab-arm64/test-apk/Maglab-5.1.5-arm64-icon-zh-test.apk`（约 85 MiB），SHA-256：`ba7a66757bc8f49aa57995ba9d30f5f4b779e04f8237e97ca7c0bf33398de307`。
- 本地 debug 测试签名验证、ZIP 对齐检查、包信息检查通过：Maglab / `org.maglab.groundcontrol.test` / 5.1.5 / arm64-v8a / min 28 / target 36。APK 经 Android 资源优化后，全部 13 张品牌 PNG 与输入逐像素一致。
- 用户确认没有飞行任务或活动链路后，已向 `192.168.137.224:42567` 执行 `adb install -r`，返回 Success；保留应用数据并重新启动，启动返回 Status: ok，随后 PID 为 8391。Maglab 更新时间为 2026-10-27 12:30:58；原版 QGC 仍为 5.1.4，更新时间 2026-09-10 11:13:56，未被覆盖。
- 启动截图和近期日志保存为 `build/android-maglab-arm64/icon-zh-installed-screen.png`、`icon-zh-installed-logcat.log`；所采集日志未匹配到 FATAL EXCEPTION / Fatal signal。不代表完整真机图标、中文排版、飞控、视频或 3D 回归已通过。未执行完整 lint/单元测试。

## 全量汉化目录进展（最新）

- 两份简体中文目录已补齐：`qgc_source_zh_CN.ts` 共 4047 条（其中 1 条为空源文），`qgc_json_zh_CN.ts` 共 1821 条；均无 `unfinished`，所有非空源文均有译文。
- 协议、型号、品牌、参数 ID、单位符号及纯格式字符串经检查保留必要原文，并非将英文描述标记为已完成。另修正历史误译：日志图表的 Current 为“当前值”，返航高度摘要中的 current 为“当前高度”，而非“电流”。
- 两份目录的 `lrelease` 编译通过；消息键集合、Qt 和地图模板占位符、JSON 枚举逗号数量检查通过。富文本检查需区分实际 HTML 与 `<None>`、`<Untitled>` 等普通占位标签，不能仅用任意尖括号匹配判断。
- **目录补齐不等于全面验收完成**：仍需审校旧译文、检查源码和 JSON 的翻译接入、验证界面布局与英文残留，并重新构建和签名 APK。设备上仍为较早的品牌测试版，尚未安装本轮全部译文。完整 lint、单元测试及真机回归尚未执行。
- 罗盘 `Lock Compass Nose-Up` 标签与此前上游英文说明（声称北向上）存在语义冲突；实际 QML 通过 `-_heading` 旋转罗盘实现机头朝上。本轮已同步修正英文源文与中文译文，未改动设置行为。

## 固定基线

- 上游：https://github.com/mavlink/qgroundcontrol
- 标签：`v5.1.5`（初始化时查询到的最新 5.x 正式标签，不使用 `v5.2.0-dev`）
- 提交：`3a67d31f0c36bf3fe38ec52970d250a89d0aaf67`
- 工作分支：`maglab/qgc-5.1.5`
- 未修改上游功能源码；本阶段应用仍叫 QGroundControl。
- 该基线通过 CPM 获取依赖，本次 `git submodule status` 为空。部分依赖跟踪分支，标签本身并不保证依赖完全可复现；验证成功后应记录实际依赖提交。

## 构建版本依据

以 [build-config.json](../../.github/build-config.json) 为准：

| 组件 | 基线要求 |
| --- | --- |
| Qt | CI 使用 6.11.1；最低 6.11.0 |
| Qt 模块 | 见 build-config.json 的 qt.modules，包括 Quick3D、Location、Graphs 等 |
| CMake | >= 3.25 |
| GStreamer macOS / Android | 1.28.4 |
| Android compile / target SDK | 36 |
| Android 最低 SDK | 28，即 Android 9 |
| Android Build Tools | 36.0.0 |
| Android NDK | 27.2.12479018（r27c）|
| Java | CI 使用 JDK 21 |
| macOS 最低部署版本 | 13.0 |

Android target Qt 与 host Qt 应使用匹配版本。不能将 Homebrew 的桌面 Qt 当成 Android Qt 使用。

## 最新进展（后续状态优先于初始检查）

- JDK 21 已通过 Homebrew 安装：`/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`，未修改全局默认 Java。Homebrew 安装同时升级了其依赖库，并自动清理了旧版本缓存。
- GStreamer 1.28.4 runtime + devel 已下载并按 build-config.json 固定 SHA-256 校验，解包合并在 `.cache/CPM/gstreamer-mac-1.28.4/root`，未覆盖系统 Framework。合并时处理了多个子包中重复的符号链接。
- Qt 6.11.1 macOS 与 Android arm64 SDK 及所需模块均已安装到 `.cache/Qt/6.11.1/`。aqt 查询 Android 包时使用 `all_os android`，不是 `mac android`。
- 原先 `build/macos-original` 的 Homebrew Qt 6.11.0 配置已通过，但编译遇到 Qt Location 私有头文件 `QGeoTileSpec` 的 qHash 静态断言，不能标记为编译成功。
- 已切换官方 Qt 6.11.1，在 `build/macos-qt6111` 新建构建，配置与编译通过（`build.exit=0`）。实际 App 为 `build/macos-qt6111/Release/QGroundControl.app`，不要误用同级部署辅助目录 `QGroundControl.app`。
- 已直接启动 App，日志在 `build/macos-qt6111/run.log`，启动时出现 GStreamer Python 插件缺少 `gi`、QML 属性覆盖及 macOS 输入法警告；尚未完成 GUI、视频与 3D 功能验收。
- Android 原版配置与构建现已通过：`configure.exit=0`、`build-retry.exit=0`。早期失败包括 SDK 下载中断，以及一次未带完整参数的重跑误入主机配置；已用完整 Android toolchain 参数纠正并核实缓存。
- Android GStreamer 1.28.4 通用 SDK 使用 curl 重试下载到 `.cache/CPM/gstreamer-android/gstreamer-android-1.28.4.tar.xz`，实际 SHA-256 为 `a48aeb1b4fbae67a2fe0a0daa55af4a6af22b48f15a9c09f09dcd9ab3e8e943e`，与仓库固定值一致。首次手工校验命令因虚拟环境 Python 不支持 `hashlib.file_digest` 失败，随后改用分块 SHA-256 校验成功。
- C++ 首轮编译与链接通过，Gradle 首次打包因 Google Maven / Maven Central TLS 握手中断而失败（`build.exit=14`）；未修改依赖或关闭 TLS 校验，直接重试后 `BUILD SUCCESSFUL`，日志为 `build/android-original-arm64/build-retry.log`。
- 已生成约 84 MiB 的未签名 Release APK：`build/android-original-arm64/android-build/build/outputs/apk/release/android-build-release-unsigned.apk`，部署副本为 `build/android-original-arm64/android-build/QGroundControl.apk`。APK SHA-256 为 `d9e12e5f1fbefe8ad9d672208cabfa207757deb9cee11cc384aae795773fd293`。
- `aapt dump badging` 已确认包名 `org.mavlink.qgroundcontrol`、版本 `5.1.5`、ABI `arm64-v8a`、最低 API 28、目标/编译 API 36。`apksigner verify` 确认当前 APK 未签名，不能直接用于安装验证；后续需本地测试签名，正式发布密钥另行管理。构建有上游 Java 废弃 API 和 Gradle 兼容性警告。
- 长任务以后台方式执行，避免单次工具调用超时中断；各目录的 `configure.log` / `build.log` 为实际日志，`configure.exit` / `build.exit` 在任务完成后记录退出码（0 才表示相应步骤成功）。缺少退出码不表示成功，可结合进程状态检查。
- 已使用本机现有 `~/.android/debug.keystore`（alias `androiddebugkey`）生成本地测试签名 APK：`build/android-original-arm64/test-apk/QGroundControl-5.1.5-arm64-local-test.apk`，未覆盖原未签名 APK，未将密钥复制进仓库。仅用于本地测试，不是正式发布签名。
- 测试 APK 的 `apksigner verify --verbose --print-certs` 通过（v3），`zipalign -c -P 16 -v 4` 通过；ZIP 对齐检查不等于全部原生库或设备的 16 KiB 页兼容验证。日志在同目录 `signature-verification.log`、`zipalign-verification.log`。APK SHA-256 为 `2aee6bcf534772196f746fa77d572a1ed812faa4de3adae4a05bdd64686c5e60`，另存 `SHA256SUMS`。
- 连接兼容设备后可执行 `adb install -r build/android-original-arm64/test-apk/QGroundControl-5.1.5-arm64-local-test.apk`。若设备上已有相同包名但其他签名的 QGC，不能直接覆盖安装；卸载会丢失应用数据，应先备份并确认，不能自动卸载。
- macOS App 已生成并发起启动；Android 本地测试签名 APK 已生成。`adb devices -l` 最近检查无设备，安装/运行、飞控、视频、3D、真机与单元测试均未验证。

复用已安装 Qt 的命令：

```sh
PATH="$PWD/.venv/bin:$PATH" .venv/bin/python tools/setup/install_qt.py install \
  --version 6.11.1 --host mac --target desktop --arch clang_64 \
  --outdir .cache/Qt --modules "$(python3 tools/setup/read_config.py --get qt.modules)"
PATH="$PWD/.venv/bin:$PATH" .venv/bin/python tools/setup/install_qt.py install \
  --version 6.11.1 --host all_os --target android --arch android_arm64_v8a \
  --outdir .cache/Qt --modules "$(python3 tools/setup/read_config.py --get qt.modules)"
```

当前 macOS 6.11.1 构建配置还指定了：

```sh
-DCMAKE_PREFIX_PATH="$PWD/.cache/Qt/6.11.1/macos"
-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
-DGStreamer_ROOT_DIR="$PWD/.cache/CPM/gstreamer-mac-1.28.4/root"
-DGStreamer_USE_FRAMEWORK=OFF
-DPKG_CONFIG_EXECUTABLE=/opt/homebrew/bin/pkg-config
```

## Maglab Android 并存测试版

- 配置：`maglab/cmake/CustomOverrides.cmake` 仅覆盖应用名和 Android 包名，保留全部上游插件、视频及 Android 3D 限制；未复制示例中的禁用 ArduPilot 配置。
- 构建目录：`build/android-maglab-arm64`；`configure.exit=0`、`build.exit=0`。
- 测试签名 APK：`build/android-maglab-arm64/test-apk/Maglab-5.1.5-arm64-local-test.apk`。
- APK SHA-256：`b26cfd4d753e7509aa9ae18fc2dcbb18d9b7b58dbc4bc688095630e0e5f03635`。
- 已检查 APK 标签为 `Maglab`、包名为 `org.maglab.groundcontrol.test`，签名与 ZIP 对齐验证通过。
- Wi-Fi ADB 设备上报 Android 14 / API 34、`arm64-v8a`、`Lahaina for arm64`；尚未确认其具体遥控器商品型号。
- 原版 5.1.5 测试 APK 覆盖现有 QGC 5.1.4 时签名不匹配，已停止，未卸载原应用。Maglab 独立包安装成功，原 QGC 5.1.4 及其更新时间保持不变。
- Maglab `am start -W` 返回 `Status: ok`，15 秒后进程仍在；采集日志未匹配到致命异常，但不能等同完整运行验证。
- 启动截图 `build/android-maglab-arm64/startup-screen.png` 显示系统“所有文件访问权限”页面，应用名 `Maglab`、版本 `5.1.5`，权限开关尚未开启。未自动授予此广泛权限，需用户决定；主界面与视频/飞控/3D 待验证。
- 启动日志 `build/android-maglab-arm64/startup-logcat.log` 有存储权限未授予、回退内部存储和上游 QML 属性覆盖警告。

## 品牌 / Android 3D 实验包（待安装验证）

- 用户反馈此前 Maglab 已进入主界面，飞控连接和视频正常；这是用户确认，尚未进行自动化回归测试。
- Android 资源位于 `maglab/android`，CMake 将上游模板与品牌资源合并到构建目录，不改上游图标；包含各密度启动图标、启动页图标和自适应图标。
- `MAGLAB_ANDROID_3D_EXPERIMENTAL` 仅由 Maglab overlay 定义：开放 Android 3D 设置页和启用选项，不改变上游构建行为。默认不开启 3D；解除限制不等于完成 GPU/驱动适配，需台架验证。
- 首批简体中文补齐了 3D 说明、常用设置标签、视频源、设置页面名等，仍有大量未翻译项，不能称为完整汉化。两份 TS XML 解析与 `git diff --check` 通过。
- 构建 `build-branding.exit=0`；未运行完整 lint / 单元测试。测试 APK 签名与 ZIP 对齐验证通过。
- APK：`build/android-maglab-arm64/test-apk/Maglab-5.1.5-arm64-branding-3d-test.apk`（约 85 MiB）。SHA-256 记录在同目录 `branding-SHA256SUMS`。
- 未覆盖安装或重启设备上的应用，以免打断已连接的飞控/视频。请先确认无飞行任务、飞行器未解锁，再更新并测试；测试中可关闭 3D 回退。

## 简体中文专项（进行中）

按用户要求暂停其他功能开发，先补齐简体中文。使用标准 Qt Linguist 链路：QML/C++ 的 `qsTr()` / `tr()` 对应 `translations/qgc_source_zh_CN.ts`，JSON 元数据对应 `translations/qgc_json_zh_CN.ts`；构建时由 `lrelease` 生成 QM。保留翻译上下文、占位符、HTML、协议标识与枚举顺序，不直接改英文源码或只移除 unfinished 标记。

此前阶段统计（已由上方最新进展取代）：源文件目录有 4047 条消息、当时仍有 2363 条未完成或空译文；JSON 目录有 1821 条、当时仍有 881 条未完成。已补齐主窗口的退出/未保存/参数更新提示，以及设置导航、视频源等文本；随后继续补齐 83 条，包括引导飞行操作说明、多载具解锁/上锁、强制解锁警告、参数编辑器、任务规划载具切换提示、语言/配色/缩放说明、离线载具类型与速度、飞行前检查和飞行视图设置。相对 HEAD 新增的全部已完成译文已检查 Qt 占位符和 JSON 枚举数量，未发现不一致。再补齐 76 条 ArduPilot 失效保护/电池/传感器摘要、潜航器机架确认、串口/TCP 错误和视频设置说明（流地址、画面适配、录像存储、低延迟、解码器选项）。两份 TS 已通过 lrelease 编译，`git diff --check` 通过；尚未重新打包或在设备验证此轮翻译。又补齐 90 条，包括主菜单与参数编辑器标签、3D 切换按钮、应用设置说明、地图服务令牌说明、跟随与数据存储选项、飞行视图和规划设置。重新运行占位符/枚举检查与 lrelease 均通过。随后新增 90 条自动连接、遥测日志、MAVLink 转发/心跳、RTK 自测定位/固定基站、电池指示器、日志回放与 NTRIP 错误提示译文；占位符、枚举及 lrelease 检查通过。按全量目标继续：从官方 v5.0.8/v5.0.0/v4.4.3 中文目录按上下文、原文、消歧信息精确复用 78 条有效中文，并新增 203 条测绘/扫描/相机、ArduPilot 安全与失效保护、摇杆、自动飞行模式、ADS-B/NTRIP 等译文。新增 `tools/translations/reuse_zh_cn.py`，默认仅预览，使用 --apply 才应用；不覆盖已完成译文，不复用纯英文或占位符/富文本不兼容的内容，检查 XML 除目标译文外不变。参考文件仅缓存于 build/translation-reference。历史目录存在明显误译（例如 current 的多义性），不得仅凭原文相同跨上下文自动采用。当前占位符/枚举、lrelease、脚本语法和 diff 检查通过，未重新构建 APK 或进行真机验收。本轮又完成 511 条（源码 308 条、JSON 203 条），覆盖远程识别法规分类与广播、PX4 安全配置、ArduPilot 调参、蓝牙/BLE、MAVLink 签名密钥安全提示、GPS/RTK 状态、摇杆详情、固件升级、Lua 脚本、传感器校准、电机测试、图像地理标记、离线地图以及视频驱动规避说明和搜索关键词。消息键集合、Qt/地图模板占位符与 JSON 枚举数量校验通过，lrelease 编译和 diff 检查通过。未构建或安装 APK，未执行完整 lint、单元测试及真机验收。不能宣称完整汉化，后续继续按页面翻译、检查占位符和真机排版。

## 本机初始检查结果

- macOS 15.7.5，arm64，已选择 Xcode。
- CMake 4.4.2、Ninja 可用。
- Homebrew Qt 6.11.0 可被 CMake 发现，所需 Qt 查找步骤通过。
- Qt Creator 配置中另有 `/Library/kaputo/soft/QT/6.10.3` 的 Qt 路径，需要安装匹配基线的新 Android SDK；不复用旧 Android Qt。
- Android SDK 已有 platform 36、Build Tools 36.0.0、NDK 27.2.12479018。
- 注册的 JDK 为 17、25，尚需 JDK 21。
- GStreamer Framework 1.28.1、Homebrew GStreamer 1.28.2，均低于基线要求。
- `adb devices -l` 未发现设备，尚未进行 H16/G30 验证。
- 检查时剩余磁盘空间约 54 GiB，安装多套 Qt、下载 GStreamer、双平台构建前需留意空间。

## macOS 原版配置

在仓库根目录执行：

```sh
cmake -S . -B build/macos-original -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/opt/homebrew \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DQGC_MACOS_UNIVERSAL_BUILD=OFF \
  -DQGC_BUILD_TESTING=OFF \
  -DQGC_MACOS_SIGN_WITH_IDENTITY=OFF \
  -DQT_QML_GENERATE_QMLLS_INI=OFF

# 仅配置成功后执行：
cmake --build build/macos-original --parallel 4
open build/macos-original/Release/QGroundControl.app
```

这里选择本机 arm64 而非发布用 universal，保留视频和 3D 等原版功能。使用其他 Qt 安装时更换 `CMAKE_PREFIX_PATH`。

第一次配置自动创建的 `.venv` 缺少 pip，已执行以下命令修复：

```sh
.venv/bin/python -m ensurepip --upgrade
```

**初次尝试**在 `gstreamer-1.0>=1.28.4` 处失败，后已补齐隔离 SDK；进一步编译结果见“最新进展”。不要降低版本约束或关闭视频来宣称原版验证通过。

## Android 原版构建准备

安装匹配的 Qt 6.11.1 host/macOS 与 Android target（先 arm64-v8a，设备要求时再增加 armeabi-v7a），以及 JDK 21。配置参数对齐 [.github/workflows/android.yml](../../.github/workflows/android.yml)：

```sh
# 按实际安装路径设置，不要直接使用占位路径。
export JAVA_HOME="/path/to/jdk-21/Contents/Home"
export ANDROID_HOME="$HOME/Library/Android/sdk"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export ANDROID_NDK_ROOT="$ANDROID_HOME/ndk/27.2.12479018"
export QT_HOST_PATH="/path/to/Qt/6.11.1/macos"
export QT_ANDROID_PATH="/path/to/Qt/6.11.1/android_arm64_v8a"

cmake -S . -B build/android-original-arm64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$QT_ANDROID_PATH/lib/cmake/Qt6/qt.toolchain.cmake" \
  -DCMAKE_PREFIX_PATH="$QT_ANDROID_PATH" \
  -DQT_HOST_PATH="$QT_HOST_PATH" \
  -DQT_ANDROID_ABIS=arm64-v8a \
  -DQGC_BUILD_TESTING=OFF \
  -DQT_QML_GENERATE_QMLLS_INI=OFF
cmake --build build/android-original-arm64 --parallel 4
```

此为待执行模板，未验证。APK 安装前需确认打包输出、使用开发签名；不得将生产签名私钥或密码提交到仓库。

## H16 / G30 设备确认

USB 调试开启后执行：

```sh
adb devices -l
adb -s SERIAL shell getprop ro.product.model
adb -s SERIAL shell getprop ro.build.version.release
adb -s SERIAL shell getprop ro.build.version.sdk
adb -s SERIAL shell getprop ro.product.cpu.abilist
adb -s SERIAL shell getprop ro.opengles.version
adb -s SERIAL shell dumpsys SurfaceFlinger
adb -s SERIAL shell wm size
adb -s SERIAL shell wm density
```

`SERIAL` 替换为设备序列号；SurfaceFlinger 输出用于辅助检查 GPU/驱动，必要时还需运行图形测试。请另确认：

- 遥控器硬件版本、系统固件版本，最好提供“关于设备”截图。
- 数传接入方式：内置 UART、USB 串口、UDP/TCP 或厂商 SDK；端口、波特率或 SDK 文档。
- 图传协议与测试地址，编码 H.264/H.265、分辨率。
- 是否要求应用读取摇杆/按键，还是遥控链路直接控制飞控。

若设备 API < 28，则不能按当前基线的官方最低要求直接部署，必须另作兼容性评估，而不是只调低 manifest 的最低版本。

## Android 3D View 单独立项

上游 [3D View 文档](../en/qgc-user-guide/viewer_3d/viewer_3d.md) 明确说明 Android 暂不支持，原因是 GPU/驱动渲染可靠性。

限制入口：`src/API/QGCCorePlugin.cc` 的 `adjustSettingMetaData()`，Android 下隐藏并关闭 `Viewer3DSettings.enabled`。`src/Viewer3D` 本身已参与跨平台构建，不代表 Android 可可靠使用。

建议顺序：

1. 原版 Android 的连接、遥测、视频、前后台切换稳定。
2. 在独立实验配置中开放 3D，默认关闭，保留 2D 回退。
3. 在每种实际遥控器上测试渲染后端、触摸手势、OSM 文件导入及权限。
4. 验证 3D 与视频并发下帧率、内存、温升和稳定性，再决定支持范围。
5. 仅在模拟器或安全台架验证飞控数据；图形适配完成前不用于实际飞行控制。

## 后续品牌定制

原版基线验证后使用独立 custom overlay / 构建目录设置 Maglab 名称、应用标识、图标和资源，不全局替换 QGroundControl 源码标识。包名和组织域名待确认，避免先虚构标识。

Logo 阶段需要 SVG 或高分辨率透明 PNG（建议 1024×1024）、深浅背景版本；当前暂不需要。发布时保留上游许可证、版权及第三方声明，并核实所采用许可证的分发义务。

## 本轮可安装实验包：未校准地形预览

增加 Fact `terrainPreview`（默认关闭），3D 页也提供开关。开启时查询 32×32 单元 DEM，使用中间样本作为局部可视化零点，并绑定建筑地面采样。该零点不是 AMSL，不是垂直基准转换：因此隐藏载具、任务和高度剖切，避免混合高度产生误导。关闭恢复旧平面场景。超时/失败退回平面，未知区域不画地形贴地建筑。OSM 建筑仍需用户已有 OSM 文件，未新增在线建筑下载。

Android 构建、签名校验、APK 对齐及 diff 空白检查通过。APK：`build/android-maglab-arm64/test-apk/Maglab-5.1.5-arm64-terrain-preview-test.apk`，SHA256 `7f97f6162b754d496aae953588e05c4b20d18e879a1249e67db88370d529a4ff`。新增测试未运行、材质真机未验收。尚未安装：需再次确认无飞行任务且活动飞控链路已断开。严格的 DEM/飞控基准统一仍未完成，不将预览宣称为统一海拔系统。

## 3D 顶部控件避让状态栏

FlyView 加载 3D 场景时，通过动态 binding 传入可见 toolbar.height；地形状态和预览开关分别在此 inset 下增加字体相关边距，避免被顶部状态栏覆盖。保留全屏三维视口，不裁切场景或改变相机尺寸。Android 构建、签名、对齐和 diff 检查通过，真机布局待验证；未安装。包 `Maglab-5.1.5-arm64-3d-overlay-inset-test.apk`，SHA256 `78f1192d7dd68ffaf57a96ffdbfc2aa39fa5a787f349a108182df4aaeaa5f2f5`。

## 高程证据与任务动态更新

垂直基准证据及缺口整理于 [elevation-datum-evidence.md](elevation-datum-evidence.md)。已取得 Copernicus 官方 EGM2008/米制及 DSM 定义，但尚不能证明 Auterion 输出和飞控采用相同基准。因此保护保持启用，无新安装。

任务场景换算不再回退使用未解析的任务 raw 高度：统一先取 amslEntryAlt/amslExitAlt，再减场景参考或计划 Home 海拔；缺失则省略。航段起点使用前项出口高度。监听每项高度、坐标、序号、命令及列表变更，以零周期单次 Timer 合并事件重建；Home 变化也触发更新。控制器清空时先清旧点，避免旧任务残留。载具缺失要求的高度时隐藏，不静默切换到另一高度框架。启动点识别不再依赖翻译缩写。

Android 构建和 diff 检查通过；上述动态监听及异常状态尚未真机/自动化运行验证，不能宣称四项已经完整交付。任务高度值的源数据仍受未知 DEM 基准影响。

## 简易 3D 范围收敛

按用户确认，将原 terrainPreview 开关在 3D 页面命名为“简易三维地形视图”。保留卫星 DEM 与姿态模型，隐藏任务/建筑/剖切。模型高度取 Home 处的局部 DEM 高度加飞控相对 Home 高度（米），仅近似局部对齐，不是 AMSL 基准转换。Home 无效、在覆盖外、DEM 未就绪、相对高度缺失或 VehicleLinkManager 报通信丢失时隐藏模型；常驻“局部相对高度示意、不用于避障或净空判断”提示。多机分别查询各自 Home，但仍共用局部场景覆盖。

Android 最终增量构建成功、diff 检查通过；qmllint 退出 0 但存在类型解析/访问限定警告，不称全 lint 通过。未运行新增 C++ 测试，未进行真机高度/交互回归，未签名安装。现有高度 bias 仍沿用用户设置，局部地面采样为三角面近似；Home 不一定等于真实起飞地面，因此只作示意，不能标为精确 AGL。

## 简易模型改用 AMSL 变化量

用户希望采用海拔并独立显示地形。本轮模型由相对高度 Fact 改为 `local DEM(Home) + altitudeAMSL.rawValue - homePosition.altitude`。这是以 Home 为锚点的海拔变化量可视化，不是未经验证的 DEM/AMSL 绝对对齐，也不代表 Home 精确位于裸地。缺少 Home 海拔或载具 AMSL 时只隐藏模型，地形加载保持独立。提示同步更新，未开放净空判断。

Maglab 专属编译宏下 terrainPreview 的默认值改为 true，仅影响未保存该设置的用户；已保存 false 仍保留选择，可用 3D 页开关开启。上游默认值仍 false，3D viewer 总开关仍默认关闭。地形仍需有效地理位置、在线/缓存数据和查询成功；不是无限区域、离线无数据也能显示。Android 构建/diff 检查通过，未签名安装，运行/高度回归待做。
