# ESP32CAM 遥控车项目说明

## 项目概述

这是一个基于 Arduino 的物联网项目，用于开发一款使用 ESP32 和摄像头模块的遥控车。它可以通过 WiFi 和网页界面提供实时视频流和电机控制功能。

## 架构

- **ESP32CAM_Car.ino**：主设置文件 - 初始化摄像头、WiFi 和 HTTP 服务器。

- **app_httpd.cpp**：HTTP 服务器实现，包含摄像头视频流和电机控制的处理程序。

- **camera_index.h**：用于控制按钮和视频显示的 Gzip 压缩 HTML/JS 网页界面。

三个 HTTP 服务器运行在连续的端口（80、81、82）上，分别用于网页界面、视频流和 WebSocket 控制。

## 关键模式

- 电机控制使用 `WheelAct(int nLf, int nLb, int nRf, int nRb)` 函数，通过 GPIO 引脚进行开关控制：

- 前进：`WheelAct(LOW, HIGH, HIGH, LOW)`

- 后退：`WheelAct(HIGH, LOW, LOW, HIGH)`

- 左转：`WheelAct(LOW, HIGH, LOW, HIGH)`

- 右转：`WheelAct(HIGH, LOW, HIGH, LOW)`

- 停止：`WheelAct(LOW, LOW, LOW, LOW)`

- 基于 PWM 的速度控制通过 `Drive(int throttle, int steer)` 函数实现（油门/转向范围 -100 到 100），该函数会调用 `setMotor(left, right)` 和 `setOneMotor(chF, chB, )` 函数。 val)` 用于可变速度。

- GPIO 分配：gpLf=2，gpLb=14，gpRf=15，gpRb=13（电机）；gpLed=4（LED 灯）。

- HTTP 端点：`/go`，`/back`，`/left`，`/right`，`/stop`，`/ledon`，`/ledoff`，`/capture`，`/stream`，`/status`，`/control`。

- WebSocket 端点 `/ws`，端口 82，用于通过 JSON 消息（例如 `{"t":50,"s":20}`）进行连续控制（油门/转向）。

- Web UI 按钮触发 GET 请求来控制端点；滑块使用 WebSocket 进行实时控制。

## 开发流程

- 使用支持 ESP32 开发板的 Arduino IDE。

- 选择“ESP32 Wrover Module”，并在开发板设置中启用 PSRAM。

- 在 `ESP32CAM_Car.ino` 中更新 WiFi 凭据：`const char* ssid = "your_wifi_name"; const char* password = "your_wifi_password";`

- 如果电机方向反转，请交换 `app_httpd.cpp` 中的处理函数（例如，交换 `go_handler` 和 `back_handler`）。

- 摄像头设置：JPEG 格式，QQVGA 分辨率以获得流畅的视频流；调整 `jpeg_quality`（数值越高，画质越低，速度越快）。

- PWM 设置：使用 `ledcSetup channels 0-3` 设置频率为 1000，分辨率为 8；分别连接到 gpLf、gpLb、gpRf 和 gpRb。

## 约定

- 默认禁用串口调试输出：`Serial.setDebugOutput(false);`

- 启用摄像头垂直翻转：`s->set_vflip(s, 1);`

- 检查 PSRAM 中是否存在更高帧缓冲区：`if(psramFound()) config.fb_count = 2;`

- 外部 GPIO 变量定义在 `ESP32CAM_Car.ino` 中，并在 `app_httpd.cpp` 中使用。

## 部署

- 通过 Arduino IDE 编译并上传。

- 连接 WiFi 后，通过 ESP32 的 IP 地址访问 Web 界面。

- 监控串口输出以获取 IP 地址和连接状态。
---

## 📂 项目结构

```text
.
├── ESP32CAM_Car.ino     # 主程序文件，初始化摄像头、WiFi和HTTP服务器
├── app_httpd.cpp        # HTTP服务器实现，处理摄像头流和电机控制
├── camera_index.h       # 网页界面（压缩的HTML/JS）
├── README.md            # 项目说明文档

```

---

## 📋 物料清单 (Bill of Materials)

| 物料名称          | 数量 |
|-------------------|------|
| Esp32主板        | 1    |
| 亚克力4WD小车底板 | 1    |
| L298N模块        | 1    |
| 电池盒           | 1    |
| TT电机支架       | 1    |
| 摄像头支架       | 1    |
| TT电机           | 4    |
| TT轮胎           | 4    |
| USB数据线        | 1    |
| 母对母杜邦线     | 10   |
| 公对母杜邦线     | 10   |
| 18650电池        | 2    |
| M3*10螺丝        | 6    |
| M3*8螺丝         | 30   |
| M3螺帽           | 14   |
| M3*10铜柱        | 9    |
| M3*30螺丝        | 9    |
| ESP32-CAM开发板  | 1    |

---

## 💻 代码介绍

本项目代码基于Arduino框架开发，使用ESP32和ESP32-CAM模块实现远程控制小车功能。代码分为多个版本迭代优化：

### 版本更新历史

- **初始版本 (v1.0)**: 基础功能实现，包括摄像头初始化、WiFi连接、HTTP服务器搭建、电机控制和网页界面。
  
- **稳定性优化 (v1.1)**: 
  - 降低摄像头初始分辨率至QVGA，减少内存使用。
  - 在loop()中添加delay(10)以防止CPU占用过高。
  - 分离HTTP服务器端口（控制80，视频81）以避免阻塞。

- **流畅度优化 (v1.2)**: 
  - 进一步降低分辨率至QQVGA，提高HTTP流畅度。
  - 限制MJPEG帧率至约20fps，释放CPU资源。
  - 限制视频流仅支持单个客户端连接。
  - 简化控制handler，移除不必要的日志输出。
  - 关闭所有非必要系统日志以提升性能。
