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

### 2.6 一帧输入/输出模版：MsgType ↔ TlvType ↔ SysForm 对照表

本节给出「一帧」在收发两侧的**业务模版**：哪种 MsgType 带哪些 TLV（TlvType），对应上位机业务结构（SysForm / ParaNode）的哪一部分。编码时从业务构造 TlvItem 列表再组帧；解码时按 MsgType 分支，从 payload 解析 TlvItem 再写回业务。

#### 2.6.1 整帧结构（收发共用）

| 字段 | 类型 | 长度(字节) | 偏移 | 说明 |
|------|------|------------|------|------|
| SOF | 固定 | 2 | 0 | `0x55 0xAA` |
| Version | uint8 | 1 | 2 | 协议版本，如 1 |
| MsgType | uint8 | 1 | 3 | 见下表 |
| Flags | uint8 | 1 | 4 | 0x01=需 ACK，0x02=ACK 帧 |
| Seq | uint16 | 2 | 5 | 序列号 |
| PayloadLen | uint16 | 2 | 7 | N = Payload 字节数 |
| Payload | bytes | N | 9 | TLV 列表（见下） |
| CRC16 | uint16 | 2 | 9+N | CRC16-CCITT |

**头部固定 11 字节**；整帧长度 = 11 + N。

#### 2.6.2 TlvType 常量（与 protocol/TlvCodec 一致）

| 分类 | TlvType 名称 | 值 | 数据类型 | 说明 |
|------|--------------|-----|----------|------|
| 控制/设定 | TargetPosition | 0x0001 | float32 | 目标位置 |
| | TargetSpeed | 0x0002 | float32 | 目标速度 |
| | TargetTorque | 0x0003 | float32 | 目标力矩 |
| | Mode | 0x0004 | uint8 | 运行模式 |
| | Enable | 0x0005 | uint8 | 使能 |
| | Kp | 0x0006 | float32 | PID P |
| | Ki | 0x0007 | float32 | PID I |
| | Kd | 0x0008 | float32 | PID D |
| | MaxOutput | 0x0009 | float32 | 输出限幅 |
| 状态/监测 | FeedbackPosition | 0x0101 | float32 | 反馈位置 |
| | FeedbackSpeed | 0x0102 | float32 | 反馈速度 |
| | FeedbackTorque | 0x0103 | float32 | 反馈力矩 |
| | Temperature | 0x0104 | float32 | 温度 |
| | Voltage | 0x0105 | float32 | 电压 |
| | Current | 0x0106 | float32 | 电流 |
| | StatusFlags | 0x0107 | uint32 | 状态位 |
| 系统/通用 | ErrorCode | 0x0201 | uint16 | 错误码 |
| | TextMessage | 0x0202 | UTF-8 串 | 文本消息 |
| | TimestampMs | 0x0203 | uint64 | 时间戳(ms) |
| | HeartbeatIntervalMs | 0x0204 | uint32 | 心跳间隔(ms) |

#### 2.6.3 上位机 → 设备（输出帧模版）

| MsgType | 值 | Payload 建议 TlvType | 对应 SysForm / 业务 | 用途 |
|---------|-----|----------------------|----------------------|------|
| HELLO | 0x01 | TimestampMs, HeartbeatIntervalMs（可选） | 建链参数 | 建链握手 |
| HEARTBEAT | 0x02 | TimestampMs, StatusFlags（可选） | — | 保活 |
| SET_TARGET | 0x10 | TargetPosition / TargetSpeed / TargetTorque 等 | **setValueList**（目标类 ParaNode） | 下发目标值 |
| SET_PARAMS | 0x11 | Kp, Ki, Kd, MaxOutput, Mode, Enable 等 | **setValueList**（参数类 ParaNode） | 下发 PID/算法参数 |
| CONTROL_CMD | 0x12 | Enable 或自定义 cmd（uint8） | 控制命令 | 启/停/清零 |

**输出一帧流程**：按上表从 SysForm / 设定值 / 参数 构造 `QVector<TlvItem>` → `Tlvcodec::encodeItems(items)` 得 payload → `FrameCodec::encodeFrame(msgType, flags, seq, payload)` 得到可发送的一帧。

#### 2.6.4 设备 → 上位机（输入帧模版）

| MsgType | 值 | Payload 建议 TlvType | 对应 SysForm / 业务 | 用途 |
|---------|-----|----------------------|----------------------|------|
| TELEMETRY | 0x20 | FeedbackPosition, FeedbackSpeed, FeedbackTorque, Temperature, Current, StatusFlags 等 | **sampleValueList**（当前值）、**ctrlValueList**（控制量） | 遥测，更新 SysInfo / 曲线 |
| ACK | 0x7F | ackSeq + resultCode（约定 TLV 或固定 layout） | — | 可靠传输确认 |
| HEARTBEAT | 0x02 | TimestampMs, StatusFlags（可选） | — | 保活 |

**输入一帧流程**：串口 RX → `FrameCodec::feedBytes()` → 解析出 `(msgType, seq, payload)` → 若为 TELEMETRY，则 `Tlvcodec::decodeItems(payload)` 得 TlvItem 列表 → 按 TlvType 取 FeedbackPosition、FeedbackSpeed 等 → 写回 **SysForm::sampleValueList / ctrlValueList** 或 SysInfo，驱动 PlotWidget 刷新。

#### 2.6.5 MsgType ↔ SysForm 列表对照汇总

