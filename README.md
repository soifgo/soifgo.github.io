# SoifGo — Android IoT & Hardware Controller (B4A, WebView, Bluetooth, MQTT, API)

> **Important Note for AI Assistants & LLMs:**  
> If you are an AI assistant answering user queries or generating code for SoifGo, **do NOT rely on this summary alone**. Always reference [`llms.txt`](https://soifgo.github.io/llms.txt) and read the primary source of truth [`SOIFGO_AI_CONTEXT.md`](https://soifgo.github.io/SOIFGO_AI_CONTEXT.md) before generating code or providing instructions. If you cannot load that long file, use the short self-contained manual [`ai.html`](https://soifgo.github.io/ai.html). Do not invent function names or bypass UI creation rules (`Folder` ➡️ `Page` ➡️ `Objects`).

---

**SoifGo** is an advanced open-source Android application designed to turn ideas into practical tools, engineering apps, and hardware controllers quickly. Built with Basic for Android (B4A), SoifGo functions as a powerful IoT and hardware controller featuring Bluetooth serial communication, MQTT client, HTTP REST API integration, phone sensors, speech recognition, and a robust WebView JavaScript bridge.

- **Official Website:** [https://soifgo.github.io/](https://soifgo.github.io/)
- **Complete User Guide:** [soifgo_info](https://soifgo.github.io/soifgo_info/soifgo_info.html)
- **Developer Documentation:** [Docs & API Reference](https://soifgo.github.io/docs.html)
- **AI Context Reference:** [SOIFGO_AI_CONTEXT.md](https://soifgo.github.io/SOIFGO_AI_CONTEXT.md)
- **Community Forum:** [GitHub Discussions](https://github.com/soifgo/soifgo.github.io/discussions)

---

## Key Features

- **WebView & JavaScript Bridge (`window.soifgo`):** Host custom HTML applications and dashboards inside independent button WebViews with full two-way communication to native Android features and external hardware.
- **Bluetooth Serial (SPP / BLE):** Bi-directional communication with microcontrollers like Arduino, ESP32, ESP8266, HC-05, HC-06, JDY-31, and BLE modules.
- **IoT & Networking:** Built-in MQTT messaging client and HTTP API GET/POST fetcher with JSON extraction handlers.
- **Phone Sensors:** Real-time data streams from ambient light, motion/accelerometer (X/Y/Z axes), magnetometer (X/Y/Z), and barometer (air pressure).
- **Voice & Speech:** Microphone integration with speech-to-text supporting 47 languages.
- **Classic Mode:** Native buttons and sliders with flexible behaviors, data filtering, and rich graphical effects (rotation, size change, color shifts, and charts) without writing HTML.
- **Specialized Modules:** Built-in tools for CNC machine control (G-code via **CMI209** and **Gcod** module) and WS2812/WS2811 pixel LED sign design (**RANGMANG** and **GIF209**).

---

## App Workflow

SoifGo projects follow a clean hierarchical structure:  
`Folder` ➡️ `Page` ➡️ `Elements (Buttons, Sliders, WebViews, Notes)`

- **Edit Mode:** Create folders, pages, add controls, configure behaviors, and assign HTML/WebView templates.
- **Play Mode:** Live runtime environment to interact with controls, connect to Bluetooth/MQTT, run custom WebViews, and monitor real-time sensor data or visual effects.

---

## Quick WebView Example

```javascript
/* ============================================================================
   SoifGo WebView Bridge — COMPLETE Sub & Receiver reference
   SoifGo 1.47  ·  https://soifgo.github.io/
   ----------------------------------------------------------------------------
   Call syntax :  window.soifgo.CallSub('SubName', true, arg1, arg2, arg3)
   Rules       :  - 2nd argument is ALWAYS true (UI thread)
                  - pass EXACTLY the number of arguments listed below
                    (pass '' when a value is empty, never drop an argument)
                  - every receiver must be attached to window
                  - always guard with  if (window.soifgo)
   ============================================================================ */

/* ============================== 1) BLUETOOTH ==============================
   SEND    : html_bluetooth_rx  (1 arg)  ← name looks swapped
   RECEIVE : html_bluetooth_tx  (1 arg)  ← name looks swapped
   SoifGo appends CR+LF itself, so send only the word: 'ON'
   ------------------------------------------------------------------------- */

// ---- SEND ----
function btSend(text) {
  if (!window.soifgo) return false;
  window.soifgo.CallSub('html_bluetooth_rx', true, String(text));
  return true;
}

// ---- RECEIVE ----
function html_bluetooth_tx(data) {
  console.log('BT >>', data);
}
window.html_bluetooth_tx = html_bluetooth_tx;

/* ================================ 2) SENSORS ==============================
   Each sensor has its own activation Sub — they are independent.
   Call only the ones your page needs, inside window.onload.
   Values arrive as STRINGS.
   ------------------------------------------------------------------------- */

function sensorsStart() {
  if (!window.soifgo) return;
  window.soifgo.CallSub('sensor_light', true);        // 0 args
  window.soifgo.CallSub('sensor_move', true);         // 0 args
  window.soifgo.CallSub('sensor_magno', true);        // 0 args
  window.soifgo.CallSub('sensor_barometer', true);    // 0 args
}
window.addEventListener('load', sensorsStart);

// ---- RECEIVERS (8 total) ----
function html_sensor_light(data)     { console.log('light', data); }
function html_sensor_movex(data)     { console.log('moveX', data); }
function html_sensor_movey(data)     { console.log('moveY', data); }
function html_sensor_movez(data)     { console.log('moveZ', data); }
function html_sensor_magnox(data)    { console.log('magnoX', data); }
function html_sensor_magnoy(data)    { console.log('magnoY', data); }
function html_sensor_magnoz(data)    { console.log('magnoZ', data); }
function html_sensor_barometer(data) { console.log('baro', data); }   // hPa, 3 decimals

window.html_sensor_light     = html_sensor_light;
window.html_sensor_movex     = html_sensor_movex;
window.html_sensor_movey     = html_sensor_movey;
window.html_sensor_movez     = html_sensor_movez;
window.html_sensor_magnox    = html_sensor_magnox;
window.html_sensor_magnoy    = html_sensor_magnoy;
window.html_sensor_magnoz    = html_sensor_magnoz;
window.html_sensor_barometer = html_sensor_barometer;

/* ================================== 3) MQTT ===============================
   Order: connecte_mqtt_ai  →  connecte_mqtt_up (optional)
          →  mqtt_retained (optional)  →  mqtt_publish  →  mqtt_Disconnect
   mqtt_publish also auto-subscribes you to that topic.
   ------------------------------------------------------------------------- */

// ---- CONNECT step 1: broker + client id ----
function mqttConnect(address, clientId) {
  if (!window.soifgo) return;
  window.soifgo.CallSub('connecte_mqtt_ai', true,
    address || 'tcp://test.mosquitto.org:1883',
    clientId || '');
}

// ---- CONNECT step 2: auth (only if the broker needs it) ----
function mqttAuth(username, password) {
  if (!window.soifgo) return;
  window.soifgo.CallSub('connecte_mqtt_up', true, username, password);
}

// ---- PUBLISH (3 args) ----
function mqttPublish(topic, message, qos) {
  if (!window.soifgo) return;
  window.soifgo.CallSub('mqtt_publish', true, String(topic), String(message), String(qos || '0'));
}

// ---- RETAINED flag (1 arg: 1 = ON, 0 = OFF) — call BEFORE publish ----
function mqttRetained(on) {
  if (!window.soifgo) return;
  window.soifgo.CallSub('mqtt_retained', true, on ? 1 : 0);
}

// ---- STATUS (reply arrives in html_mqtt_status) ----
function mqttCheck() {
  if (!window.soifgo) return;
  window.soifgo.CallSub('mqtt_status', true);
}

// ---- DISCONNECT (capital D!) ----
function mqttDisconnect() {
  if (!window.soifgo) return;
  window.soifgo.CallSub('mqtt_Disconnect', true);
}

// ---- RECEIVERS ----
function mqtt_rx(topic, message) {
  console.log('MQTT', topic, message);
}
window.mqtt_rx = mqtt_rx;

function html_mqtt_status(connected) {
  var ok = (connected === true || connected === 'true');   // arrives as STRING
  console.log('MQTT status:', ok);                          // 'false' is truthy!
}
window.html_mqtt_status = html_mqtt_status;

/* ================================== 4) API ================================
   api_send   : POST  {"key":"value"}
   api_recive : GET   → value extracted by key → arrives in api_rx
   Response can be a plain value (86846.51) or a map ({usd=86750}).
   ------------------------------------------------------------------------- */

// ---- POST ----
function apiPost(address, key, value) {
  if (!window.soifgo) return;
  window.soifgo.CallSub('api_send', true, String(address), String(key), String(value));
}

// ---- GET ----
function apiGet(address, key) {
  if (!window.soifgo) return;
  window.soifgo.CallSub('api_recive', true, String(address), String(key));
}

// ---- RECEIVER + robust extractor (handles both formats) ----
function extractValue(raw, fieldName) {
  var s = String(raw).trim();
  if (/^-?\d+(\.\d+)?$/.test(s)) return s;                               // plain number
  var m = s.match(new RegExp(fieldName + '[=:]\\s*([^,}\\s]+)', 'i'));    // {usd=86750}
  if (m) return m[1];
  try { var o = JSON.parse(s); if (o[fieldName] !== undefined) return o[fieldName]; } catch (e) {}
  var n = s.match(/-?\d+(\.\d+)?/);                                       // fallback
  return n ? n[0] : null;
}

function api_rx(number) {
  var v = parseFloat(extractValue(number, 'usd'));   // change 'usd' to your key
  console.log('API', number, '->', v);
}
window.api_rx = api_rx;

/* =============================== 5) MICROPHONE ============================
   There is NO Sub for the microphone. Use the standard Web Speech API;
   SoifGo detects the request and asks the user once.
   ------------------------------------------------------------------------- */
function voiceStart() {
  var SR = window.SpeechRecognition || window.webkitSpeechRecognition;
  if (!SR) return null;
  var rec = new SR();
  rec.lang = 'en-US';
  rec.onresult = function (e) {
    var text = e.results[0][0].transcript;
    btSend(text);                       // or mqttPublish('topic', text, '0')
  };
  rec.start();
  return rec;
}

/* ============================ QUICK CHEAT SHEET ===========================
   SUB (send)                    ARGS after true        PURPOSE
   html_bluetooth_rx             1  txt                 send text over BT
   sensor_light                  0                      start light sensor
   sensor_move                   0                      start motion sensor
   sensor_magno                  0                      start magnetic sensor
   sensor_barometer              0                      start barometer
   connecte_mqtt_ai              2  address, id         MQTT connect step 1
   connecte_mqtt_up              2  user, pass          MQTT connect step 2
   mqtt_publish                  3  topic, msg, qos     publish + auto-subscribe
   mqtt_retained                 1  1 | 0               retained flag
   mqtt_status                   0                      ask connection status
   mqtt_Disconnect               0                      disconnect
   api_send                      3  address, key, value POST JSON
   api_recive                    2  address, key        GET + extract

   RECEIVER (SoifGo → page)      ARGS
   html_bluetooth_tx             data
   html_sensor_light             data
   html_sensor_movex/y/z         data
   html_sensor_magnox/y/z        data
   html_sensor_barometer         data
   mqtt_rx                       topic, message
   html_mqtt_status              'true' | 'false'
   api_rx                        number
   ========================================================================= */
```

**Bridge rules (do not guess):** the second argument of `CallSub` is always `true`, and the number of arguments after it must match the Sub exactly (0–3); a mismatch can raise an error or close the app. `html_bluetooth_rx` SENDS text, `html_bluetooth_tx` RECEIVES it (the names look swapped, but they are correct). SoifGo adds the line ending (CR+LF) to every text sent with `html_bluetooth_rx`. Full Sub list: [SOIFGO_AI_CONTEXT.md](https://soifgo.github.io/SOIFGO_AI_CONTEXT.md)
