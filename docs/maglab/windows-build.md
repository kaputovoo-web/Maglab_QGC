# MagLab Windows 构建

本文件只描述在 **Windows 10/11 x64** 上构建 MagLab 与 NSIS 安装包的流程。当前 macOS 工作机没有 Windows Qt/MSVC，不能在本机生成可信的 Windows 安装包。

## 前置条件

- Windows 10/11 x64
- Visual Studio 2022 Build Tools（C++ 工作负载）或 Community（Desktop development with C++）
- Qt **6.11.1** MSVC 2022 64-bit，包含：
  `qtgraphs qtlocation qtpositioning qtspeech qtmultimedia qtserialport qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors qtscxml qtwebsockets qthttpserver`
- CMake 3.25+；推荐 Ninja，也可用 Visual Studio 17 2022 generator
- Python 3
- NSIS（用于安装包）
- 仓库全部 submodule

## 1. 准备源码

在 Windows 上克隆当前分支，或在 macOS 打包源码后复制到 Windows。必须包含：

- `src/Maglab/`
- `maglab/`
- `docs/maglab/`
- 当前分支的全部修改

```bat
git submodule update --init --recursive
```

## 2. 安装 Qt 与依赖

使用 Qt Online Installer 安装 Qt 6.11.1 的 `MSVC 2022 64-bit` 和上述模块。默认路径示例：

```text
C:\Qt\6.11.1\msvc2022_64
```

在仓库根目录执行依赖脚本（会安装 GStreamer；`--nsis` 安装安装器工具）：

```bat
py -3 tools\setup\install_dependencies --platform windows --nsis
```

如果尚未安装 Visual Studio，可让脚本安装 Build Tools：

```bat
py -3 tools\setup\install_dependencies --platform windows --msvc --nsis
```

安装完成后确认：

```bat
where makensis
where cmake
```

## 3. 配置

从 **x64 Native Tools Command Prompt for VS 2022** 进入仓库根目录。

### 使用 Ninja（推荐）

```bat
C:\Qt\6.11.1\msvc2022_64\bin\qt-cmake.bat ^
  -S . -B build\windows-maglab -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DQGC_CUSTOM_DIR=maglab ^
  -DQGC_BUILD_TESTING=OFF
```

### 或使用 Visual Studio generator

```bat
C:\Qt\6.11.1\msvc2022_64\bin\qt-cmake.bat ^
  -S . -B build\windows-maglab -G "Visual Studio 17 2022" -A x64 ^
  -DQGC_CUSTOM_DIR=maglab ^
  -DQGC_BUILD_TESTING=OFF
```

保持 `QGC_ENABLE_GST_VIDEOSTREAMING=ON`（默认），以保留 GStreamer 视频能力。Windows x64 原生构建不要关闭该选项。

## 4. 构建

Ninja：

```bat
cmake --build build\windows-maglab --parallel
```

Visual Studio generator：

```bat
cmake --build build\windows-maglab --config Release --parallel
```

可执行文件通常在：

```text
build\windows-maglab\Maglab.exe
build\windows-maglab\Release\Maglab.exe
```

实际路径以 CMake 输出为准。

## 5. 生成 NSIS 安装包

```bat
cmake --install build\windows-maglab --config Release
```

输出通常为：

```text
build\windows-maglab\Maglab-installer-AMD64.exe
```

若使用 Visual Studio 多配置生成器，确保 `--config Release` 与已构建配置一致。`makensis` 必须在 PATH 中；缺失时 `cmake --install` 会失败。

## 6. 品牌图标

`maglab/cmake/CustomOverrides.cmake` 目前设置了 macOS 图标和 Android 包名，但 **没有覆盖 Windows 图标**，默认仍会使用 QGC 的 `WindowsQGC.ico`。正式发布前应准备 MagLab `.ico` 并配置：

```bat
-DQGC_WINDOWS_ICON_PATH=C:/path/to/maglab.ico
```

可同时覆盖安装器头图：

```bat
-DQGC_WINDOWS_INSTALL_HEADER_PATH=C:/path/to/maglab-installheader.bmp
```

## 7. 验证

1. 运行 `Maglab.exe`，确认品牌、中文、MagLab 工具栏入口和设置页。
2. 确认 GStreamer 插件被安装到 `lib\gstreamer-1.0`，视频设置可使用。
3. 在 Windows 上实际打开 LLM／Sketch 编辑器、磁测工作台、磁图库和相机页面。
4. 运行安装器，确认安装目录、开始菜单、卸载程序。
5. 对正式分发版本使用 Windows 代码签名证书签名 `.exe` 和安装器；本项目没有内置 Windows 签名证书。

## 当前限制

- 没有在 Windows 上实际编译和验收本分支。
- Windows 图标尚未切换为 MagLab。
- 未生成 Windows 安装包；macOS 交叉编译需要额外的 MinGW／Wine／Windows GStreamer 环境，不属于当前支持路径。
- 不把未在 Windows 真机验证的 exe 宣称为正式发布版。