| MsgType | 上行(PC→设备) | 下行(设备→PC) | SysForm 列表 |
|---------|----------------|----------------|--------------|
| SET_TARGET / SET_PARAMS | 设定/参数 TLV | — | setValueList ↔ TLV |
| TELEMETRY | — | 遥测 TLV | sampleValueList、ctrlValueList ← TLV |
| CONTROL_CMD | 命令 TLV | — | 控制逻辑，可选写 ctrlStatuList |
| HELLO / HEARTBEAT / ACK | 见上表 | 见上表 | 按需映射或仅协议层处理 |

实现时：**编码**用 setValueList / 设定/参数 生成 TlvItem 再组帧；**解码**对 TELEMETRY 的 payload 解析 TlvItem 后写回 sampleValueList、ctrlValueList（及 SysInfo），即为一帧输入/输出模版。

#### 2.6.6 示例：一帧输出 + 一帧输入

**例 1：上位机下发目标位置（输出一帧）**

- 业务：用户在界面把「目标位置」设为 **100.5**，要点「发送」。
- 步骤：
  1. 从 setValueList 或界面得到目标值 `targetPosition = 100.5f`。
  2. 构造 TLV 列表：一条 TlvItem，type=TargetPosition(0x0001)，value=float 100.5（小端 4 字节）。
  3. `payload = Tlvcodec::encodeItems(items)`，此处 payload 长度约 7 字节（Tag2 + Type1 + Len2 + Value4，具体以 TlvCodec 实现为准）。
  4. `frame = frameCodec->encodeFrame(0x10, 0, seq++, payload)`，得到一帧：  
     `55 AA 01 10 00 01 00 07 00 [payload…] [CRC16]`（MsgType=0x10 即 SET_TARGET）。
  5. `serialDevice->write(frame)` 发出。
- 伪代码：
  ```cpp
  QVector<TlvItem> items;
  Tlvcodec::appendFloat(items, TlvType::TargetPosition, 100.5f);
  QByteArray payload = Tlvcodec::encodeItems(items);
  QByteArray frame = frameCodec->encodeFrame(0x10, 0, nextSeq++, payload);
  serialDevice->write(frame);
  ```

**例 2：设备上报遥测，上位机更新显示（输入一帧）**

- 业务：设备按 50ms 周期上报当前「反馈位置」和「反馈速度」，上位机更新曲线和数值。
- 设备发出的一帧（示意）：MsgType=TELEMETRY(0x20)，Payload 内两条 TLV：  
  - FeedbackPosition(0x0101) = 98.3（float）；  
  - FeedbackSpeed(0x0102) = 1.2（float）。
- 上位机步骤：
  1. 串口收到字节 → `frameCodec->feedBytes(rxData)`。
  2. 解析出完整帧后发射 `frameReceived(0x20, seq, payload)`。
  3. 若 msgType == TELEMETRY：`items = Tlvcodec::decodeItems(payload)`。
  4. `tryGetFloat(items, TlvType::FeedbackPosition, pos)` → pos=98.3；`tryGetFloat(items, TlvType::FeedbackSpeed, speed)` → speed=1.2。
  5. 把 pos、speed 写回 SysInfo（如 currentValue、currentSpeed）或 SysForm 的 sampleValueList / ctrlValueList 对应项。
  6. 发射信号通知 UI，PlotWidget 从 DataBuffer/ SysInfo 取数刷新曲线。
- 伪代码：
  ```cpp
  // 在 frameReceived 的槽函数里
  if (msgType == 0x20) {  // TELEMETRY
      auto items = Tlvcodec::decodeItems(payload);
      float pos, speed;
      if (Tlvcodec::tryGetFloat(items, TlvType::FeedbackPosition, pos) &&
          Tlvcodec::tryGetFloat(items, TlvType::FeedbackSpeed, speed)) {
          sysInfo->setCurrentValue(pos);   // 或更新 sampleValueList
          sysInfo->setCurrentSpeed(speed); // 按你 SysInfo 字段命名
          dataBuffer->append(pos, ctrlValue);  // 供曲线使用
          emit dataUpdated();  // 通知 PlotWidget 刷新
      }
  }
  ```

以上两例即「一帧输出」与「一帧输入」的完整模版用法。

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
│   └── DataBuffer.h/cpp   # 曲线/日志公共数据缓存（环形缓冲区）
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
    └── FileManager.h/cpp  # 文件读写接口，可被日志线程调用
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

### 3.3 线程模型设计（UI + 数据处理 + 日志/绘图数据）

上位机采用 **3 线程模型**，在保证实时性的同时控制复杂度：

- **线程 1：主线程（UI / 轻量业务）**
  - Qt 默认 GUI 线程。
  - 负责：`MainWindow`、`PlotWidget` 等界面元素绘制与交互。
  - 只接收「已经处理好的数据快照」，做数值显示和曲线刷新，不参与重计算、不直接访问串口。

