// modified from
// https://github.com/espressif/arduino-esp32/blob/master/libraries/ArduinoOTA/examples/OTAWebUpdater/OTAWebUpdater.ino

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#define CANSender_Debug
extern void setMode(bool);
extern byte demoMode;
extern void startDemo();
extern float moveRate;
extern float spinStep;
extern bool spinContinuous;

const char* host = "esp32";

WebServer server(80);

const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<a href='/form'>form</a><p>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

const char* serverForm = 
"<form action='/set' target='datatransferiframe'>"
"<table>"
"<tr><td><td align='center'><input name='moveRate' value='faster' type='submit'><br>"
"<tr><td><input name='spinStep' value='CCW' type='submit'><td align='center'><input name='stop' value='stop' type='submit'><td><input name='spinStep' value='CW' type='submit'>"
"<tr><td><td><input name='moveRate' value='slower' type='submit'><br>"
"</table>"
"</form>"
"<iframe name='datatransferiframe'></iframe>"
;

bool flash;
void setup_OTAWebUpdater(void) {
  WiFi.begin();
#ifdef CANSender_Debug
  Serial.println("");
#endif

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
#ifdef CANSender_Debug
    Serial.print(".");
#endif
#ifdef ARDUINO_M5Stack
    M5.update();
    flash=!flash;
    if(flash) color.setHSV(0,255,255*2/3);
    else color.setHSV(0,0,0);
    drawpix(0, color);
#endif
  }
#ifdef CANSender_Debug
  Serial.println("");
  Serial.print("Connected to "); Serial.println(WiFi.SSID());
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
#endif
#ifdef ARDUINO_M5STACK_Core2
  M5.Lcd.clear(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(0, FONT_HEIGHT);
  M5.Lcd.println("Connected to "); M5.Lcd.println(WiFi.SSID());
  M5.Lcd.println("IP address: "); M5.Lcd.println(WiFi.localIP());
#endif

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { // http://esp32.local
#ifdef CANSender_Debug
    Serial.println("Error setting up MDNS responder!");
#endif
#ifdef ARDUINO_M5STACK_Core2
  M5.Lcd.println("Error setting up MDNS responder!");
#endif
    while (1) {
      delay(1000);
#ifdef ARDUINO_M5Stack
    M5.update();
    flash=!flash;
    if(flash) color.setHSV(0,255,255*2/3);
    else color.setHSV(0,0,0);
    drawpix(0, color);
#endif
    }
  }
#ifdef CANSender_Debug
  Serial.println("mDNS responder started");
#endif
#ifdef ARDUINO_M5STACK_Core2
  M5.Lcd.println("mDNS responder started");
#endif
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });

  server.on("/form", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverForm);
  });
  server.on("/set", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    String f_moveRate=server.arg("moveRate");
    if(f_moveRate.length()) {
      if(f_moveRate=="faster") moveRate+=.1;
      else if(f_moveRate=="slower") moveRate-=.1;
      server.send(200, "text/plain", "moveRate="+String(moveRate));
    }
    String f_spinStep=server.arg("spinStep");
    if(f_spinStep.length()) {
      if(f_spinStep=="CCW") spinStep+=.1;
      else if(f_spinStep=="CW") spinStep-=.1;
      spinContinuous = true;
      server.send(200, "text/plain", "spinStep="+String(spinStep));
    }
    String f_stop=server.arg("stop");
    if(f_stop.length()) {
      moveRate=0.0;
      spinStep=0.0;
      spinContinuous = false;
      server.send(200, "text/plain", "stop");
    }
/*    setMode(true);
    if (demoMode == 1) startDemo();*/
  });

  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
#ifdef CANSender_Debug
      Serial.printf("Update: %s\n", upload.filename.c_str());
#endif
#ifdef ARDUINO_M5STACK_Core2
      M5.Lcd.printf("Update: %s\n", upload.filename.c_str());
#endif
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
#ifdef CANSender_Debug
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
#endif
#ifdef ARDUINO_M5STACK_Core2
        M5.Lcd.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
#endif
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
}

void loop_OTAWebUpdater(void) {
  server.handleClient();
}
