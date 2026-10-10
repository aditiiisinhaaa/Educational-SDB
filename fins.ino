#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>

const char* ssid     = "Students 1";
const char* password = "FutureisTech";

Servo frontFin;    
Servo rightFin;    
Servo backFin;     
Servo leftFin;     
Servo rightWing;  
Servo leftWing;   

const int pinFront  = 23;
const int pinRight  = 19;
const int pinBack   = 5;
const int pinLeft   = 18;
const int pinRWing  = 4;
const int pinLWing  = 2;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

unsigned long lastMessageTime = 0;
const unsigned long CONTROL_TIMEOUT = 2000;  

void returnToNeutral() {
  Serial.println("CONTROL LOST → Returning all actuators to neutral (90°)");
  frontFin.write(90);
  rightFin.write(90);
  backFin.write(90);
  leftFin.write(90);
  rightWing.write(90);
  leftWing.write(90);
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("Client #%u connected\n", client->id());
    lastMessageTime = millis();
  } 
  else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("Client #%u disconnected\n", client->id());
  } 
  else if (type == WS_EVT_DATA) {
    lastMessageTime = millis();

    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len) {
      data[len] = 0;
      String message = (char*)data;
      message.trim();

      int colon = message.indexOf(':');
      if (colon == -1) return;

      String servoStr = message.substring(0, colon);
      servoStr.trim();
      servoStr.toLowerCase();

      int angle = message.substring(colon + 1).toInt();
      angle = constrain(angle, 0, 180);

      // Use writeMicroseconds for potentially smoother / more precise control
      int us = map(angle, 0, 180, 500, 2500);  // same range you used in attach

      if (servoStr == "front" || servoStr == "frontfin") {
        frontFin.writeMicroseconds(us);
      }
      else if (servoStr == "right" || servoStr == "rightfin") {
        rightFin.writeMicroseconds(us);
      }
      else if (servoStr == "back" || servoStr == "backfin") {
        backFin.writeMicroseconds(us);
      }
      else if (servoStr == "left" || servoStr == "leftfin") {
        leftFin.writeMicroseconds(us);
      }
      else if (servoStr == "rwing" || servoStr == "rightwing") {
        rightWing.writeMicroseconds(us);
      }
      else if (servoStr == "lwing" || servoStr == "leftwing") {
        leftWing.writeMicroseconds(us);
      }

      Serial.printf("%s → %d° (%d µs)\n", servoStr.c_str(), angle, us);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Allocate timers (good practice - you already had this)
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  frontFin.attach(pinFront, 500, 2500);
  rightFin.attach(pinRight, 500, 2500);
  backFin.attach(pinBack, 500, 2500);
  leftFin.attach(pinLeft, 500, 2500);
  rightWing.attach(pinRWing, 500, 2500);
  leftWing.attach(pinLWing, 500, 2500);

  returnToNeutral();
  delay(800);           // longer delay helps servos settle

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP: http://");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>SDB Fin Control</title>
  <style>
    body {font-family:Arial; background:#000; color:#0f9; text-align:center; padding:20px;}
    h1 {color:#f00; text-shadow:0 0 10px #f00; font-size:2.5em;}
    .servo {margin:25px auto; max-width:480px; padding:18px; background:#200; border:2px solid #f00; border-radius:12px; box-shadow:0 0 18px #f005;}
    .name {font-size:1.7em; margin-bottom:8px; color:#f33;}
    input[type=range] {width:100%; height:48px; -webkit-appearance:none; background:#400; border-radius:24px;}
    input[type=range]::-webkit-slider-thumb {-webkit-appearance:none; width:58px; height:58px; background:#f00; border-radius:50%; box-shadow:0 0 14px #f00;}
    .value {font-size:2.3em; margin-top:12px; color:#f90;}
  </style>
</head>
<body>
  <h1>Small Diameter Bomb<br>Fin & Wing Control</h1>

  <div class="servo"><div class="name">Front Fin</div><input type="range" min="0" max="180" value="90" id="front"><div class="value"><span id="vfront">90</span>°</div></div>
  <div class="servo"><div class="name">Right Fin</div><input type="range" min="0" max="180" value="90" id="right"><div class="value"><span id="vright">90</span>°</div></div>
  <div class="servo"><div class="name">Back Fin</div><input type="range" min="0" max="180" value="90" id="back"><div class="value"><span id="vback">90</span>°</div></div>
  <div class="servo"><div class="name">Left Fin</div><input type="range" min="0" max="180" value="90" id="left"><div class="value"><span id="vleft">90</span>°</div></div>
  <div class="servo"><div class="name">Right Wing</div><input type="range" min="0" max="180" value="90" id="rwing"><div class="value"><span id="vrwing">90</span>°</div></div>
  <div class="servo"><div class="name">Left Wing</div><input type="range" min="0" max="180" value="90" id="lwing"><div class="value"><span id="vlwing">90</span>°</div></div>

<script>
  var ws = new WebSocket("ws://" + location.hostname + "/ws");
  var lastSend = 0;
  const throttleMs = 40;   // send max ~25 times/sec - smooth but not flooding

  const sliders = {
    front:  document.getElementById("front"),
    right:  document.getElementById("right"),
    back:   document.getElementById("back"),
    left:   document.getElementById("left"),
    rwing:  document.getElementById("rwing"),
    lwing:  document.getElementById("lwing")
  };

  Object.keys(sliders).forEach(key => {
    let prevVal = 90;
    sliders[key].addEventListener("input", function() {
      let val = parseInt(this.value);
      document.getElementById("v" + key).innerText = val;

      const now = Date.now();
      if (val !== prevVal && now - lastSend >= throttleMs) {
        ws.send(key + ":" + val);
        prevVal = val;
        lastSend = now;
      }
    });

    // Optional: send final value on release (pointerup/touchend)
    sliders[key].addEventListener("pointerup", function() {
      let val = parseInt(this.value);
      ws.send(key + ":" + val);   // ensure last position is sent
    });
    sliders[key].addEventListener("touchend", function() {
      let val = parseInt(this.value);
      ws.send(key + ":" + val);
    });
  });

  ws.onopen    = () => console.log("Connected");
  ws.onclose   = () => alert("Connection lost – check WiFi");
  ws.onerror   = (err) => console.error("WebSocket error:", err);
</script>
</body>
</html>
)rawliteral");
  });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();
  Serial.println("Server started");
}

void loop() {
  ws.cleanupClients();

  if (millis() - lastMessageTime > CONTROL_TIMEOUT) {
    static bool safe = false;
    if (!safe) {
      returnToNeutral();
      safe = true;
    }
  } else {
    static bool safe = false;
    if (safe) safe = false;
  }

  delay(5);   // slightly shorter delay
}