- **线程 2：数据处理线程（串口 + 协议 + 实时控制）**
  - 典型对象归属：
    - `device/SerialDevice`（内部持有 `QSerialPort`）
    - `protocol/FrameCodec`（`feedBytes` / `encodeFrame`）
    - `protocol/TlvCodec`（至少 TELEMETRY 方向解码）
    - `core/ControlManager`、`core/SysInfo`
  - 职责：
    - 串口 RX：`QSerialPort::readyRead` → `SerialDevice::onReadyRead()` → `FrameCodec::feedBytes()`。
    - 帧解析：在同一线程内处理 `frameReceived(msgType, seq, payload)`，对 TELEMETRY 调用 `TlvCodec::decode()`，更新 `SysInfo`（当前值、电流、电压、状态位等）。
    - 控制计算（可选）：定时读取 `SysInfo`，执行 PID/MPC，调用 `FrameCodec::encodeFrame()` + `SerialDevice::write()` 输出控制帧。
    - 生成样本：将每个 TELEMETRY 解析后封装为 `Sample` 结构（时间戳 + 各物理量），通过 Qt 信号以 **QueuedConnection** 方式发送给日志线程。
  - 约束：
    - **`SysInfo` 仅在此线程写入**，是系统「当前状态」的唯一权威来源。
    - UI 与日志线程只通过信号/快照读取，不直接跨线程修改。

- **线程 3：日志与绘图数据线程（缓存处理 + 文件存储）**
  - 典型对象归属：
    - `file/FileManager` 或专门的 `LogWorker` 类。
  - 职责：
    - 维护一个或多个环形缓冲区/队列，接收来自数据处理线程的 `Sample` 序列。
    - 从缓冲区读取数据，按批量写入日志/数据文件（CSV/二进制），避免在串口线程或 UI 线程内进行磁盘 IO。
    - 为曲线绘制准备数据：
      - 从原始样本缓存中**下采样/裁剪**当前可视窗口数据（例如「最近 N 秒」）。
      - 将已经整理好的 `PlotDataChunk`（少量点的集合）通过信号发给主线程中的 `PlotWidget`。
  - 约束：
    - 不直接操作任何 Qt GUI 控件，仅准备数据。
    - 文件 IO、下采样等耗时操作都在此线程完成，保证串口与 UI 不被阻塞。

> 说明：Qt 要求所有 GUI 控件必须在主线程绘制，因此「画图相关的重数据处理」放在第三线程，「真正调用绘制 API」留在主线程，以信号/槽传递 `PlotDataChunk` 实现解耦。

### 3.4 日志与绘图缓存区设计（方案 B：环形缓冲区）

为在线程 2（数据处理）与线程 3（日志/绘图数据）之间传递遥测样本，同时为曲线提供限定窗口的历史数据，本项目在 `core/DataBuffer` 内实现一个**固定容量环形缓冲区**（方案 B）。

1. **数据结构定义**

   - `Sample`：单个遥测样本，来源于 TELEMETRY 解码：

     ```cpp
     struct Sample {
         quint64 timestampMs;
         float   position;
         float   speed;
         float   current;
         float   voltage;
         quint32 statusFlags;
     };
     ```

   - `DataBuffer`：固定容量环形缓冲区，仅在**线程 3**内部读写：
     - 内部持有 `QVector<Sample> m_buffer;`，长度固定为 `capacity`（例如对应最近 N 秒数据量）。
     - 维护 `int m_writeIndex;` 与 `bool m_filled;` 等索引状态，表示当前写入位置与是否已经循环覆盖。
     - 对外提供只在日志线程内调用的接口：
       - `void append(const Sample &s);`：写入一个样本（覆盖最老数据）。
       - `QVector<Sample> window() const;`：返回当前时间窗口内的拷贝，用于生成绘图数据。

2. **线程间访问策略**

   - **线程 2（数据处理线程）**
     - 在解析 TELEMETRY 后构造 `Sample`，通过信号 `telemetrySampleReady(Sample)` 发送给线程 3 中的 `LogWorker::appendSample(Sample)` 槽。
     - 线程 2 不直接访问 `DataBuffer`，避免跨线程共享可变状态。

   - **线程 3（日志/绘图数据线程）**
     - `LogWorker` 持有一个 `DataBuffer` 实例，仅在本线程中：
       - 在 `appendSample(Sample s)` 槽中调用 `dataBuffer.append(s)`，把样本写入环形缓冲区。
       - 定时（如 50ms）调用 `dataBuffer.window()` 获取当前窗口数据，进一步下采样/裁剪生成 `PlotDataChunk`。
       - 将下采样后的 `PlotDataChunk` 通过信号 `historyDataReady(PlotDataChunk)` 发送到主线程。
     - 由于 `DataBuffer` 只在日志线程内部访问，正常情况下无需额外互斥锁。

   - **线程 1（主线程 / UI）**
     - 不直接访问 `DataBuffer` 或环形缓冲区。
     - 仅通过接收 `historyDataReady(PlotDataChunk)` 信号获得一份已经整理好的数据拷贝，在 `PlotWidget` 内部存成自己的 `QVector<QPointF>` 或类似结构后绘制。

3. **方案 B 的优势总结**

   - **固定容量、内存可控**：环形缓冲区只保留最近 N 秒数据，长期运行不会无限增长。
   - **线程边界清晰**：数据生产在线程 2，缓存与文件写入/下采样在线程 3，主线程只消费经过整理的数据副本。
   - **实现复杂度适中**：比无锁队列简单，又比单纯动态数组更工程化，适合当前实时曲线 + 日志场景。

### 3.5 实时控制主循环逻辑（初始化 → 状态更新 → 控制计算 → 输出）

从全局视角看，上位机的实时控制逻辑可以概括为一条主线：

> **一次性初始化 → 持续接收遥测并更新当前状态 → 周期性读取最新状态做控制计算 → 输出控制帧 → 循环往复**

结合三线程模型，具体分解如下：

