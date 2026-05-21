#include "config_server.h"
#include "weather.h"
#include <WebServer.h>
#include <WiFi.h>

static WebServer server(80);
static bool serverStarted = false;

static const char PAGE_HEAD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,'Segoe UI',sans-serif;background:#0a1628;color:#e0e0e0;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;padding:20px}
.card{background:#132238;border-radius:16px;padding:32px;width:100%;max-width:400px;box-shadow:0 8px 32px rgba(0,0,0,.4)}
h1{font-size:20px;color:#7ec8e3;margin-bottom:24px;text-align:center}
label{display:block;font-size:13px;color:#8899aa;margin-bottom:6px}
input{width:100%;padding:12px 16px;border:1px solid #2a3a50;border-radius:10px;background:#0d1b2e;color:#fff;font-size:14px;outline:none;transition:border-color .2s}
input:focus{border-color:#5d9fcf}
.field{margin-bottom:18px}
.btn{width:100%;padding:12px;border:none;border-radius:10px;background:#2c6ea0;color:#fff;font-size:15px;font-weight:600;cursor:pointer;transition:background .2s}
.btn:hover{background:#3a8ac4}
.btn:active{transform:scale(.98)}
.status{margin-top:16px;padding:10px;border-radius:8px;text-align:center;font-size:13px;display:none}
.status.success{display:block;background:#1a3a2a;color:#6fcf97}
.status.error{display:block;background:#3a1a1a;color:#cf6f6f}
.ip{text-align:center;margin-top:16px;font-size:12px;color:#556677}
</style>
</head>
<body>
<div class="card">
<h1>Weather Config</h1>
<form id="cfg" onsubmit="return save()">
<div class="field"><label>API Key</label>
<input type="text" id="apiKey" value=")rawliteral";

static const char PAGE_MID[] PROGMEM = R"rawliteral(" /></div>
<div class="field"><label>Host</label>
<input type="text" id="host" value=")rawliteral";

static const char PAGE_TAIL[] PROGMEM = R"rawliteral(" /></div>
<button class="btn" type="submit">Save</button>
</form>
<div id="status" class="status"></div>
<div style="height:1px;background:#2a3a50;margin:20px 0"></div>
<form onsubmit="return setCity()">
<div class="field"><label>City Name</label>
<input type="text" id="cityName" value=")rawliteral";

static const char PAGE_END[] PROGMEM = R"rawliteral(" placeholder="e.g. 福州" /></div>
<button class="btn" type="submit" style="background:#1a5a3a">Set City</button>
</form>
<div id="cityStatus" class="status"></div>
<div class="ip">)rawliteral";

static void handleRoot() {
  String page = FPSTR(PAGE_HEAD);
  page += weatherApiKey;
  page += FPSTR(PAGE_MID);
  page += weatherHost;
  page += FPSTR(PAGE_TAIL);
  page += weatherName;
  page += FPSTR(PAGE_END);
  page += WiFi.localIP().toString();
  page += F("</div>\n<script>\n"
    "async function save(){const r=await fetch('/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'apiKey='+encodeURIComponent(document.getElementById('apiKey').value)+'&host='+encodeURIComponent(document.getElementById('host').value)});const t=await r.text();const s=document.getElementById('status');s.className='status '+(r.ok?'success':'error');s.textContent=r.ok?'Saved! Refetching weather...':'Error: '+t;s.style.display='block';return false}\n"
    "async function setCity(){const r=await fetch('/setcity',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'city='+encodeURIComponent(document.getElementById('cityName').value)});const t=await r.text();const s=document.getElementById('cityStatus');s.className='status '+(r.ok?'success':'error');s.textContent=r.ok?'City set to: '+document.getElementById('cityName').value:'Error: '+t;s.style.display='block';return false}\n"
    "</script>\n</body>\n</html>");
  server.send(200, "text/html; charset=utf-8", page);
}

static void handleSave() {
  if (!server.hasArg("apiKey") || !server.hasArg("host")) {
    server.send(400, "text/plain", "Missing apiKey or host");
    return;
  }
  String apiKey = server.arg("apiKey");
  String host = server.arg("host");
  apiKey.trim();
  host.trim();
  if (apiKey.length() == 0 || host.length() == 0) {
    server.send(400, "text/plain", "Fields cannot be empty");
    return;
  }
  saveConfig(apiKey, host);
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    state.lastWeatherFetch = 0;
    xSemaphoreGive(dataMutex);
  }
  server.send(200, "text/plain", "OK");
}

static void handleSetCity() {
  if (!server.hasArg("city")) {
    server.send(400, "text/plain", "Missing city name");
    return;
  }
  String city = server.arg("city");
  city.trim();
  if (city.length() == 0) {
    server.send(400, "text/plain", "City name cannot be empty");
    return;
  }
  if (setCityByName(city)) {
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Failed to resolve city");
  }
}

void startConfigServer() {
  if (serverStarted) return;
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/setcity", HTTP_POST, handleSetCity);
  server.begin();
  serverStarted = true;
}

void handleConfigClient() {
  if (serverStarted) {
    server.handleClient();
  }
}
