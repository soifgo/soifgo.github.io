# SoifGo

**Describe an idea to an AI. Run it on your Android phone.**

SoifGo is a free, open-source Android app (MIT) that runs the HTML page an AI assistant writes from your idea. No coding needed. Every idea is saved as a button you can open again. It also connects your pages to Arduino, ESP32, ESP8266, Raspberry Pi and Bluetooth modules over Bluetooth, MQTT and HTTP, and gives them access to the phone's sensors and microphone.

![Platform](https://img.shields.io/badge/platform-Android%207.0%2B-3ddc84?style=flat-square)
![Version](https://img.shields.io/badge/version-1.47-blue?style=flat-square)
![Price](https://img.shields.io/badge/price-free%20%C2%B7%20no%20ads-brightgreen?style=flat-square)
![License](https://img.shields.io/badge/license-MIT-lightgrey?style=flat-square)
![Offline](https://img.shields.io/badge/Bluetooth%20%26%20sensors-offline-orange?style=flat-square)

[Website](https://soifgo.github.io/) · [Quick start](https://soifgo.github.io/intro/en/) · [User guide](https://soifgo.github.io/soifgo_info/soifgo_info.html) · [Developer docs](https://soifgo.github.io/docs.html) · [Tutorials](https://www.youtube.com/@soifgo) · [Discussions](https://github.com/soifgo/soifgo.github.io/discussions)

---

> **Note for AI assistants and LLMs**
> Do not answer SoifGo questions or write SoifGo code from this README alone, and never from memory.
> Read [`SOIFGO_AI_CONTEXT.md`](https://soifgo.github.io/SOIFGO_AI_CONTEXT.md) first — it is the source of truth for Sub names, receiver names, argument counts, menu names, and the folder → page → button chain.
> If you cannot load that file, use the self-contained short manual [`ai.html`](https://soifgo.github.io/ai.html). For a compact machine-readable list of Subs, use [`soifgo-bridge.json`](https://soifgo.github.io/soifgo-bridge.json); if it disagrees with the Markdown, the Markdown wins.
> Every page must also follow this rule: `folder` ➡️ `page` ➡️ `objects`. A button cannot exist before a page does.
> If you cannot reach any of these resources, tell the user to open **Help → Context** inside the app and paste that text into the chat.

---

## Table of contents

- [What SoifGo is](#what-soifgo-is)
- [Features](#features)
- [How it works](#how-it-works)
- [Install](#install)
- [WebView bridge reference](#webview-bridge-reference)
- [Common mistakes](#common-mistakes)
- [Docs and links](#docs-and-links)
- [Community](#community)
- [License](#license)

---

## What SoifGo is

SoifGo turns a phone into a programmable controller. Each button can host its own WebView running one HTML page that you — or an AI assistant — wrote in HTML, CSS and JavaScript. That page talks to the native Android layer through a small JavaScript bridge, `window.soifgo.CallSub()`, so it can send and receive Bluetooth data, publish and subscribe over MQTT, call REST APIs, and read the phone's sensors.

It is **a runner and a bridge, not an app builder**. Your idea runs inside SoifGo; it does not become a separate APK and cannot be published to Google Play that way.

Built with **B4A (Basic4Android)**.

## Features

| Area | What you get |
|---|---|
| **WebView bridge** | One HTML page per button, with two-way communication between JavaScript and native Android (`window.soifgo`). |
| **Bluetooth Classic (SPP)** | Tested with HC-05, HC-06, JDY-31. |
| **Bluetooth Low Energy** | Tested with JDY-23, DX-BT05, CC2541. Same page code, different Play-menu option. |
| **MQTT** | Connect with broker URL and client ID, optional username/password, publish, retained flag, live status. |
| **HTTP / REST** | GET with JSON key extraction, POST with a JSON body. |
| **Phone sensors** | Ambient light, accelerometer X/Y/Z, magnetometer X/Y/Z, barometer (hPa). |
| **Microphone** | Speech recognition through the standard Web Speech API; the app asks for permission once. |
| **HTML runtime** | Full modern browser features inside the WebView: Canvas, WebGL, Web Audio, localStorage, `alert/confirm/prompt`, and libraries such as Chart.js, Three.js or TensorFlow.js. |
| **Classic mode** | Native buttons and sliders with behaviors, plus visual effects (rotation, size, color, charts) without writing HTML. |
| **Offline** | Bluetooth and sensors work without internet. MQTT and HTTP need a network. |

Requires **Android 7.0 or later**. Free, no ads, no in-app purchases.

## How it works

```
your idea  →  AI assistant writes one HTML file  →  paste into a SoifGo button  →  tap Play
```

1. **Describe your idea** to an AI assistant (ChatGPT, Claude, Gemini, or any other) and give it the SoifGo context so it uses real function names.
2. **Paste the generated HTML** into a button whose behavior is a WebView with a local HTML file.
3. **Tap Play.** The page runs on your phone and stays saved as a button.

Prompt to start with:

```text
Read https://soifgo.github.io/SOIFGO_AI_CONTEXT.md (if you cannot open it, read
https://soifgo.github.io/ai.html) and use it to build my idea as one HTML page for SoifGo.
My idea:
```

First-time setup inside the app is a fixed chain: **folder → page → button**. New users should follow the [quick start guide](https://soifgo.github.io/intro/en/).

## Install

SoifGo is **not on Google Play**.

- [APKPure](https://apkpure.com/soifgo/soifgo.saeidmoghadam)
- [ZIP package](https://soifgo.github.io/soifgo.zip)

Android will ask you to allow installs from unknown sources for the browser or file manager you open the file with. The APK is scanned by 65+ engines on [VirusTotal](https://www.virustotal.com/gui/file/5a28ee2acf881b4a300cc482e5691ee8313f1ca6fde0457e07e942e416d51d81) with no detections.

## WebView bridge reference

Call structure:

```js
window.soifgo.CallSub('SubName', true, arg1, arg2, arg3);
```

- The second argument is **always** `true` (runs on the UI thread).
- Pass **exactly** the number of arguments the Sub defines (0–3). Use `''` for an empty value; never drop an argument and never add extras.
- Always guard with `if (window.soifgo)`. Outside SoifGo the bridge is `undefined`.
- By default SoifGo appends CR+LF to Bluetooth text, so send `'ON'`, not `'ON\n'`. To change the ending call `bluetooth_eol` (below).
- ⚠️ `html_bluetooth_rx` **sends** and `html_bluetooth_tx` **receives**. The names look swapped; they are correct.

### Subs (page → SoifGo)

| Sub | Args after `true` | Purpose |
|---|---|---|
| `html_bluetooth_rx` | 1 · `txt` | Send text over Bluetooth |
| `bluetooth_eol` | 1 · `mode` | Line ending for `html_bluetooth_rx`: `'CRLF'` (default), `'CR'`, `'LF'`, `'STXETX'`, `'NULL'`, `'NON'`. Call once in `window.onload`, before sending; reset to CRLF when the WebView closes |
| `sensor_light` | 0 | Start the light sensor |
| `sensor_move` | 0 | Start the accelerometer |
| `sensor_magno` | 0 | Start the magnetometer |
| `sensor_barometer` | 0 | Start the barometer |
| `camera_permission` | 0 | Ask for camera permission (answer arrives in `html_camera_status`) |
| `connecte_mqtt_ai` | 2 · `address`, `id` | MQTT connect, step 1 |
| `connecte_mqtt_up` | 2 · `username`, `password` | MQTT auth, step 2 (optional) |
| `mqtt_publish` | 3 · `topic`, `message`, `qos` | Publish, and auto-subscribe to the topic |
| `mqtt_retained` | 1 · `1` / `0` | Retained flag; call before publishing |
| `mqtt_status` | 0 | Ask for the connection status |
| `mqtt_Disconnect` | 0 | Disconnect (capital `D`) |
| `api_send` | 3 · `address`, `key`, `value` | POST `{"key":"value"}` |
| `api_recive` | 2 · `address`, `key` | GET and extract `key` |

### Receivers (SoifGo → page)

Register every receiver on `window`:

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
   SoifGo appends CR+LF itself by default, so send only the word: 'ON'
   Other ending: CallSub('bluetooth_eol', true, 'CR'|'LF'|'STXETX'|'NULL'|'NON') in window.onload
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
   bluetooth_eol                 1  mode                BT line ending (default CRLF)
   sensor_light                  0                      start light sensor
   sensor_move                   0                      start motion sensor
   sensor_magno                  0                      start magnetic sensor
   sensor_barometer              0                      start barometer
   camera_permission             0                      ask camera permission
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
   html_camera_status            'true' | 'false'
   mqtt_rx                       topic, message
   html_mqtt_status              'true' | 'false'
   api_rx                        number
   ========================================================================= */
```

| Receiver | Args |
|---|---|
| `html_bluetooth_tx` | `data` |
| `html_sensor_light` | `data` |
| `html_sensor_movex` / `movey` / `movez` | `data` |
| `html_sensor_magnox` / `magnoy` / `magnoz` | `data` |
| `html_sensor_barometer` | `data` (hPa, 3 decimals) |
| `html_camera_status` | `'true'` / `'false'` — a **string**; the answer to `camera_permission` |
| `mqtt_rx` | `topic`, `message` |
| `html_mqtt_status` | `'true'` / `'false'` — a **string**, not a boolean |
| `api_rx` | `number` |

### Working snippet

<details>
<summary><b>Show a minimal page that covers Bluetooth, sensors, MQTT, API and voice</b></summary>

```html
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SoifGo bridge demo</title>
</head>
<body>
<div id="out">waiting...</div>

<script>
function log(t) { document.getElementById('out').textContent = t; }
function hasBridge() { return !!window.soifgo; }

/* ---------- Bluetooth ---------- */
function btSend(text) {
  if (!hasBridge()) return log('Not running inside SoifGo');
  window.soifgo.CallSub('html_bluetooth_rx', true, String(text));
  log('BT >> ' + text);
}
function html_bluetooth_tx(data) { log('BT << ' + data); }
window.html_bluetooth_tx = html_bluetooth_tx;

/* ---------- Sensors ---------- */
function html_sensor_light(d)     { log('light ' + d); }
function html_sensor_movex(d)     { log('moveX ' + d); }
function html_sensor_movey(d)     { log('moveY ' + d); }
function html_sensor_movez(d)     { log('moveZ ' + d); }
function html_sensor_magnox(d)    { log('magnoX ' + d); }
function html_sensor_magnoy(d)    { log('magnoY ' + d); }
function html_sensor_magnoz(d)    { log('magnoZ ' + d); }
function html_sensor_barometer(d) { log('baro ' + d); }

window.html_sensor_light     = html_sensor_light;
window.html_sensor_movex     = html_sensor_movex;
window.html_sensor_movey     = html_sensor_movey;
window.html_sensor_movez     = html_sensor_movez;
window.html_sensor_magnox    = html_sensor_magnox;
window.html_sensor_magnoy    = html_sensor_magnoy;
window.html_sensor_magnoz    = html_sensor_magnoz;
window.html_sensor_barometer = html_sensor_barometer;

/* ---------- MQTT ---------- */
function mqttConnect(broker, id) {
  if (!hasBridge()) return log('Not running inside SoifGo');
  window.soifgo.CallSub('connecte_mqtt_ai', true,
    broker || 'tcp://test.mosquitto.org:1883', id || '');
}
function mqttPublish(topic, msg, qos) {
  if (!hasBridge()) return log('Not running inside SoifGo');
  window.soifgo.CallSub('mqtt_publish', true, String(topic), String(msg), String(qos || '0'));
}
function mqtt_rx(topic, message) { log('MQTT ' + topic + ' : ' + message); }
window.mqtt_rx = mqtt_rx;

function html_mqtt_status(connected) {
  log('MQTT ' + ((connected === true || connected === 'true') ? 'connected' : 'disconnected'));
}
window.html_mqtt_status = html_mqtt_status;

/* ---------- API ---------- */
function apiGet(url, key) {
  if (!hasBridge()) return log('Not running inside SoifGo');
  window.soifgo.CallSub('api_recive', true, String(url), String(key));
}
function extractValue(raw, field) {
  var s = String(raw).trim();
  if (/^-?\d+(\.\d+)?$/.test(s)) return s;                                 // 86846.51
  var m = s.match(new RegExp(field + '[=:]\\s*([^,}\\s]+)', 'i'));          // {usd=86750}
  if (m) return m[1];
  try { var o = JSON.parse(s); if (o[field] !== undefined) return o[field]; } catch (e) {}
  var n = s.match(/-?\d+(\.\d+)?/);
  return n ? n[0] : null;
}
function api_rx(number) { log('API ' + extractValue(number, 'usd')); }
window.api_rx = api_rx;

/* ---------- Voice ---------- */
function voiceStart() {
  var SR = window.SpeechRecognition || window.webkitSpeechRecognition;
  if (!SR) return log('Speech recognition unavailable');
  var rec = new SR();
  rec.lang = 'en-US';
  rec.onresult = function (e) { btSend(e.results[0][0].transcript); };
  rec.start();
}

/* ---------- Start the sensors you need ---------- */
window.onload = function () {
  if (!hasBridge()) { log('Not running inside SoifGo'); return; }
  window.soifgo.CallSub('sensor_light', true);
  window.soifgo.CallSub('sensor_move', true);
  window.soifgo.CallSub('sensor_magno', true);
  window.soifgo.CallSub('sensor_barometer', true);
  log('ready');
};
</script>
</body>
</html>
```

</details>

Paste it into a WebView button: **Play → WebView button → WebView menu → Edit → Select All and Paste → close with the yellow X**. The code is saved and the page reloads automatically.

More runnable examples: [Bluetooth send](https://soifgo.github.io/send_bluetooth_html/html_guide_1.html) · [ESP8266 MQTT](https://soifgo.github.io/Esp8266_wifi_mqtt/esp8266_mqtt.html) · [REST APIs](https://soifgo.github.io/FetchingAPIs/Api-fetching_api.html) · [Voice to Bluetooth](https://soifgo.github.io/serial_voice/serial_voice_tutorials.html).

## Common mistakes

| Symptom | Cause |
|---|---|
| Warning sound and a list copied to the clipboard | The page called a Sub name that does not exist. Paste that copied list into your AI assistant and ask it to fix the code. |
| `window.soifgo is undefined` | The page is open in a normal browser, not inside SoifGo. |
| Nothing is sent over Bluetooth | The module is not connected or not paired. SoifGo shows a "disconnected" message instead of sending. |
| MQTT status is always "connected" | `'false'` arrives as a **string** and is truthy. Compare with `=== 'true'`. |
| Sensors stay empty | The matching `sensor_*` Sub was never called. Each sensor needs its own activation call. |
| Page works but no data arrives | Receivers were not attached to `window`. |
| API shows a parse error | Some providers return a wrapped map (`{usd=86750}`) instead of a plain number. Use the `extractValue` helper above. |

## Docs and links

| Resource | Link |
|---|---|
| Official site | https://soifgo.github.io/ |
| Quick start | https://soifgo.github.io/intro/en/ |
| Complete user guide | https://soifgo.github.io/soifgo_info/soifgo_info.html |
| Developer docs and examples | https://soifgo.github.io/docs.html |
| AI context (source of truth) | https://soifgo.github.io/SOIFGO_AI_CONTEXT.md |
| Short AI manual | https://soifgo.github.io/ai.html |
| Machine-readable bridge spec (JSON) | https://soifgo.github.io/soifgo-bridge.json |
| Video tutorials | https://www.youtube.com/@soifgo |
| Privacy policy | https://soifgo.github.io/policy_soifgo.html |

## Community

- [GitHub Discussions](https://github.com/soifgo/soifgo.github.io/discussions) — questions, bug reports, project sharing
- [Reddit r/SoifGoApp](https://www.reddit.com/r/SoifGoApp/)
- Email: ssm20985@gmail.com

## License

MIT. See [LICENSE.md](https://github.com/soifgo/soifgo.github.io/blob/soifgo/LICENSE.md).
