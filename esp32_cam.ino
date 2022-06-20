
#include <WiFi.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"

int dir = 0;

void back() {
  digitalWrite(12, 1);
  digitalWrite(13, 0);
  
  digitalWrite(14, 1);
  digitalWrite(15, 0);
}

void front() {
  digitalWrite(12, 0);
  digitalWrite(13, 1);
  
  digitalWrite(14, 0);
  digitalWrite(15, 1);
}

void stop1() {
  digitalWrite(12, 0);
  digitalWrite(13, 0);
  
  digitalWrite(14, 0);
  digitalWrite(15, 0);
} 

void right() {
  digitalWrite(12, 0);
  digitalWrite(13, 1);
  
  digitalWrite(14, 1);
  digitalWrite(15, 0);
}

void left() {
  digitalWrite(12, 1);
  digitalWrite(13, 0);
  
  digitalWrite(14, 0);
  digitalWrite(15, 1);
}

const char* ssid = "Mottanet";
const char* password = "991132826";

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];
 
  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK){
    return res;
  }
 
  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if(fb->width > 400){
        if(fb->format != PIXFORMAT_JPEG){
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if(!jpeg_converted){
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
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
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){
      break;
    }
    //Serial.printf("MJPG: %uB\n",(uint32_t)(_jpg_buf_len));
  }
  return res;
}

static esp_err_t cmd_handler(httpd_req_t *req){
  char*  buf;
  size_t buf_len;
  char variable[32] = {0,};
  
  buf_len = httpd_req_get_url_query_len(req) + 1;
  buf = (char*)malloc(buf_len);
   Serial.println(buf);
  esp_err_t h = httpd_req_get_url_query_str ( req , buf ,  buf_len ) ;
  Serial.println(buf);
  httpd_query_key_value(buf, "go", variable, sizeof(variable));
  free(buf);
  Serial.println( variable );
  
  if( strcmp("front", variable) == 0) {
    dir = 1;
  }else if( strcmp("back", variable) == 0 ){
    dir = 2;
  }else if( strcmp("right", variable) == 0 ){
    dir = 3; 
  }else if( strcmp("left", variable) == 0 ){
    dir = 4;
  }else if( strcmp("stop", variable) == 0 ){
    dir = 5;
  }else if( strcmp("flashon", variable) == 0 ){
   digitalWrite(4, 1); 
  }else if( strcmp("flashoff", variable) == 0 ){
   digitalWrite(4, 0); 
  }else {
    const char resp[] = R"(
      <h1>ESP32-CAM Robot</h1>
      <img src="/" id="photo" >
      <button class="button" onclick='toggle("front")' >front</button>
      <button class="button" onclick='toggle("back")' >back</button>
      <button class="button" onclick='toggle("right")' >right</button>
      <button class="button" onclick='toggle("left")' >left</button>
      <button class="button" onclick='toggle("stop")' >stop</button>
      <button class="button" onclick='toggle("flashon")' >flashon</button>
      <button class="button" onclick='toggle("flashoff")' >flashoff</button>
     <script>
     function toggle(x) {
       fetch("/a?go=" + x);
     }
     window.onload = () => document.getElementById('photo').src = 'http://' + window.location.host + ':81';
    </script>)";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);  
    return ESP_OK;
  }
  httpd_resp_send(req, "ok", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

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

httpd_uri_t uri_get = {
    .uri      = "/a",
    .method   = HTTP_GET,
    .handler  = cmd_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_stream = {
  .uri      = "/",
  .method   = HTTP_GET,
  .handler  = stream_handler,
  .user_ctx = NULL
};

void startServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  /* Empty handle to esp_http_server */
  httpd_handle_t server = NULL;
  httpd_handle_t stream_httpd = NULL;

  /* Start the httpd server */
  if (httpd_start(&server, &config) == ESP_OK) {
      /* Register URI handlers */
      httpd_register_uri_handler(server, &uri_get);
  }

  config.server_port += 1;
  config.ctrl_port += 1;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &uri_stream);
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_SVGA; //FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 2;

  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  }
   
  // Iniciação da câmera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  /**
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    Serial.println(".");
  }
   
  Serial.println(WiFi.localIP());
  **/

  const char* n = "esp32-cam";
  const char* p = "12345678";
  WiFi.softAP(n, p);
  startServer();
  pinMode(4, OUTPUT);
  
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  
  pinMode(14, OUTPUT);
  pinMode(15, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  if(dir == 1) {
    front();
  }

  if(dir == 2) {
    back();
  }

  if(dir == 3) {
    right();
  }

  if(dir == 4) {
    left();
  }

  if(dir == 5) {
    stop1();
  }
  
  if(dir != 0){
    delay(100);
    dir = 0;
    stop1();
  }
}
