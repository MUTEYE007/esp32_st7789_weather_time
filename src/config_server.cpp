#include "config_server.h"
#include "weather.h"
#include <WebServer.h>
#include <WiFi.h>

static WebServer server(80);
static bool serverStarted = false;

static const char PAGE_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,'Segoe UI',sans-serif;background:#0a1628;color:#e0e0e0;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;padding:20px}
.card{background:#132238;border-radius:16px;padding:32px;width:100%%;max-width:600px;box-shadow:0 8px 32px rgba(0,0,0,.4)}
h1{font-size:20px;color:#7ec8e3;margin-bottom:24px;text-align:center}
.row{display:flex;gap:16px;flex-wrap:wrap}
.row>*{flex:1;min-width:220px}
label{display:block;font-size:13px;color:#8899aa;margin-bottom:6px}
input{width:100%%;padding:12px 16px;border:1px solid #2a3a50;border-radius:10px;background:#0d1b2e;color:#fff;font-size:14px;outline:none;transition:border-color .2s}
input:focus{border-color:#5d9fcf}
.field{margin-bottom:18px}
.btn{width:100%%;padding:12px;border:none;border-radius:10px;background:#2c6ea0;color:#fff;font-size:15px;font-weight:600;cursor:pointer;transition:background .2s}
.btn:hover{background:#3a8ac4}
.btn:active{transform:scale(.98)}
.btn.green{background:#1a5a3a}
.btn.green:hover{background:#247a4e}
.status{margin-top:16px;padding:10px;border-radius:8px;text-align:center;font-size:13px;display:none}
.status.success{display:block;background:#1a3a2a;color:#6fcf97}
.status.error{display:block;background:#3a1a1a;color:#cf6f6f}
.ip{text-align:center;margin-top:16px;font-size:12px;color:#556677}
.hr{height:1px;background:#2a3a50;margin:20px 0}
.ipstat{display:flex;gap:12px;flex-wrap:wrap;margin-bottom:18px;padding:12px;background:rgba(0,0,0,.15);border-radius:10px}
.ipstat>div{flex:1;min-width:100px;text-align:center}
.ipstat .l{font-size:11px;color:#556677}
.ipstat .v{font-size:16px;font-weight:600;color:#7ec8e3}
</style></head><body><div class="card"><h1>ESP32 Weather Config</h1>

<div class="ipstat">
<div><div class="l">IP</div><div class="v">%s</div></div>
<div><div class="l">WiFi</div><div class="v">%s</div></div>
<div><div class="l">Weather</div><div class="v" style="color:%s">%s</div></div>
</div>

<div class="row">
<div>
<form id="cfg" onsubmit="save();return false">
<div class="field"><label>API Key</label><input type="text" id="apiKey" value="%s" /></div>
<div class="field"><label>Host</label><input type="text" id="host" value="%s" /></div>
<button class="btn" type="submit">Save</button></form>
<div id="status" class="status"></div>
</div>
<div>
<form id="cityForm" onsubmit="setCity();return false">
<div class="field"><label>City Name</label><input type="text" id="cityName" value="%s" placeholder="e.g. Fuzhou" /></div>
<button class="btn green" type="submit">Set City</button></form>
<div id="cityStatus" class="status"></div>
</div>
</div>

<div class="hr"></div>
<div style="text-align:center">
<button class="btn" style="width:auto;padding:10px 28px;display:inline-block" id="refreshBtn" onclick="doRefresh()">Refresh Weather</button>
<div id="refreshStatus" class="status"></div>
</div>

<div class="ip">%s</div>
<script>
function stat(el,ok,msg){el.className='status '+(ok?'success':'error');el.textContent=msg;el.style.display='block'}
function busy(btn,text){btn.disabled=1;btn.textContent=text}
function ready(btn,text){btn.disabled=0;btn.textContent=text}

async function save(){
  var btn=document.querySelector('#cfg button'),st=document.getElementById('status'),key=document.getElementById('apiKey').value,host=document.getElementById('host').value;
  if(!key||!host){stat(st,0,'Fields cannot be empty');return}
  busy(btn,'Saving...');stat(st,1,'Saving...');
  try{
    var r=await fetch('/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'apiKey='+encodeURIComponent(key)+'&host='+encodeURIComponent(host)});
    var t=await r.text();
    stat(st,r.ok,'Saved! Refetching weather...')
  }catch(e){stat(st,0,'Network error: '+e.message)}
  setTimeout(function(){st.style.display='none'},3000);
  ready(btn,'Save')
}

async function setCity(){
  var btn=document.querySelector('#cityForm button'),st=document.getElementById('cityStatus'),city=document.getElementById('cityName').value;
  if(!city){stat(st,0,'Enter a city name');return}
  busy(btn,'Setting...');stat(st,1,'Resolving city...');
  try{
    var r=await fetch('/setcity',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'city='+encodeURIComponent(city)});
    var t=await r.text();
    if(r.ok){stat(st,1,'Resolved: '+city)}else{stat(st,0,'Failed: '+t)}
  }catch(e){stat(st,0,'Network error: '+e.message)}
  setTimeout(function(){st.style.display='none'},4000);
  ready(btn,'Set City')
}

async function doRefresh(){
  var btn=document.getElementById('refreshBtn'),st=document.getElementById('refreshStatus');
  busy(btn,'Refreshing...');stat(st,1,'Requesting weather data...');
  try{
    var r=await fetch('/refresh',{method:'POST'});var t=await r.text();
    if(r.ok){stat(st,1,'Refreshing, please wait...');setTimeout(function(){location.reload()},2000)}
    else{stat(st,0,'Error: '+t)}
  }catch(e){stat(st,0,'Network error')}
  ready(btn,'Refresh Weather')
}
</script></body></html>
)rawliteral";

static void handleRoot() {
  char ipStr[16];
  WiFi.localIP().toString().toCharArray(ipStr, sizeof(ipStr));

  char rssiStr[12];
  int rssi = WiFi.RSSI();
  snprintf(rssiStr, sizeof(rssiStr), "%d dBm", rssi);

  const char *wthColor;
  const char *wthText;
  if (weather.valid) {
    wthColor = "#6fcf97";
    wthText = "OK";
  } else {
    wthColor = "#e5a526";
    wthText = "Waiting...";
  }

  char buf[4096];
  snprintf_P(buf, sizeof(buf), PAGE_TEMPLATE,
    ipStr,
    rssiStr,
    wthColor,
    wthText,
    weatherApiKey.c_str(),
    weatherHost.c_str(),
    weatherName.c_str(),
    ipStr
  );
  server.send(200, "text/html; charset=utf-8", buf);
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

static void handleRefresh() {
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    state.lastWeatherFetch = 0;
    state.lastMinutelyFetch = 0;
    xSemaphoreGive(dataMutex);
  }
  server.send(200, "text/plain", "OK");
}

void startConfigServer() {
  if (serverStarted) return;
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/setcity", HTTP_POST, handleSetCity);
  server.on("/refresh", HTTP_POST, handleRefresh);
  server.begin();
  serverStarted = true;
}

void handleConfigClient() {
  if (serverStarted) {
    server.handleClient();
  }
}
