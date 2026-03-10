# 上位机软件设计说明书（SDS）

> 工程级 C++/Qt 实时控制系统 | 串口通信协议与上位机架构设计

---

## 一、项目概述

### 1.1 目标

开发面向工业控制与算法验证的桌面软件，实现实时数据采集、控制算法计算、控制量输出、曲线显示、参数配置及串口通信等核心功能。

### 1.2 技术栈

- 开发语言：C++17
- 框架：Qt 6
- 架构模式：MVC + 模块化分层
- 通信方式：串口（QSerialPort）

---

## 二、串口帧协议设计（最优解）

### 2.1 设计目标

- **可分帧**：在连续字节流中明确帧边界
- **可校验**：抗串口噪声、丢字节
- **可扩展**：版本、消息类型、TLV 参数
- **可追踪**：序列号支持请求/应答、重发、丢帧检测
- **跨平台**：字节布局明确，Qt / MCU / Python 均可实现

### 2.2 帧结构（Serial Frame）

统一采用**小端序**。每帧结构如下：

| 字段       | 类型    | 长度 | 偏移 | 意义 |
|------------|---------|------|------|------|
| **SOF**    | 固定    | 2    | 0    | 帧起始标记，用于在流中同步。建议 `0x55 0xAA` |
| **Version**| uint8   | 1    | 2    | 协议版本，便于后续扩展不破坏兼容 |
| **MsgType**| uint8   | 1    | 3    | 消息类型（见 2.4） |
| **Flags**  | uint8   | 1    | 4    | 控制位：0x01=需 ACK，0x02=ACK 帧，0x04=分片 |
| **Seq**    | uint16  | 2    | 5    | 序列号，用于匹配请求/应答、重发 |
| **PayloadLen** | uint16 | 2  | 7    | 载荷字节数，接收端据此分帧 |
| **Payload**| bytes   | N    | 9    | 业务数据，采用 TLV 格式 |
| **CRC16**  | uint16  | 2    | 9+N  | CRC16-CCITT 校验（0x1021） |
| **EOF**（可选）| 固定 | 1    | 11+N | 可选，如 `0x0D`，便于调试 |

**各字段存在意义简述：**

- **SOF**：串口是流式传输，无边界。SOF 让接收端在丢字节或乱序后重新找到帧头。
- **Version**：协议升级时，旧设备可识别版本并拒绝/降级处理。
- **MsgType**：区分 payload 语义（设定值、遥测、命令等），解析逻辑分支依据。
- **Flags**：预留 ACK、分片、加密等扩展，避免改头部结构。
- **Seq**：串口不可靠，Seq 用于请求应答配对、重发、去重。
- **PayloadLen**：明确 payload 边界，避免靠超时或固定长度猜测。
- **Payload**：业务数据，推荐 TLV 格式，易扩展。
- **CRC16**：校验帧完整性，发现噪声或丢字节。

### 2.3 Payload 格式：TLV

每个参数采用 Tag-Length-Value 结构：

| 字段  | 类型   | 长度 | 意义 |
|-------|--------|------|------|
| **Tag**  | uint16 | 2 | 参数 ID（如 1=温度，2=压力） |
| **Type** | uint8  | 1 | 数据类型：0=float, 1=double, 2=int32, 3=uint32, 4=int16, 5=bool |
| **Len**  | uint16 | 2 | Value 字节长度 |
| **Value**| bytes  | Len | 实际值（小端） |

**TLV 优势**：可增删参数、顺序可变、旧程序可忽略未知 Tag。

### 2.4 消息类型（MsgType）

| MsgType | 值   | 方向       | 载荷           | 用途 |
|---------|------|------------|----------------|------|
| HELLO   | 0x01 | PC→设备    | 版本/能力      | 建链握手 |
| HEARTBEAT | 0x02 | 双向       | 时间戳/状态    | 保活、掉线检测 |
| SET_TARGET | 0x10 | PC→设备  | TLV（设定值）  | 下发目标值 |
| SET_PARAMS | 0x11 | PC→设备  | TLV（算法参数）| 下发 PID/MPC 参数 |
| CONTROL_CMD | 0x12 | PC→设备 | cmd(开始/停止/清零) | 控制命令 |
| TELEMETRY | 0x20 | 设备→PC  | TLV（当前值/控制值）| 遥测数据 |
| ACK     | 0x7F | 双向       | ackSeq + resultCode | 可靠传输确认 |

### 2.5 规范约定

