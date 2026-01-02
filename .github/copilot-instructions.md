# ESP32CAM Car Project Instructions

## Project Overview
This is an Arduino-based IoT project for a remote-controlled car using ESP32 with camera module. It provides live video streaming and motor control via web interface over WiFi.

## Architecture
- **ESP32CAM_Car.ino**: Main setup file - initializes camera, WiFi, and HTTP server.
- **app_httpd.cpp**: HTTP server implementation with handlers for camera streaming and motor control.
- **camera_index.h**: Gzipped HTML/JS web interface for control buttons and video display.

## Key Patterns
- Motor control uses `WheelAct(int nLf, int nLb, int nRf, int nRb)` function with GPIO pins:
  - Forward: `WheelAct(LOW, HIGH, HIGH, LOW)`
  - Backward: `WheelAct(HIGH, LOW, LOW, HIGH)`
  - Left: `WheelAct(LOW, HIGH, LOW, HIGH)`
  - Right: `WheelAct(HIGH, LOW, HIGH, LOW)`
  - Stop: `WheelAct(LOW, LOW, LOW, LOW)`
- GPIO assignments: gpLf=2, gpLb=14, gpRf=15, gpRb=13 (motors); gpLed=4 (LED light).
- HTTP endpoints: `/go`, `/back`, `/left`, `/right`, `/stop`, `/ledon`, `/ledoff`, `/capture`, `/stream`.
- Web UI buttons trigger GET requests to control endpoints.

## Development Workflow
- Use Arduino IDE with ESP32 board support.
- Select "ESP32 Wrover Module" and enable PSRAM in board settings.
- Update WiFi credentials in `ESP32CAM_Car.ino`: `const char* ssid = "your_wifi_name"; const char* password = "your_wifi_password";`
- Camera model: `#define CAMERA_MODEL_AI_THINKER` (adjust pins if using different model).
- If motor directions are reversed, swap handler functions in `app_httpd.cpp` (e.g., exchange `go_handler` and `back_handler`).
- Camera settings: JPEG format, QQVGA resolution for smooth streaming; adjust `jpeg_quality` (higher = lower quality, faster).

## Conventions
- Serial debug output disabled by default: `Serial.setDebugOutput(false);`
- Camera vertical flip enabled: `s->set_vflip(s, 1);`
- PSRAM check for higher frame buffers: `if(psramFound()) config.fb_count = 2;`

## Deployment
- Compile and upload via Arduino IDE.
- Access web interface at ESP32's IP address after WiFi connection.
- Monitor serial output for IP address and connection status.