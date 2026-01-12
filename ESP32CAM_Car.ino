
/*
 * @Date: 2020-11-27 11:45:09
 * @Description: ESP32 Camera Surveillance Car
 * @FilePath: 
 */

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_wifi.h"

//
// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled
//
// Adafruit ESP32 Feather

// ===== WiFi 配置 =====
const char* ssid = "MERCURY_77DA";   //你的WiFi名称
const char* password = "Lss5201314";   //你的WiFi密码

// ===== 静态IP配置 (固定地址，无需串口查看) =====
// 设置为 true 使用静态IP，设置为 false 使用DHCP自动分配
#define USE_STATIC_IP true

// 静态IP地址配置 (根据你的路由器网段修改)
// 手机浏览器直接访问: http://192.168.1.100
IPAddress staticIP(192, 168, 1, 100);      // ESP32的固定IP地址
IPAddress gateway(192, 168, 1, 1);         // 路由器网关地址
IPAddress subnet(255, 255, 255, 0);        // 子网掩码
IPAddress dns1(192, 168, 1, 1);            // DNS服务器1 (通常与网关相同)
IPAddress dns2(8, 8, 8, 8);                // DNS服务器2 (Google DNS)

// ===== 电池监测配置 =====
#define BATTERY_PIN -1  // ADC pin for battery voltage, set to -1 if no voltage divider used
float batteryVoltageDivider = 1.0;  // Voltage divider ratio (adjust based on your circuit)

float getBatteryVoltage() {
    if (BATTERY_PIN == -1) {
        return 3.7;  // Assume full battery if no monitoring
    }
    int adcValue = analogRead(BATTERY_PIN);
    float voltage = (adcValue / 4095.0) * 3.3 * batteryVoltageDivider;
    return voltage;
}

int getBatteryPercent() {
    float voltage = getBatteryVoltage();
    // Assuming Li-ion battery: 3.0V empty, 4.2V full
    int percent = (voltage - 3.0) / (4.2 - 3.0) * 100;
    return constrain(percent, 0, 100);
}

// ===== 摄像头引脚配置 (AI-THINKER ESP32-CAM) =====
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0   // 摄像头时钟
#define SIOD_GPIO_NUM     26   // 摄像头 I2C 数据线
#define SIOC_GPIO_NUM     27   // 摄像头 I2C 时钟线

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// GPIO Setting   小车电机 & LED 引脚定义 
//一般电机都会接错，烧录代码后对照操作与实际效果修改小车电机引脚定义
//或者在app_http.cpp文件中的云端电机前后左右控制函数（go_handler，back_handler，left_handler，right_handler）中修改函数名，比如在操作界面中点击前进的实际效果是左转，那么就将左转和前进的函数名互换即可
int gpLb = 2; 
int gpLf = 14;
int gpRb = 15;
int gpRf = 13;

int gpLed =  4; // Light
String WiFiAddr ="";

void startCameraServer();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);  //关掉 Debug 输出减少卡顿
  Serial.println();


  pinMode(gpLb, OUTPUT); //Left Backward
  pinMode(gpLf, OUTPUT); //Left Forward
  pinMode(gpRb, OUTPUT); //Right Forward
  pinMode(gpRf, OUTPUT); //Right Backward
  pinMode(gpLed, OUTPUT); //Light

  // ADC setup for battery monitoring
  analogReadResolution(12);  // 12-bit resolution
  pinMode(BATTERY_PIN, INPUT);

  // PWM setup for continuous motor control
  #define PWM_FREQ  1000
  #define PWM_RES   8   // 0~255
  ledcSetup(0, PWM_FREQ, PWM_RES); // Left Forward
  ledcSetup(1, PWM_FREQ, PWM_RES); // Left Backward
  ledcSetup(2, PWM_FREQ, PWM_RES); // Right Forward
  ledcSetup(3, PWM_FREQ, PWM_RES); // Right Backward
  ledcAttachPin(gpLf, 0);
  ledcAttachPin(gpLb, 1);
  ledcAttachPin(gpRf, 2);
  ledcAttachPin(gpRb, 3);

  //initialize
  digitalWrite(gpLb, LOW);
  digitalWrite(gpLf, LOW);
  digitalWrite(gpRb, LOW);
  digitalWrite(gpRf, LOW);
  digitalWrite(gpLed, LOW);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // ===== 低延迟优化配置 =====
  // CAMERA_GRAB_LATEST: 只获取最新帧，丢弃旧帧，消除帧缓冲积压延迟
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;  // 帧缓冲放在 PSRAM
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA;   // 320x240 for stability
    config.jpeg_quality = 50;   // 数字越大 → 质量越低 → 编码更快 → 延迟更低
    config.fb_count = 2;        // 双缓冲：采集和发送并行
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 55;   // 无PSRAM时进一步降低质量
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  //drop down frame size for higher initial frame rate
  sensor_t * s = esp_camera_sensor_get();
  // s->set_framesize(s, FRAMESIZE_CIF);  //最高摄像头画质
  // s->set_framesize(s, FRAMESIZE_QVGA); // 320x240
  s->set_framesize(s, FRAMESIZE_QQVGA); // 160x120



  // 修正画面方向
  // s->set_vflip(s, 1);    // 垂直翻转
  // s->set_hmirror(s, 1);  // 水平镜像


  // 配置静态IP (如果启用)
  #if USE_STATIC_IP
    if (!WiFi.config(staticIP, gateway, subnet, dns1, dns2)) {
      Serial.println("Static IP configuration failed!");
    } else {
      Serial.println("Static IP configured: " + staticIP.toString());
    }
  #endif

  WiFi.begin(ssid, password);

  int attempts = 0;
  const int maxAttempts = 20;  // 10 seconds (20 * 500ms)
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");

    // ===== WiFi 低延迟优化 =====
    WiFi.setSleep(false);                    // 禁用 WiFi 省电模式
    esp_wifi_set_ps(WIFI_PS_NONE);           // ESP-IDF 级别禁用省电
    WiFi.setTxPower(WIFI_POWER_19_5dBm);     // 最大发射功率
    Serial.println("WiFi power saving disabled, max TX power set");

    // 先设置 WiFiAddr，再启动服务器（网页生成需要此变量）
    WiFiAddr = WiFi.localIP().toString();

    startCameraServer();

    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");
  } else {
    Serial.println("");
    Serial.println("Failed to connect to WiFi after multiple attempts. Restarting...");
    delay(2000);
    ESP.restart();  // Restart ESP32 to try again
  }
}

void loop() 
{
  // ===== 优化：减少 WiFi 检查频率，使用 FreeRTOS 延迟释放 CPU =====
  static unsigned long lastWiFiCheck = 0;
  unsigned long now = millis();
  
  if (now - lastWiFiCheck > 5000) {  // 每5秒检查一次 WiFi
    lastWiFiCheck = now;
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, reconnecting...");
      WiFi.reconnect();
      
      unsigned long startAttemptTime = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
        vTaskDelay(pdMS_TO_TICKS(500));  // FreeRTOS 延迟，释放 CPU
        Serial.print(".");
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Reconnected to WiFi");
        // 重新应用低延迟设置
        WiFi.setSleep(false);
        esp_wifi_set_ps(WIFI_PS_NONE);
      }
    }
  }
  
  vTaskDelay(pdMS_TO_TICKS(100));  // 使用 FreeRTOS 延迟，释放 CPU 给其他任务
}