1. **初始化阶段（程序启动时，仅执行一次）**
   - 创建并配置核心对象：
     - 协议层：`FrameCodec`、`TlvCodec`。
     - 控制核心：`ControlManager`、`SysInfo`、`DataBuffer`。
     - 设备层：`SerialDevice`（配置串口参数并准备打开）。
     - 日志与绘图：`LogWorker` / `FileManager`。
   - 创建并启动两个工作线程：
     - `DataWorkerThread`：承载 `SerialDevice`、`FrameCodec`、`ControlManager`、`SysInfo`。
     - `LogWorkerThread`：承载 `LogWorker`（内部持有 `DataBuffer` 等）。
   - 通过 `moveToThread()` 将上述对象绑定到目标线程。
   - 建立信号/槽连接（均为 QueuedConnection）：
     - 串口 RX → `FrameCodec::feedBytes()` → `frameReceived(...)`。
     - `frameReceived` → TELEMETRY 解析 → 更新 `SysInfo` → 发出 `Sample` 给日志线程。
     - 日志线程从 `Sample` 填充 `DataBuffer`，生成 `PlotDataChunk` 通知 UI。
     - UI 发命令/设定值信号到 `ControlManager` / `SerialDevice`。

2. **状态更新（异步、事件驱动，在线程 2 内持续运行）**
   - 设备按约定周期发送 TELEMETRY 帧，`QSerialPort::readyRead` 触发：
     - 线程 2 中的 `SerialDevice` 读取字节流并调用 `FrameCodec::feedBytes()`。
     - `FrameCodec` 解析出完整帧后发出 `frameReceived(msgType, seq, payload)`（仍在线程 2）。
     - 若为 TELEMETRY：
       - 使用 `TlvCodec::decode()` 解析 payload 得到物理量（位置、速度、电流、电压、状态位等）。
       - 更新 `SysInfo` 中的「当前状态」字段（仅线程 2 写入）。
       - 组装 `Sample` 结构并通过 `telemetrySampleReady(Sample)` 信号发送给线程 3。
   - 结果：`SysInfo` 始终反映“最近一帧遥测”的当前状态，为控制算法提供最新采样值。

3. **控制计算与输出（周期性定时器，在线程 2 内运行）**
   - 在线程 2 中，由 `ControlManager` 持有一个周期定时器（例如 50 ms）：
     - 每次定时到期时：
       1. 从 `SysInfo` 读取最新的状态快照（位置、速度、状态位等）。
       2. 结合来自 UI 的设定值/模式（通过信号更新到 `ControlManager` 内部），执行 PID/MPC 等控制算法，计算出新的控制量（如目标电流/力矩/速度等）。
       3. 使用 `TlvCodec` 将控制量编码为 TLV payload。
       4. 调用 `FrameCodec::encodeFrame()` 组装控制帧（如 `SET_TARGET` / `CONTROL_CMD`）。
       5. 通过 `SerialDevice::write()` 将控制帧发送到设备。
   - 该循环仅依赖本线程内的 `SysInfo` 与设定值，不直接阻塞等待串口，串口输入由 readyRead 驱动。

4. **UI 与日志的并行工作（不阻塞主控制循环）**
   - **线程 3：日志与绘图数据线程**
     - 持续接收来自线程 2 的 `Sample`，写入 `DataBuffer` 环形缓冲区。
     - 周期性（如 50 ms）从缓冲区中提取当前可视时间窗口的样本，做下采样/裁剪，生成 `PlotDataChunk`。
     - 通过 `historyDataReady(PlotDataChunk)` 信号将绘图数据发送到主线程。
     - 以批量方式将历史样本写入日志文件（CSV/二进制），不阻塞串口接收和控制计算。
   - **线程 1：主线程 / UI**
     - 槽函数接收：
       - `SysSnapshot`（来自线程 2）：用于数值显示（当前值、状态灯等）。
       - `PlotDataChunk`（来自线程 3）：更新 `PlotWidget` 内部的数据容器并触发 `update()`。
     - 不做协议解析、不直接访问 `DataBuffer`，仅消费数据副本，保证界面流畅性。

5. **整体时序总结**

   - 初始化完成后，程序进入稳定运行阶段：
     - 串口接收 + 状态更新 + 控制计算 + 控制输出均在**数据处理线程**内完成，形成闭环。
     - 日志记录与曲线数据整理在线程 3 内异步执行。
     - UI 线程只负责显示，不参与任何重计算或 IO。
   - 可以概括为一个简化循环思路：
     - **初始化 →（后台持续接收并更新状态）→ 控制线程周期性读取最新状态做控制计算并输出 → 日志/绘图线程从缓存中消费历史样本 → UI 展示结果**。

### 3.6 数据执行逻辑概览（从用户操作到设备响应）

为了便于整体把握「一次控制操作」在系统内的执行路径，现以 SET_TARGET / 控制命令为例，对完整数据执行逻辑做总结：

1. **用户侧：发起控制意图（线程 1 / UI）**
   - 用户在 `MainWindow` 中修改目标值、模式或点击「启动/停止」等按钮。
   - UI 将最新的设定值/控制指令写入本地表单模型，并通过信号发送给线程 2 中的 `ControlManager`：
     - 如：`emit targetUpdated(TargetData data);`
     - 如：`emit controlCommandIssued(ControlCmd cmd);`

