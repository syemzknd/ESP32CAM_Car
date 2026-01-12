# ESP32-CAM 遥控车 - 现有架构优化方案

> 文档版本：1.0  
> 更新日期：2026年1月12日  
> 适用范围：基于当前 HTTP MJPEG + WebSocket 架构的优化

---

## 📊 当前架构瓶颈分析

### 延迟来源分解

| 延迟环节 | 当前估计延迟 | 占比 | 可优化程度 |
|---------|-------------|------|-----------|
| 摄像头采集 + JPEG编码 | 30-50ms | 15% | ⭐⭐⭐ |
| 帧缓冲等待 | 50-150ms | 35% | ⭐⭐⭐⭐⭐ |
| HTTP 传输开销 | 20-40ms | 10% | ⭐⭐ |
| 网络传输 | 30-80ms | 20% | ⭐⭐⭐ |
| 浏览器解码显示 | 10-30ms | 8% | ⭐⭐ |
| 串口日志阻塞 | 10-50ms | 12% | ⭐⭐⭐⭐⭐ |

### 当前代码问题清单

1. **摄像头配置**
   - 未使用 `CAMERA_GRAB_LATEST` 模式，获取的可能是旧帧
   - `fb_count=1` 单缓冲可能导致采集阻塞

2. **视频流处理**
   - `stream_handler` 中大量 `Serial.printf` 调用阻塞主线程
   - 无帧率限制，网络差时帧堆积
   - 无丢帧机制，延迟累积

3. **WiFi 配置**
   - 未禁用 WiFi 省电模式，导致随机延迟尖峰
   - 使用默认发射功率

4. **控制通道**
   - WebSocket 50ms 固定轮询间隔
   - JSON 文本格式开销

---

## 🔧 优化实施方案

### 阶段一：立即实施（预计改善 30-50%）

#### 1.1 禁用 WiFi 省电模式

**文件**: `ESP32CAM_Car.ino`  
**位置**: `setup()` 函数中 WiFi 连接后

```cpp
// 在 WiFi.begin() 之后添加
#include "esp_wifi.h"

// WiFi 连接成功后添加以下代码
WiFi.setSleep(false);                    // Arduino API
esp_wifi_set_ps(WIFI_PS_NONE);           // ESP-IDF API
WiFi.setTxPower(WIFI_POWER_19_5dBm);     // 最大发射功率
```

**预期效果**: 消除 WiFi 省电导致的 10-50ms 随机延迟尖峰

---

#### 1.2 启用最新帧抓取模式

**文件**: `ESP32CAM_Car.ino`  
**位置**: 摄像头配置部分

```cpp
// 在 camera_config_t config 配置中添加
config.grab_mode = CAMERA_GRAB_LATEST;   // 只获取最新帧，丢弃旧帧
config.fb_location = CAMERA_FB_IN_PSRAM; // 确保帧缓冲在 PSRAM
```

**预期效果**: 消除帧缓冲积压导致的 50-150ms 延迟

---

#### 1.3 移除串口日志输出

**文件**: `app_httpd.cpp`  
**位置**: `stream_handler` 函数

```cpp
// 删除或注释以下代码
// Serial.printf("MJPG: %uB %ums (%.1ffps), AVG: %ums (%.1ffps)"
//     ,(uint32_t)(_jpg_buf_len),
//     (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time,
//     avg_frame_time, 1000.0 / avg_frame_time
// );
```

**同时移除其他 handler 中的日志**:
- `go_handler`: 移除 `Serial.println("Go");`
- `back_handler`: 移除 `Serial.println("Back");`
- `left_handler`: 移除 `Serial.println("Left");`
- `right_handler`: 移除 `Serial.println("Right");`
- `stop_handler`: 移除 `Serial.println("Stop");`
- `toggleled_handler`: 移除 `Serial.println(...);`

**预期效果**: 消除串口阻塞导致的 10-50ms 延迟

---

#### 1.4 降低 JPEG 质量

**文件**: `ESP32CAM_Car.ino`

```cpp
// 修改 jpeg_quality
if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 50;    // 从 30 改为 50（数值越大质量越低）
    config.fb_count = 2;         // 双缓冲
} else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 55;
    config.fb_count = 1;
}
```

**预期效果**: 减少 JPEG 编码时间 20-30%

---

### 阶段二：中等优化（预计额外改善 20-30%）

#### 2.1 添加帧率限制与丢帧机制

**文件**: `app_httpd.cpp`  
**修改**: `stream_handler` 函数