- **字节序**：小端（Little Endian）
- **字符串**：UTF-8 + uint16 长度前缀（尽量避免长字符串）
- **CRC**：CRC16-CCITT，多项式 0x1021，初始值 0xFFFF
- **最大帧长**：建议 1024 或 2048 字节
- **超时与重发**：关键命令 `Flags & 0x01` 时需 ACK，超时 300ms 重发最多 3 次

---

## 三、上位机软件架构

### 3.1 目录结构

```
RealCtrl/
├── ui/                    # 界面层
│   ├── MainWindow.h/cpp
│   └── PlotWidget/        # 实时曲线
├── core/                  # 核心控制
│   ├── ControlManager.h/cpp
│   ├── SysInfo.h/cpp
│   └── DataBuffer.h
├── algorithm/             # 算法层
│   ├── IAlgorithm.h
│   ├── PIDAlgorithm.h/cpp
│   └── MPCAlgorithm.h/cpp
├── protocol/              # 协议层（新增）
│   ├── FrameCodec.h/cpp   # 帧编解码
│   ├── TlvCodec.h/cpp     # TLV 编解码
│   └── SerialProtocol.h/cpp
├── device/                # 设备层
│   ├── DeviceInterface.h
│   └── SerialDevice.h/cpp
└── file/                  # 文件管理
    └── FileManager.h/cpp
```

### 3.2 数据流

```
串口 RX → FrameCodec.feedBytes() → 完整帧
       → TlvCodec::decode(payload) → 参数列表
       → ControlManager / SysInfo 更新

ControlManager → SysInfo / 设定值
       → TlvCodec::encode() → payload
       → FrameCodec.encodeFrame() → QByteArray
       → SerialDevice.write()
```

---

## 四、实施步骤（怎么做）

### 4.1 第一阶段：协议基础（1–2 天）

1. **创建 `protocol/FrameCodec.h` 和 `FrameCodec.cpp`**
   - 常量：SOF、EOF、最大帧长
   - `QByteArray encodeFrame(quint8 msgType, quint8 flags, quint16 seq, const QByteArray& payload)`：组帧 + 计算 CRC
   - `void feedBytes(const QByteArray& data)`：喂入接收字节，检测完整帧
   - 使用信号 `frameReceived(quint8 msgType, quint16 seq, QByteArray payload)` 通知上层

2. **实现 CRC16-CCITT**
   - 查找表或逐字节计算，多项式 0x1021，初始 0xFFFF

3. **单元测试**
   - 编码一帧 → 解码 → 校验 payload 一致

### 4.2 第二阶段：TLV 与业务映射（1 天）

1. **创建 `protocol/TlvCodec.h` 和 `TlvCodec.cpp`**
   - `QByteArray encode(const QList<ParaNode>& list)`：ParaNode 列表 → TLV 字节流
   - `QList<ParaNode> decode(const QByteArray& data)`：TLV → ParaNode 列表

2. **定义 Tag 常量**
   - 如 `TAG_TEMPERATURE=1`, `TAG_TARGET=2`, `TAG_PID_KP=10` 等

3. **将现有 `SysForm` / `ParaNode` 与 TLV 对接**
   - `setValueList` ↔ TLV 编码/解码

### 4.3 第三阶段：串口设备封装（1 天）

1. **实现 `SerialDevice`**
   - 继承 `DeviceInterface`
   - 使用 `QSerialPort` 打开/关闭/读写
   - `readReady` 时把数据喂给 `FrameCodec.feedBytes()`

2. **连接信号**
   - `FrameCodec::frameReceived` → 解析 MsgType → 更新 SysInfo 或发送 ACK

### 4.4 第四阶段：控制循环与界面（2–3 天）

1. **ControlManager 集成**
   - 50ms 定时器：采样（串口读）→ 算法计算 → 输出（串口写）

2. **UI 更新**
   - SysInfo 变化 → 信号 → PlotWidget 刷新曲线
   - 设定值输入 → 编码 SET_TARGET → 串口发送

---

## 五、与旧 MFC 协议的关系

本协议为**全新定义**，与 `realctrl - 副本 (2)/DcsPrtcl` 的 MFC 协议**不兼容**。若需与旧上位机/设备互通，需在设备端或网关侧做协议转换；或单独保留一套 MFC 兼容编解码器，按运行配置切换。

---

## 六、版本与修订

| 版本 | 日期       | 修订内容 |
|------|------------|----------|
| 1.0  | 2025-03    | 初版：帧协议 + TLV + 实施步骤 |

---

*文档路径：`RealCtrl/上位机软件设计说明书SDS.md`*