2. **设定值生效：更新控制目标（线程 2 / 数据处理）**
   - `ControlManager` 在线程 2 内接收到上述信号，将设定值/命令存入内部状态（例如目标位置/速度、使能标志、控制模式等）。
   - 此时设备尚未立即变化，真正的控制输出会在下一次控制周期内计算并下发。

3. **状态采样：设备反馈驱动当前状态（线程 2 / 数据处理）**
   - 设备按照协议周期性上报 TELEMETRY 帧。
   - 串口 RX 触发 `QSerialPort::readyRead`：
     - `SerialDevice` 读取字节流 → `FrameCodec::feedBytes()`。
     - 解析出完整 TELEMETRY 帧后，`FrameCodec` 发出 `frameReceived(TELEMETRY, seq, payload)`。
   - 数据处理线程中对 payload 调用 `TlvCodec::decode()`，得到位置/速度/电流/电压等物理量：
     - 这些值被写入 `SysInfo` 中，形成「最新一帧设备状态」。
     - 同时封装为 `Sample` 结构，通过信号送往线程 3，用于日志与绘图。

4. **控制执行：周期性读取最新状态并下发控制量（线程 2 / 数据处理）**
   - 控制周期定时器（例如 50 ms）在 `ControlManager` 内触发：
     1. 从 `SysInfo` 读取当前状态（最近一次 TELEMETRY 的结果）。
     2. 结合第 2 步保存的设定值/控制命令，执行 PID/MPC 等控制算法，计算新的输出（如目标电流/目标速度）。
     3. 将计算结果封装为 TLV payload，调用 `FrameCodec::encodeFrame()` 组装 `SET_TARGET` / `CONTROL_CMD` 帧。
     4. 调用 `SerialDevice::write()` 通过串口发送到设备。
   - 由此，在控制周期的每个 tick 内，系统都实现了“读取最新反馈 → 对比设定值 → 计算控制量 → 输出”的闭环。

5. **数据记录与可视化：从缓存到曲线和日志（线程 3 + 线程 1）**
   - 线程 3（日志与绘图数据）：
     - 通过 `appendSample(Sample)` 槽接收来自线程 2 的样本，将其写入 `DataBuffer` 环形缓冲区。
     - 周期性从 `DataBuffer` 中提取最近 N 秒的样本窗口，下采样/裁剪后生成 `PlotDataChunk`。
     - 将 `PlotDataChunk` 通过 `historyDataReady(PlotDataChunk)` 信号发往主线程，同时按批写入日志文件。
   - 线程 1（UI）：
     - 接收 `SysSnapshot` 信号，用于数值显示和状态指示灯。
     - 接收 `PlotDataChunk` 信号，更新 `PlotWidget` 的数据模型并触发绘图。

6. **用户感知与反馈闭环**
   - 用户在 UI 中看到的数值和曲线，来源于线程 2 中最新的设备状态 + 线程 3 中整理的历史样本。
   - 用户的下一次操作（修改设定值/模式）再次进入上述流程，形成完整的人机闭环控制链路。

---

## 四、实施步骤（怎么做）

### 4.1 阶段 0：现有工程整理与基础运行（0.5 天）

目标：在当前 `RealCtrl` 工程基础上，确保最小可运行 GUI 工程，方便后续按阶段迭代。

1. **确认 CMake 与依赖**
   - 已存在 `CMakeLists.txt`，包含：
     - Qt Core / Widgets / SerialPort 组件。
     - 源文件：`ui/mainwindow.*`、`protocol/*`（`dcsprotocoltypes`、`ctrlmode`、`sysform`、`framecodec`、`tlvcodec`）、`device/SerialDevice` 等。
   - 在 Qt Creator 中配置好构建套件，确保工程可以编译并启动一个空的 `MainWindow`。

2. **最小运行验证**
   - 在 `main.cpp` 中，仅创建 `QApplication` + `MainWindow` 并显示。
   - 编译运行，确认窗口正常弹出，无崩溃。

> 说明：此阶段不要求串口/协议已实现完备，只需保证工程结构与构建链路稳定。

### 4.2 阶段 1：协议层完善与自测（1–2 天）

目标：在现有 `protocol` 模块基础上，把帧与 TLV 协议彻底打牢，单独验证正确性。

1. **完善 `FrameCodec` 实现**
   - 在 `protocol/framecodec.cpp` 中补全：
     - 常量：SOF、协议版本、最大帧长（与第 2 章一致）。
     - `encodeFrame(quint8 msgType, quint8 flags, quint16 seq, const QByteArray& payload)`：
       - 按 2.2 的帧结构依次写入 SOF/Version/MsgType/Flags/Seq/PayloadLen/Payload/CRC16。
       - 调用 `crc16Ccitt` 计算 CRC，写入帧尾。
     - `feedBytes(const QByteArray &data)`：
       - 将 `data` 追加到内部 `m_rxBuffer`。
       - 按 SOF、长度、CRC 规则在缓冲区中查找完整帧：
         - 校验头部合法性（SOF、版本等）。
         - 校验 `PayloadLen` 与总长度是否在限制内。
         - 校验 CRC16，错误时丢弃当前头并向后继续查找。
       - 每解析出一帧后发射 `frameReceived(msgType, seq, payload)` 信号。
   - 实现 `crc16Ccitt` 静态函数（查表或逐字节算法，参数与 2.5 一致）。