```cpp
static esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];

    // 帧率控制参数
    const int64_t MIN_FRAME_INTERVAL = 40000;  // 40ms = 最大25fps
    int64_t last_frame = esp_timer_get_time();

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    while(true){
        // 帧率限制
        int64_t now = esp_timer_get_time();
        int64_t elapsed = now - last_frame;
        if (elapsed < MIN_FRAME_INTERVAL) {
            vTaskDelay(pdMS_TO_TICKS((MIN_FRAME_INTERVAL - elapsed) / 1000));
        }
        last_frame = esp_timer_get_time();

        fb = esp_camera_fb_get();
        if (!fb) {
            res = ESP_FAIL;
            break;
        }

        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;

        // 发送帧
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }

        esp_camera_fb_return(fb);
        fb = NULL;

        if(res != ESP_OK){
            break;
        }
    }

    return res;
}
```

**预期效果**: 稳定帧率，防止网络拥塞时延迟累积

---

#### 2.2 优化 WebSocket 控制发送

**文件**: `app_httpd.cpp` 中的前端 JavaScript  
**修改**: `index_handler` 中的脚本

```javascript
// 原代码
// setInterval(sendControl, 50);

// 优化为变化触发 + 节流
var lastSent = 0;
var lastThrottle = 0, lastSteer = 0;
function sendControl() {
    var now = Date.now();
    // 仅当值变化或超过100ms时发送
    if ((throttle !== lastThrottle || steer !== lastSteer) && (now - lastSent > 30)) {
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({t: Math.round(throttle * speedMultiplier), s: Math.round(steer * speedMultiplier)}));
            lastThrottle = throttle;
            lastSteer = steer;
            lastSent = now;
        }
    }
}
setInterval(sendControl, 20);  // 检查间隔改为20ms
```

**预期效果**: 减少不必要的网络流量，控制响应更快

---

#### 2.3 HTTP 服务器配置优化

**文件**: `app_httpd.cpp`  
**位置**: `startCameraServer` 函数

```cpp
void startCameraServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    // 增加发送缓冲区大小
    config.send_wait_timeout = 5;     // 减少发送超时
    config.recv_wait_timeout = 5;     // 减少接收超时
    config.max_uri_handlers = 16;
    config.stack_size = 8192;         // 增加栈大小
    
    // ... 后续代码保持不变
}
```

---

### 阶段三：进一步优化

#### 3.1 FreeRTOS 任务优先级调整

**文件**: `ESP32CAM_Car.ino`

```cpp
// 在 setup() 末尾添加，提升当前任务优先级
vTaskPrioritySet(NULL, configMAX_PRIORITIES - 2);
```

---

#### 3.2 loop() 函数优化

**文件**: `ESP32CAM_Car.ino`

```cpp
void loop() 
{
    // 减少 WiFi 检查频率
    static unsigned long lastWiFiCheck = 0;
    unsigned long now = millis();
    
    if (now - lastWiFiCheck > 5000) {  // 每5秒检查一次
        lastWiFiCheck = now;
        
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.reconnect();
            // 等待重连...
        }
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));  // 使用 FreeRTOS 延迟，释放 CPU
}
```

---

## 📋 实施检查清单

### 立即实施（阶段一）

- [ ] 添加 WiFi 省电禁用代码
- [ ] 添加 `config.grab_mode = CAMERA_GRAB_LATEST`
- [ ] 移除 `stream_handler` 中的串口日志
- [ ] 移除各 handler 中的 `Serial.println`
- [ ] 调整 `jpeg_quality` 到 50
- [ ] 设置 `fb_count = 2`

### 中等优化（阶段二）

- [ ] 实现帧率限制机制
- [ ] 优化前端控制发送逻辑
- [ ] 调整 HTTP 服务器配置

### 进一步优化（阶段三）

- [ ] 添加任务优先级调整
- [ ] 优化 loop() 函数

---

## 📈 预期效果汇总

| 指标 | 优化前 | 阶段一后 | 全部完成后 |
|-----|-------|---------|-----------|
| 视频延迟 | 300-500ms | 150-250ms | 100-180ms |
| 控制延迟 | 80-150ms | 50-100ms | 40-70ms |
| 帧率 | 10-15fps | 15-20fps | 20-25fps |
| 稳定性 | 波动大 | 较稳定 | 稳定 |

---

## ⚠️ 注意事项

1. **逐步实施**: 建议按阶段逐步实施，每次修改后测试效果
2. **备份代码**: 修改前务必备份原始代码
3. **测试环境**: 确保测试时 WiFi 信号良好
4. **串口调试**: 调试完成后再移除串口日志，开发期间可保留部分关键日志

---

## 🔗 相关文档

- [进阶架构升级方案](UPGRADE_PLAN_ADVANCED.md) - 更深层次的架构改造方案
