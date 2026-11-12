#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include <ArduinoJson.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// --- Access Point Settings ---
const char *ap_ssid = "bio-mouse";
const char *ap_password = "12345678";

IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// --- Pin Definitions for AI-Thinker ESP32-CAM ---
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
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

#define FLASH_LED_PIN      4

static httpd_handle_t server = NULL;
String serialLineBuffer = "";
String latestTelemetry = "{\"d\":0,\"p\":[]}";

// --- Unified Web Dashboard (HTML + CSS + JS) ---
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>BIO-MOUSE Cockpit</title>
  <style>
    :root {
      --primary: #A51C30;
      --bg-dark: #1B1212;
      --card-bg: #261B1B;
      --accent: #F4BB44;
      --text: #FAFAFA;
      --text-muted: #9E9494;
      --danger: #e53935;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; -webkit-user-select: none; touch-action: manipulation; }
    body { background: var(--bg-dark); color: var(--text); font-family: -apple-system, BlinkMacSystemFont, sans-serif; display: flex; flex-direction: column; align-items: center; min-height: 100vh; padding: 10px; }
    header { width: 100%; max-width: 440px; display: flex; justify-content: space-between; align-items: center; padding: 8px 14px; background: var(--card-bg); border-radius: 12px; border: 1px solid rgba(255,255,255,0.08); margin-bottom: 10px; }
    .brand { font-weight: 700; letter-spacing: 1px; color: var(--accent); font-size: 1rem; }
    .badge { font-size: 0.7rem; padding: 3px 8px; border-radius: 20px; font-weight: 600; display: flex; align-items: center; gap: 5px; color: #81c784; background: rgba(76, 175, 80, 0.15); }
    .dot { width: 7px; height: 7px; border-radius: 50%; background: currentColor; }
    .alert-banner { display: none; width: 100%; max-width: 440px; background: var(--danger); color: #fff; font-weight: 700; text-align: center; padding: 10px; border-radius: 10px; margin-bottom: 10px; font-size: 0.85rem; letter-spacing: 0.5px; }
    .mode-card, .video-card, .thermal-card, .telemetry-row { width: 100%; max-width: 440px; background: var(--card-bg); border-radius: 12px; padding: 10px; border: 1px solid rgba(255,255,255,0.06); margin-bottom: 10px; display: flex; flex-direction: column; align-items: center; }
    .mode-card { flex-direction: row; justify-content: space-between; padding: 10px 14px; }
    select { background: var(--bg-dark); color: var(--text); border: 1px solid var(--accent); padding: 6px 10px; border-radius: 8px; font-weight: 600; font-size: 0.8rem; outline: none; }
    .video-feed { width: 100%; height: 210px; object-fit: cover; border-radius: 8px; background: #000; border: 1px solid rgba(255,255,255,0.1); }
    .telemetry-row { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; padding: 0; background: transparent; border: none; }
    .card { background: var(--card-bg); padding: 10px; border-radius: 12px; border: 1px solid rgba(255,255,255,0.06); display: flex; flex-direction: column; align-items: center; width: 100%; }
    .card-label { font-size: 0.7rem; text-transform: uppercase; letter-spacing: 0.5px; color: var(--text-muted); margin-bottom: 4px; }
    .card-val { font-size: 1.5rem; font-weight: 700; }
    .card-val small { font-size: 0.8rem; margin-left: 2px; }
    canvas { width: 140px; height: 140px; border-radius: 6px; image-rendering: pixelated; background: #000; border: 1px solid rgba(255,255,255,0.1); }
    .dpad-container { width: 100%; max-width: 440px; display: grid; grid-template-columns: repeat(3, 1fr); grid-template-rows: repeat(3, 60px); gap: 6px; margin-bottom: 10px; transition: opacity 0.2s; }
    .btn { background: var(--card-bg); border: 1px solid rgba(255,255,255,0.08); color: var(--text); font-weight: 700; border-radius: 12px; font-size: 1rem; display: flex; justify-content: center; align-items: center; cursor: pointer; }
    .btn:active, .btn.pressed { background: var(--primary); transform: scale(0.96); }
    .btn-forward  { grid-column: 2; grid-row: 1; }
    .btn-left     { grid-column: 1; grid-row: 2; }
    .btn-stop     { grid-column: 2; grid-row: 2; background: rgba(229, 57, 53, 0.2); color: var(--danger); }
    .btn-right    { grid-column: 3; grid-row: 2; }
    .btn-backward { grid-column: 2; grid-row: 3; }
    .actions-row { width: 100%; max-width: 440px; display: grid; grid-template-columns: 1fr 1fr; gap: 6px; }
    .btn-util { padding: 10px; background: var(--card-bg); border: 1px solid rgba(255,255,255,0.08); color: var(--accent); border-radius: 10px; font-weight: 600; font-size: 0.8rem; }
  </style>
</head>
<body>
  <header>
    <div class="brand">BIO-MOUSE COCKPIT</div>
    <div class="badge"><div class="dot"></div> Online</div>
  </header>

  <div class="alert-banner" id="trapBanner">⚠️ ROBOT TRAPPED: EXTRACTION REQUIRED</div>

  <div class="mode-card">
    <span class="card-label" style="margin:0;">Controller Mode</span>
    <select id="modeSelect" onchange="switchMode(this.value)">
      <option value="M">Manual D-Pad</option>
      <option value="A">Auto (Heat-Seeking)</option>
    </select>
  </div>

  <div class="video-card">
    <div class="card-label" style="margin-bottom:6px;">OV3660 Live Feed</div>
    <img id="camImg" class="video-feed" alt="Initializing camera...">
  </div>

  <div class="telemetry-row">
    <div class="card">
      <div class="card-label">Distance</div>
      <div class="card-val" id="distVal">--<small>cm</small></div>
    </div>
    <div class="card">
      <div class="card-label">Peak Thermal</div>
      <div class="card-val" id="tempVal">--<small>&deg;C</small></div>
    </div>
  </div>

  <div class="thermal-card">
    <div class="card-label" style="margin-bottom:6px;">AMG8833 Thermal Array (8x8)</div>
    <canvas id="thermalCanvas" width="8" height="8"></canvas>
  </div>

  <div class="dpad-container" id="dpad">
    <button class="btn btn-forward"  data-cmd="L">&#9650;</button>
    <button class="btn btn-left"     data-cmd="F">&#9664;</button>
    <button class="btn btn-stop"     data-cmd="S">STOP</button>
    <button class="btn btn-right"    data-cmd="B">&#9654;</button>
    <button class="btn btn-backward" data-cmd="R">&#9660;</button>
  </div>

  <div class="actions-row">
    <button class="btn btn-util" id="btnHorn">HORN ALERT</button>
    <button class="btn btn-util" id="btnStopAll" style="color:var(--danger)">EMERGENCY STOP</button>
  </div>

  <script>
    const camImg = document.getElementById('camImg');
    const canvas = document.getElementById('thermalCanvas');
    const ctx = canvas.getContext('2d');
    const distVal = document.getElementById('distVal');
    const tempVal = document.getElementById('tempVal');
    const trapBanner = document.getElementById('trapBanner');
    let activeCmd = 'S';
    let currentMode = 'M';

    function updateVideo() {
      camImg.src = '/capture?t=' + Date.now();
    }
    camImg.onload = () => setTimeout(updateVideo, 65);
    camImg.onerror = () => setTimeout(updateVideo, 500);
    updateVideo();

    setInterval(async () => {
      try {
        const res = await fetch('/telemetry', { cache: 'no-store' });
        if (!res.ok) return;
        const data = await res.json();
        
        trapBanner.style.display = (data.status === 'ROBOT_TRAPPED') ? 'block' : 'none';

        if (data.d !== undefined) {
          distVal.innerHTML = data.d + '<small>cm</small>';
          distVal.style.color = (data.d > 0 && data.d < 15) ? 'var(--danger)' : 'var(--text)';
        }
        if (data.p && data.p.length === 64) {
          renderThermal(data.p);
        }
      } catch (e) {}
    }, 350);

    setInterval(() => {
      if (currentMode === 'M' && activeCmd !== 'S') {
        sendCmd(activeCmd);
      } else if (currentMode === 'A') {
        sendCmd('A');
      }
    }, 800);

    function switchMode(mode) {
      currentMode = mode;
      activeCmd = 'S';
      sendCmd(mode);
      document.getElementById('dpad').style.opacity = (mode === 'A') ? '0.4' : '1.0';
    }

    function renderThermal(pixels) {
      let maxTemp = -999;
      const imgData = ctx.createImageData(8, 8);
      for (let i = 0; i < 64; i++) {
        const temp = pixels[i];
        if (temp > maxTemp) maxTemp = temp;
        const norm = Math.min(Math.max((temp - 20) / 20.0, 0), 1);
        const r = Math.floor(Math.min(Math.max(2.0 * norm - 0.5, 0), 1) * 255);
        const g = Math.floor(Math.min(Math.max(2.0 * (norm - 0.5), 0), 1) * 255);
        const b = Math.floor(Math.min(Math.max(1.0 - 2.0 * norm, 0), 1) * 255);
        const idx = i * 4;
        imgData.data[idx] = r; imgData.data[idx+1] = g; imgData.data[idx+2] = b; imgData.data[idx+3] = 255;
      }
      ctx.putImageData(imgData, 0, 0);
      tempVal.innerHTML = maxTemp.toFixed(1) + '<small>&deg;C</small>';
      tempVal.style.color = (maxTemp >= 31.0) ? 'var(--accent)' : 'var(--text)';
    }

    function sendCmd(cmd) {
      fetch('/cmd?val=' + cmd, { cache: 'no-store' }).catch(e => {});
    }

    document.querySelectorAll('.btn[data-cmd]').forEach(button => {
      const cmd = button.getAttribute('data-cmd');
      const start = (e) => {
        e.preventDefault();
        if (currentMode !== 'M') return;
        button.classList.add('pressed');
        activeCmd = cmd;
        sendCmd(cmd);
      };
      const stop = (e) => {
        e.preventDefault();
        if (currentMode !== 'M') return;
        button.classList.remove('pressed');
        if (cmd !== 'S') {
          activeCmd = 'S';
          sendCmd('S');
        }
      };
      button.addEventListener('touchstart', start, { passive: false });
      button.addEventListener('touchend', stop, { passive: false });
      button.addEventListener('mousedown', start);
      button.addEventListener('mouseup', stop);
    });

    document.getElementById('btnHorn').addEventListener('click', () => sendCmd('Z'));
    document.getElementById('btnStopAll').addEventListener('click', () => {
      activeCmd = 'S';
      document.getElementById('modeSelect').value = 'M';
      switchMode('M');
      sendCmd('S');
    });
  </script>
</body>
</html>
)rawliteral";

static esp_err_t capture_handler(httpd_req_t *req) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return res;
}

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t telemetry_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, latestTelemetry.c_str(), latestTelemetry.length());
}

static esp_err_t cmd_handler(httpd_req_t *req) {
    char buf[32];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char val = 0;
        if (sscanf(buf, "val=%c", &val) == 1) {
            Serial.write(val);
        }
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

void startServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;

    httpd_uri_t index_uri     = { .uri = "/",          .method = HTTP_GET, .handler = index_handler,     .user_ctx = NULL };
    httpd_uri_t capture_uri   = { .uri = "/capture",   .method = HTTP_GET, .handler = capture_handler,   .user_ctx = NULL };
    httpd_uri_t telemetry_uri = { .uri = "/telemetry", .method = HTTP_GET, .handler = telemetry_handler, .user_ctx = NULL };
    httpd_uri_t cmd_uri       = { .uri = "/cmd",       .method = HTTP_GET, .handler = cmd_handler,       .user_ctx = NULL };

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &index_uri);
        httpd_register_uri_handler(server, &capture_uri);
        httpd_register_uri_handler(server, &telemetry_uri);
        httpd_register_uri_handler(server, &cmd_uri);
    }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  Serial.begin(115200);
  delay(500);

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
  config.xclk_freq_hz = 16000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 14;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 14;
    config.fb_count = 1;
  }

  if (esp_camera_init(&config) != ESP_OK) {
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s && s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password);
  WiFi.setSleep(false);
  
  startServer();
}

void loop() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n') {
      serialLineBuffer.trim();
      if (serialLineBuffer.length() > 0 && serialLineBuffer.startsWith("{")) {
        latestTelemetry = serialLineBuffer;
      }
      serialLineBuffer = "";
    } else if (c != '\r') {
      serialLineBuffer += c;
    }
  }
}