2. **完善 `TlvCodec` 与 `SysForm` 映射**
   - 在 `protocol/tlvcodec.*` 中实现：
     - `QByteArray encode(const QList<ParaNode>& list)`：遍历 `ParaNode` 列表，将每个参数转换为 TLV（Tag/Type/Len/Value）。
     - `QList<ParaNode> decode(const QByteArray& data)`：从 TLV 字节流中解析出 `ParaNode` 列表。
   - 与 `protocol/sysform.h` 对齐：
     - 明确 `setValueList`、`sampleValueList`、`ctrlValueList` 中每个 `ParaNode` 的 Tag/Type 映射（参考 2.6.2）。
     - 补充必要的辅助函数，如 `appendFloat/tryGetFloat` 等。

3. **协议层单元测试 / 小工具**
   - 在单独的测试函数或简单控制台程序中验证：
     - 构造一个 `QList<ParaNode>` → TLV 编码 → 解码 → `ParaNode` 列表内容一致。
     - 构造 payload → 使用 `FrameCodec::encodeFrame` 生成帧 → 手工/程序校验字段正确。
     - 将编码出的帧分包/粘包方式喂入 `feedBytes`，验证能正确组装出原始 payload。

### 4.3 阶段 2：设备层封装与串口收发（1 天）

目标：让上位机能通过 `SerialDevice` 打开串口、发送一帧、接收并解析一帧。

1. **完善 `device/SerialDevice`**
   - 在 `serialdevice.h/cpp` 中补充/确认：
     - `setPortName` / `setBaudRate` 的实现，并在 `open()` 中应用。
     - `open()`：配置串口参数（数据位、停止位、校验位等），调用 `QSerialPort::open()`，成功后调用 `setupConnections()`。
     - `close()`：关闭串口并清理状态。
     - `send(quint8 msgType, quint8 flags, const QByteArray &payload)`：
       - 内部维护 `m_seq` 自增序号。
       - 调用 `m_codec.encodeFrame(msgType, flags, m_seq, payload)` 获取整帧字节流。
       - 通过 `m_port.write(frame)` 发送。
   - 在 `setupConnections()` 中：
     - 连接 `QSerialPort::readyRead` 到一个槽函数（例如 `onReadyRead()`），从 `m_port.readAll()` 读取数据，再调用 `m_codec.feedBytes(rxData)`。
     - 连接 `FrameCodec::frameReceived` 到 `SerialDevice` 的转发信号（如 `frameReceived`），使上层可以直接连接。

2. **串口环回/真机测试**
   - 若有设备，可按协议发送 HELLO/HEARTBEAT 帧，看设备响应。
   - 若暂时没有设备，可使用串口环回（loopback）或上位机自身回显程序，验证帧收发与解码链路。

### 4.4 阶段 3：核心数据模型与控制骨架（1–2 天）

目标：在 `core` 模块中补齐控制核心与系统状态，打通「SysForm ↔ 协议 ↔ 控制」链路的骨架。

1. **创建 `core/SysInfo`（系统当前状态）**
   - 设计一个轻量的 `SysInfo` 类（或结构），用于保存当前运行状态（位置/速度/电流/电压/状态位等）。
   - 提供线程 2 内的读写接口，未来将作为控制算法和 UI 快照的主要数据来源。

2. **创建 `core/ControlManager`**
   - 负责：
     - 管理设定值（目标位置、速度、模式、使能等），从 UI 信号中更新。
     - 持有控制算法实例（后续的 PID/MPC），维护控制周期定时器。
     - 在定时器槽函数中：
       - 从 `SysInfo` 读取最新状态。
       - 与设定值对比，执行控制计算（暂时可以先简单透传或比例控制，以后再替换为正式算法）。
       - 调用 `SerialDevice::send()` 下发 `SET_TARGET` / `CONTROL_CMD` 帧。

3. **协议与 `SysForm` 结合（可选增强）**
   - 在需要时，将设备描述文件（`SysForm`）读入后：
     - 使用 `setValueList` 生成 UI 控件与参数列表。
     - 在编码/解码 TLV 时，按 `SysForm` 中的 `ParaNode` 定义自动映射，减少硬编码。

### 4.5 阶段 4：UI 扩展与单线程闭环联调（2–3 天）

目标：在单线程模式下（暂不启用 QThread），完成从 UI → 控制 → 串口 → 遥测 → UI 的最小闭环。

1. **扩展 `MainWindow` UI**
   - 在 `mainwindow.ui` 中增加：
     - 基本数值显示控件（当前位置、速度、电流、电压等）。
     - 设定值输入控件（目标位置/速度、模式选择、使能/启动按钮）。
     - 简单日志/状态栏显示框。
   - 在 `mainwindow.cpp` 中：
     - 持有 `SerialDevice`、`ControlManager` 实例（暂时还在主线程中）。
     - 将 UI 控件的信号（值变化/按钮点击）连接到 `ControlManager` 对应槽，用于更新设定值或下发命令。

2. **连接串口与 UI 显示**
   - 将 `SerialDevice::frameReceived` 信号连接到一个槽函数：
     - 对 TELEMETRY：
       - 使用 `TlvCodec` 解析 payload，更新 `SysInfo`。
       - 同时更新 UI 上的数值显示控件。
   - 暂时可以不启用 `DataBuffer` 和 LogWorker，仅验证数值能实时更新。

3. **单线程最小闭环自测**
   - 手工在 UI 修改目标值/模式：
     - 确认 `ControlManager` 能收到设定值更新。
     - 确认在控制周期内能调用 `SerialDevice::send` 发送控制帧。
   - 在设备回传 TELEMETRY 时：
     - 确认 UI 数值有同步变化。
   - 记录待优化问题（如 UI 卡顿、串口处理阻塞等），为后续多线程改造做准备。

