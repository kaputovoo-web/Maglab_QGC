# 磁测数据模拟器

脚本：`tools/maglab/magnetic_survey_simulator.py`

它只通过 UDP 发送 MagLab 航磁协议，不导入 QGC、不连接飞控、不发送 MAVLink 控制命令，也不修改任务。支持标量和矢量两种协议。

## 协议

### 标量

每帧 24 个 `+` 分隔字段，以换行结束：

```text
UU+id+time+lon+lat+alt+accuracy+clearance+
S1+S2+fluxX+fluxY+fluxZ+roll+pitch+yaw+
satellites+barometer+angularX+angularY+angularZ+accX+accY+accZ
```

### 矢量

每帧 25 个 `+` 分隔字段，以换行结束：

```text
UU+len+time+lon+lat+alt_msl+fix_type+lidar+
fg1_X+fg1_Y+fg1_Z+fg2_X+fg2_Y+fg2_Z+
gyro_X+gyro_Y+gyro_Z+acc_X+acc_Y+acc_Z+
roll+pitch+yaw+count+CRC16
```

模拟器会保留两路磁通门 XYZ 分量，并生成平滑磁异常；QGC 侧用
`sqrt(X²+Y²+Z²)` 计算两路幅值后作差用于磁图。

## 默认端口

| 模式 | QGC 本地监听 | 模拟器目标端口 |
|---|---:|---:|
| 标量 Scalar | 14551 | 14551 |
| 矢量 Vector | 13551 | 13551 |

在 QGC「设置 → 航磁通信设置」中选择对应模式；默认值会随模式切换，也可以手工覆盖。

## 启动方式

### 1. 标量模拟

终端 A：启动 macOS Maglab，进入 MagLab 磁测工作台的「采集与磁图」页面，确认模式为「标量 Scalar」。

终端 B：

```bash
cd /Users/kaput0/LIB/Code/QGC
python3 tools/maglab/magnetic_survey_simulator.py \
  --protocol scalar \
  --host 127.0.0.1 \
  --port 14551 \
  --rate 5
```

### 2. 矢量模拟

终端 A：在「航磁通信设置」中切换到「矢量 Vector」；默认监听端口会变为 13551。

终端 B：

```bash
cd /Users/kaput0/LIB/Code/QGC
python3 tools/maglab/magnetic_survey_simulator.py \
  --protocol vector \
  --host 127.0.0.1 \
  --port 13551 \
  --rate 5
```

两种模式都支持定时、后台运行和只读 SITL 数据：

```bash
# 运行 60 秒
python3 tools/maglab/magnetic_survey_simulator.py \
  --protocol vector --duration 60

# 后台运行
nohup python3 tools/maglab/magnetic_survey_simulator.py \
  --protocol scalar --host 127.0.0.1 --port 14551 --rate 5 \
  > /tmp/magnetic-simulator.log 2>&1 &

# 停止后台进程
pkill -f tools/maglab/magnetic_survey_simulator.py
```

## 常用参数

```text
--protocol scalar|vector  协议模式，默认 scalar
--host 127.0.0.1          QGC 所在机器
--port                    目标端口；省略时标量 14551、矢量 13551
--rate 5                  每秒帧数
--duration 0              0 表示一直运行到 Ctrl+C
--lat 36.0 --lon 120.0    圆形路径中心
--radius 30               路径半径，米
--altitude 50             模拟高度，米
--seed 1                  噪声种子
--quiet                   只输出错误
--mavlink udp:...         只读 SITL MAVLink 数据
```

## 使用 SITL 经纬度和姿态

SITL 启动时额外输出一个只读 MAVLink 端口给模拟器，QGC 仍使用 14550：

```bash
python Tools/autotest/sim_vehicle.py \
  -v ArduCopter -f quad -N --console --map \
  --out=udp:127.0.0.1:14550 \
  --out=udp:127.0.0.1:14552
```

用 ArduPilot 的 Python 环境运行模拟器：

```bash
cd /Users/kaput0/LIB/Code/QGC
/Users/kaput0/LIB/Code/ardupilot/.venv/bin/python \
  tools/maglab/magnetic_survey_simulator.py \
  --protocol vector --host 127.0.0.1 --port 13551 --rate 5 \
  --mavlink udp:127.0.0.1:14552
```

模拟器只读取 `GLOBAL_POSITION_INT`、`ATTITUDE`、`GPS_RAW_INT`、`VFR_HUD`，不发送控制命令。读不到 SITL 时会继续使用本地模拟参数。

QGC 不在本机时，`--host` 填 QGC 所在机器 IP，并允许对应 UDP 端口。

## 预期结果

标量模式：采集页显示 S1、S2、S1−S2，录制后生成标量 CSV。  
矢量模式：采集页显示 FG1/FG2 的 XYZ 分量，同时生成矢量 CSV；磁图和曲线使用两路幅值差。

模拟器不会发送飞控任务，也不会改变 SITL/QGC 的连接状态。