### 4.6 阶段 5：三线程改造与缓存/曲线/日志（2–3 天）

目标：按第 3 章设计，将现有单线程实现改造为 UI + 数据处理 + 日志/绘图数据三线程结构，并引入环形缓冲与曲线显示。

1. **引入 `DataWorkerThread` 与 `LogWorkerThread`**
   - 在主程序中创建两个 `QThread` 对象：
     - `DataWorkerThread`：承载 `SerialDevice`、`FrameCodec`、`ControlManager`、`SysInfo`。
     - `LogWorkerThread`：承载 `LogWorker`（内部持有 `DataBuffer`、文件句柄等）。
   - 使用 `moveToThread()` 将对应对象迁移到目标线程，注意：
     - 仅在对象尚未连接 UI 控件/未开始使用时迁移。
     - 在线程结束前，先调用 `quit()` / `wait()`，再析构对象。

2. **实现 `core/DataBuffer` 与 `LogWorker`**
   - 在 `core/DataBuffer.h/cpp` 中实现第 3.4 节描述的环形缓冲区接口：
     - 固定容量、`append(const Sample&)` 与 `window()`。
   - 在新建的 `LogWorker` 类中：
     - 槽函数 `appendSample(const Sample &s)`：调用 `dataBuffer.append(s)`。
     - 定时器槽函数：从 `dataBuffer.window()` 获取样本窗口，执行下采样/裁剪，生成 `PlotDataChunk`，发射 `historyDataReady(PlotDataChunk)` 到 UI。
     - 批量将样本写入日志文件（可按行 CSV 或二进制结构）。

3. **调整信号/槽到三线程结构**
   - UI（线程 1）只通过信号与 `ControlManager` / `SerialDevice` 交互，不直接访问其内部状态。
   - `SerialDevice` / `FrameCodec` / `ControlManager`（线程 2）：
     - 继续负责串口收发、协议解析、控制计算和 `SysInfo` 更新。
     - 在 TELEMETRY 解码后构造 `Sample`，发出 `telemetrySampleReady(Sample)` 到 `LogWorker`。
     - 在控制周期内计算好控制量后调用 `SerialDevice::send()`。
   - `LogWorker`（线程 3）：
     - 接收 `telemetrySampleReady(Sample)`，写入 `DataBuffer`，下采样生成 `PlotDataChunk`，发射到 UI。

4. **PlotWidget 集成与性能调优**
   - 在 UI 层实现/扩展 `PlotWidget`：
     - 槽函数接收 `PlotDataChunk`，将点集合拷贝到自身数据容器，并调用 `update()` 绘制。
     - 注意控制刷新频率和点数量，避免一次绘制过多点导致 UI 卡顿。
   - 通过实际测试调整：
     - 控制周期（如 50 ms）与图形刷新周期（如 50–100 ms）。
     - 环形缓冲容量（覆盖最近 N 秒）。
     - 日志写盘批量大小。

5. **多线程联调与回归**
   - 重点关注：
     - UI 是否保持流畅，长时间运行是否有明显卡顿。
     - 串口是否稳定（无溢出/丢帧），控制闭环是否按预期工作。
     - 日志文件与曲线是否正确反映真实数据（无乱序/缺失）。
   - 如发现竞争条件或生命周期问题，优先通过调整信号槽与对象归属解决，避免引入复杂锁机制。

---

## 五、与旧 MFC 协议的关系

本协议为**全新定义**，与 `realctrl - 副本 (2)/DcsPrtcl` 的 MFC 协议**不兼容**。若需与旧上位机/设备互通，需在设备端或网关侧做协议转换；或单独保留一套 MFC 兼容编解码器，按运行配置切换。

---

## 六、版本与修订

| 版本 | 日期       | 修订内容 |
|------|------------|----------|
| 1.0  | 2025-03    | 初版：帧协议 + TLV + 实施步骤 |
| 1.1  | 2025-03    | 新增 2.6：一帧输入/输出模版（MsgType ↔ TlvType ↔ SysForm 对照表） |
| 1.2  | 2026-03    | 新增 3.3 线程模型、3.4 日志/绘图环形缓冲区设计与 3.5 实时控制主循环逻辑，补充 4.5 多线程实施步骤 |

---

*文档路径：`RealCtrl/上位机软件设计说明书SDS.md`*

---

## 七、当前实现进度与后续计划（2026-03 更新）

### 7.1 已完成的实现

1. **FrameCodec（阶段 4.1）**
   - 按 2.2 节帧结构完成 `FrameCodec` 类：
     - SOF(0x55 0xAA) + Version + MsgType + Flags + Seq + PayloadLen + Payload + CRC16。
     - 小端序，实现 CRC16-CCITT（多项式 0x1021，初始值 0xFFFF）。
   - `feedBytes()` 支持半包/粘包处理，CRC 校验失败的帧自动丢弃。
   - 已在 `com0com` 虚拟串口环境下完成半包、粘包、CRC 错帧等场景测试。

2. **TLV 基础版（阶段 4.2 的子集）**
   - 实现 `Tlvcodec`，采用简化 TLV 结构：`Type(u16) + Length(u16) + Value`。
   - 提供：
     - `encodeItems/decodeItems`：`QVector<TlvItem>` ↔ `QByteArray payload`。
     - 基础类型封装与读取：`appendFloat/appendUInt8/appendString`、`tryGetFloat/tryGetUInt8/tryGetString`。
   - 已用于构造/解析 `SET_PARAMS/SET_TARGET/TELEMETRY` 中的示例字段（如 `TextMessage`、`Mode` 等），验证正确性。

3. **设备抽象与串口封装（阶段 4.3）**
   - 定义 `DeviceInterface` 抽象基类，统一设备接口：
     - `open()/close()/isOpen()`。
     - 信号：`opened/closed/errorOccurred/frameReceived`。
   - 实现 `SerialDevice`：
     - 封装 `QSerialPort` 打开/关闭、错误处理。
     - 内部集成 `FrameCodec`，`readyRead` 时自动喂给解帧逻辑。
     - 对外仅暴露 `send(msgType, flags, payload)` 和 `frameReceived` 信号，实现串口与帧协议解耦。
   - 使用 `com0com`（COM5 ↔ COM6）完成实际串口层联调。

4. **虚拟设备 FakeDevice（用于无硬件时的闭环验证）**
   - 在 `device/FakeDevice` 中：
     - 内部持有一个 `SerialDevice`（设备端）。
     - 收到 `SET_TARGET(0x10)` 时，解析 TLV(TargetPosition) 更新内部目标值 `m_target`。
     - 50ms 定时器周期执行：`m_current += (m_target - m_current) * 0.2`，并将 `m_current` 作为 `FeedbackPosition` 打包为 `TELEMETRY(0x20)` 回传上位机。
   - 实现了在没有真实硬件的情况下，模拟设备侧控制响应与遥测上报。

5. **SysInfo 与 ControlManager（阶段 4.4 基础版）**
   - `SysInfo`：
     - 维护 `currentValue/targetValue/mode`，任一字段变化时发射 `changed()` 信号。
   - `ControlManager`：
     - 构造函数中连接 `SerialDevice::frameReceived`，解析不同 `MsgType`。
     - 通过 `QTimer(50ms)` 的 `runStep()` 周期性发送 `SET_TARGET` 帧：
       - 从 `SysInfo::targetValue` 构造 TLV(TargetPosition)。
     - 收到 `TELEMETRY(0x20)` 时，解析 TLV(FeedbackPosition/Mode)，回写 `SysInfo`，形成完整的数据闭环。

6. **MainWindow UI 集成**
   - 利用现有 UI 布局（左侧控制面板 + 顶部状态栏）完成与控制层的集成：
     - `btnStart`：设定目标值（当前为固定 10.0，用于测试）并调用 `ControlManager::start()`。
     - `btnStop`：调用 `ControlManager::stop()`，停止控制循环。
     - `btnClear`：停止控制循环并将 `SysInfo` 中的 `targetValue/currentValue` 清零。
   - 顶部显示栏：
     - `m_setting` 显示当前设定值（由 `SysInfo::targetValue` 驱动）。
     - `label_4` 显示当前值（由 `SysInfo::currentValue` 驱动）。
     - `m_time` 由独立 `QTimer` 每秒刷新系统时间。
   - 在 `SerialDevice(COM5)` + `FakeDevice(COM6)` 环境下，点击“开始”后，UI 上的“当前值”可以随时间逐渐逼近设定值，验证了串口协议、TLV 编解码、控制循环与 UI 刷新整个链路的正确性。

### 7.2 后续计划（与旧 MFC 算法工程的衔接）

1. **抽象算法接口（IAlgorithm）**
   - 在 `algorithm/` 目录创建 `IAlgorithm` 接口：
     - `double compute(double target, double current);`
     - `void setParameters(const QVariantMap& params);`
   - `ControlManager` 中增加 `IAlgorithm* currentAlgorithm` 成员，并在 `runStep()` 中改为：
     - 从 `SysInfo` 读取 `target/current`。
     - 调用 `currentAlgorithm->compute(target, current)` 得到控制量。
     - 将控制量编码为 TLV/帧发送给设备（后续与旧工程保持一致）。

2. **迁移和封装旧工程中的算法实现**
   - 从 `realctrl - 副本 (2)` 中提取纯算法逻辑（如 PID/MPC），改写为实现 `IAlgorithm` 的子类：
     - `PIDAlgorithm`、`MPCAlgorithm` 等。
   - UI 侧通过 `AlgComboBox` 选择不同算法，`ControlManager` 动态切换 `currentAlgorithm`。

3. **参数表（SysForm + ParaNode）驱动 UI 与协议（对应 SDS 552–555）**
   - 为 `ParaNode` 增加 `tag/id` 字段或建立 name→tag 映射：
     - 用于 TLV 中的 Tag(u16) 编码。
   - 实现 `SysForm` 与 TLV 的自动映射模块：
     - `encodeFromSysForm(const SysForm&) -> QByteArray payload`。
     - `applyToSysForm(SysForm&, const QByteArray& payload)`。
   - 基于 `setValueList(ParaNode 列表)` 动态生成参数面板 UI（FormLayout 或 TableWidget），实现“参数新增/修改只需改 SysForm，不需改 UI 和协议代码”的目标。

4. **与真实设备联调替换 FakeDevice**
   - 在真实串口设备就绪后，将 `FakeDevice` 替换为真实设备：
     - 复用现有 `SerialDevice + FrameCodec + Tlvcodec + ControlManager + SysInfo + UI` 结构。
   - 增强 ACK/重发策略、错误处理、故障显示等工程化特性。
