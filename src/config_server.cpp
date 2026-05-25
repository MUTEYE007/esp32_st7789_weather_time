#include "config_server.h"
#include "weather.h"
#include "display.h"
#include "page_manager.h"

volatile int otaProgress = -1; // OTA progress for screen display
#include <WebServer.h>
#include <WiFi.h>
#include <Update.h>
#include <time.h>
#include <ArduinoJson.h>

static WebServer server(80);
static bool serverStarted = false;

// ---- Template ----
static const char PAGE_TEMPLATE[] = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<style>
*{margin:0;padding:0;box-sizing:border-box}
@keyframes scan{0%{transform:translateY(-100%)}100%{transform:translateY(100%)}}
@keyframes glow{0%,100%{box-shadow:0 0 6px #e8c58022}50%{box-shadow:0 0 20px #e8c58044}}
:root{--bg:#1a1a1a;--card:#242424;--border:#3a3a3a;--amber:#e8c580;--gold:#f5d998;--teal:#00bfa5;--mute:#8a7a6a;--bg2:#202020;--red:#e85a4f;--orange:#e8a04f;--fs:12px;--fs-s:10px;--fs-xs:8px;--fs-lg:14px;--p:16px;--gap:6px;--r:6px}
@media(min-width:768px){:root{--fs:14px;--fs-s:12px;--fs-xs:10px;--fs-lg:18px;--p:28px;--gap:10px;--r:8px}}
body{font-family:Georgia,'Times New Roman',serif;background:var(--bg);color:var(--amber);min-height:100vh;display:flex;justify-content:center;align-items:flex-start;margin:0;padding:var(--p);position:relative}
body::before{content:'';position:fixed;top:0;left:0;width:100%;height:100%;background:radial-gradient(ellipse at 50% 0%,#2a2a1a 0%,#1a1a1a 70%);z-index:-1}
.panel{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:var(--p);width:100%;max-width:960px;position:relative;box-shadow:0 4px 24px rgba(0,0,0,.5)}
h1{font-family:Georgia,serif;font-size:var(--fs-lg);color:var(--gold);text-align:center;letter-spacing:3px;font-weight:400;margin-bottom:calc(var(--p) - 4px);animation:glow 3s infinite}
.grid{display:grid;grid-template-columns:1fr;gap:var(--gap)}
@media(min-width:768px){.grid{grid-template-columns:1fr 1fr}}
@media(min-width:1024px){.grid{grid-template-columns:1fr 1fr 1fr}}
.sec{margin-bottom:var(--gap)}
.sec .t{font-size:var(--fs-xs);text-transform:uppercase;letter-spacing:1.5px;color:var(--mute);font-family:'Courier New',monospace;margin-bottom:4px;padding:3px 0 2px;border-bottom:1px solid var(--border)}
.sc-span{grid-column:1 / -1}
.dash{display:grid;grid-template-columns:1fr 1fr;gap:var(--gap)}
.dash>div{background:var(--bg2);border:1px solid var(--border);border-radius:var(--r);padding:6px 8px}
@media(min-width:768px){.dash>div{padding:8px 12px}}
.dash .l{font-size:var(--fs-xs);text-transform:uppercase;letter-spacing:1px;color:var(--mute);font-family:'Courier New',monospace}
.dash .v{font-size:var(--fs-s);color:var(--gold);font-family:'Courier New',monospace;word-break:break-all}
.dash .v.g{color:var(--teal)}.dash .v.r{color:var(--red)}
.s2{grid-column:span 2}
.gr{display:flex;flex-direction:column;gap:var(--gap)}
.gr>*{flex:none}
.lb{display:block;font-size:var(--fs-xs);text-transform:uppercase;letter-spacing:1px;color:var(--mute);font-family:'Courier New',monospace;margin-bottom:4px}
.in{width:100%;padding:7px 10px;border:1px solid var(--border);border-radius:var(--r);background:var(--bg2);color:var(--amber);font-size:var(--fs-s);font-family:'Courier New',monospace;outline:none;transition:border-color .2s}
.in:focus{border-color:var(--teal)}
@media(min-width:768px){.in{padding:9px 12px;font-size:var(--fs)}}
.fld{margin-bottom:8px}
.b{width:100%;padding:7px;border:1px solid var(--teal);border-radius:var(--r);background:linear-gradient(135deg,#00bfa522,#00bfa511);color:var(--teal);font-size:var(--fs-xs);font-family:'Courier New',monospace;text-transform:uppercase;letter-spacing:1.5px;cursor:pointer;transition:all .2s;user-select:none}
.b:hover{background:linear-gradient(135deg,#00bfa544,#00bfa522);box-shadow:0 0 12px #00bfa533}
.b:active{transform:scale(.97)}
.b.r{border-color:var(--red)44;color:var(--red);background:linear-gradient(135deg,var(--red)22,var(--red)11)}
.b.r:hover{border-color:var(--red)88;box-shadow:0 0 12px var(--red)44}
@media(min-width:768px){.b{padding:9px;font-size:var(--fs-s);letter-spacing:2px}}
.sel{width:100%;padding:6px 8px;border:1px solid var(--border);border-radius:var(--r);background:var(--bg2);color:var(--amber);font-size:var(--fs-s);font-family:'Courier New',monospace;outline:none;cursor:pointer}
.stel{display:flex;align-items:center;gap:var(--gap)}
.st{margin-top:6px;padding:5px;border-radius:4px;text-align:center;font-size:var(--fs-xs);display:none;font-family:'Courier New',monospace}
.st.ok{display:block;background:#00bfa511;color:var(--teal);border:1px solid #00bfa533}
.st.er{display:block;background:var(--red)11;color:var(--red);border:1px solid var(--red)33}
.bpres{display:grid;grid-template-columns:1fr 1fr 1fr 1fr 1fr;gap:4px}
.bpres .b{padding:6px 0}
.pg{display:grid;grid-template-columns:1fr 1fr 1fr 1fr 1fr;gap:4px}
.pg .b{padding:6px 0;font-size:var(--fs-xs);letter-spacing:1px}
.pg .b.ac{background:var(--teal);color:var(--card);border-color:var(--teal)}
.navrow{display:flex;flex-wrap:wrap;gap:var(--gap);align-items:center}
.navrow .lb2{font-size:var(--fs-xs);color:var(--mute);font-family:'Courier New',monospace;text-transform:uppercase;letter-spacing:1px;white-space:nowrap}
.ctrls{display:grid;grid-template-columns:1fr 1fr 1fr 1fr;gap:4px;flex:1}
.ctrls .b{font-size:var(--fs-xs);padding:6px 0;letter-spacing:1px}
.wg{display:flex;gap:4px;flex-wrap:wrap;margin:4px 0}
.wg .wg-i{font-size:var(--fs-xs);padding:2px 6px;border-radius:3px;font-family:'Courier New',monospace;white-space:nowrap}
.wg .wg-i.rd{background:var(--red)22;color:var(--red);border:1px solid var(--red)33}
.wg .wg-i.or{background:var(--orange)22;color:var(--orange);border:1px solid var(--orange)33}
.wg .wg-i.yl{background:#e8c58022;color:#e8c580;border:1px solid #e8c58033}
.wg .wg-i.bl{background:#5a9fcf22;color:#5a9fcf;border:1px solid #5a9fcf33}
.fc{display:flex;gap:6px;overflow-x:auto;padding:4px 0}
.fc .fc-i{text-align:center;min-width:38px;padding:4px 2px;background:var(--bg2);border-radius:var(--r)}
.fc .fc-i .t2{font-size:var(--fs-xs);color:var(--mute);font-family:'Courier New',monospace}
.fc .fc-i .t1{font-size:var(--fs-s);color:var(--gold);font-family:'Courier New',monospace;font-weight:700}
.gu{margin-top:var(--gap);border:1px solid var(--border);border-radius:var(--r);overflow:hidden}
.gh{padding:7px 12px;cursor:pointer;display:flex;justify-content:space-between;align-items:center;font-size:var(--fs-s);text-transform:uppercase;letter-spacing:1.5px;color:var(--teal);user-select:none;background:var(--bg2);transition:background .2s;font-family:'Courier New',monospace}
.gh:hover{background:#2a2a2a}.gh .a{color:var(--mute);font-size:7px;transition:transform .25s}
.go .gh .a{transform:rotate(180deg)}.gb{padding:0 12px 8px;display:none}.go .gb{display:block}
.gb table{width:100%;border-collapse:collapse;margin-top:5px;font-size:var(--fs-s)}
.gb td{padding:3px 3px;border-bottom:1px solid var(--bg2);vertical-align:top;color:#9a8a7a;font-family:'Courier New',monospace}
.gb td:first-child{color:var(--teal);white-space:nowrap;width:65px}
.gb .sh{color:var(--mute);font-size:var(--fs-xs);text-transform:uppercase;letter-spacing:1.5px;padding:6px 0 2px;font-family:'Courier New',monospace}
.foot{text-align:center;margin-top:8px;font-size:var(--fs-xs);color:var(--mute);letter-spacing:1px;font-family:'Courier New',monospace}
.dbgs{display:flex;gap:6px;flex-wrap:wrap;margin-top:6px}
.dbgs>span{font-size:var(--fs-xs);color:var(--mute);font-family:'Courier New',monospace;background:var(--bg2);padding:2px 6px;border-radius:3px}
</style></head><body><div class="panel"><h1>ESP32 天气站</h1><div class="grid">

<div class="sc-span sec"><div class="t">设备状态</div>
<div class="dash">
<div class="s2"><div class="l">WiFi 网络</div><div class="v"><span id="wSsid">---</span> &nbsp; <span id="wMac">---</span></div></div>
<div><div class="l">IP 地址</div><div class="v" id="wIp">%s</div></div>
<div><div class="l">信号</div><div class="v" id="wRssi">%s</div></div>
<div><div class="l">网关</div><div class="v" id="wGw">---</div></div>
<div><div class="l">运行时长</div><div class="v" id="wUptime">---</div></div>
<div><div class="l">天气</div><div class="v %s" id="wWth">%s</div></div>
</div></div>

<div class="sc-span sec"><div class="t">配置</div>
<div class="gr">
<div>
<div class="fld"><label class="lb">API 密钥</label><input class="in" type="text" id="apiKey" value="%s" /></div>
<div class="fld"><label class="lb">接口地址</label><input class="in" type="text" id="host" value="%s" /></div>
<button class="b" onclick="save()">保存</button><div id="st" class="st"></div>
</div>
<div>
<div class="fld"><label class="lb">城市名称</label><input class="in" type="text" id="cityName" value="%s" placeholder="如 福州" /></div>
<button class="b r" onclick="setCity()" style="margin-bottom:4px">设置城市</button>
<button class="b" onclick="doRefresh()">刷新天气</button>
<div id="rfSt" class="st"></div><div id="ctSt" class="st"></div>
</div>
</div></div>

<div class="sc-span sec"><div class="t">天气概况</div>
<div id="wthContainer" style="display:none">
<div class="dash" style="grid-template-columns:1fr 1fr 1fr">
<div><div class="l">温度</div><div class="v" id="wTemp">--</div></div>
<div><div class="l">体感</div><div class="v" id="wFeels">--</div></div>
<div><div class="l">湿度</div><div class="v" id="wHumi">--</div></div>
<div><div class="l">风向</div><div class="v" id="wWind">--</div></div>
<div><div class="l">最高</div><div class="v" id="wTmax">--</div></div>
<div><div class="l">最低</div><div class="v" id="wTmin">--</div></div>
</div>
<div class="fc" id="fcContainer"></div>
<div id="warnContainer"></div>
<div id="precipContainer"></div>
</div>
<div id="wthWaiting" style="color:var(--mute);font-size:var(--fs-s);font-family:'Courier New',monospace;text-align:center;padding:12px">等待天气数据...</div>
</div>

<div class="sec"><div class="t">亮度</div>
<div class="stel">
<div class="b" style="flex:0 0 36px;padding:7px 0" onclick="brightAdj(-16)">-</div>
<div class="pv" id="brightVal" style="flex:1;text-align:center;background:var(--bg2);border:1px solid var(--border);border-radius:var(--r);padding:7px 0;font-size:var(--fs-lg);font-weight:700;color:var(--gold);font-family:'Courier New',monospace">%d</div>
<div class="b" style="flex:0 0 36px;padding:7px 0" onclick="brightAdj(16)">+</div>
</div>
<div class="bpres">
<button class="b" onclick="brightSet(16)">16</button>
<button class="b" onclick="brightSet(48)">48</button>
<button class="b" onclick="brightSet(96)">96</button>
<button class="b" onclick="brightSet(160)">160</button>
<button class="b" onclick="brightSet(255)">255</button>
</div></div>

<div class="sec"><div class="t">远程页面控制</div>
<div class="pg" id="pgBtns">
<button class="b %s" onclick="setPg(0)">主页</button>
<button class="b %s" onclick="setPg(1)">预警</button>
<button class="b %s" onclick="setPg(2)">降水</button>
<button class="b %s" onclick="setPg(3)">系统</button>
<button class="b %s" onclick="setPg(4)">帮助</button>
</div></div>

<div class="sec"><div class="t">设备控制</div>
<div class="navrow" style="margin-bottom:6px">
<div class="lb2">旋转</div>
<div class="ctrls">
<button class="b" onclick="ctrl('rotate','0')">0&#xb0;</button>
<button class="b" onclick="ctrl('rotate','1')">90&#xb0;</button>
<button class="b" onclick="ctrl('rotate','2')">180&#xb0;</button>
<button class="b" onclick="ctrl('rotate','3')">270&#xb0;</button>
</div></div>
<div class="navrow" style="margin-bottom:6px">
<div class="lb2">间隔</div>
<select class="sel" id="wIntervalSel" onchange="ctrl('winterval',this.value)" style="max-width:200px">
<option value="300000">5 分钟</option>
<option value="600000">10 分钟</option>
<option value="900000">15 分钟</option>
<option value="1800000" selected>30 分钟</option>
<option value="3600000">60 分钟</option>
</select></div>
<div class="navrow">
<button class="b" onclick="ctrl('wake','1')" style="flex:1;min-width:80px">唤醒</button>
<button class="b r" onclick="ctrl('sleep','1')" style="flex:1;min-width:80px">熄屏</button>
<button class="b r" onclick="if(confirm('确认重启?'))ctrl('reboot','1')" style="flex:1;min-width:80px">重启</button>
<button class="b" onclick="location.href='/update'" style="flex:1;min-width:80px">⚡升级</button>
</div></div>

<div class="sc-span sec"><div class="t">调试信息</div>
<div class="dbgs" id="dbgContainer"></div></div>

<div class="sc-span gu" id="gu">
<div class="gh" onclick="var p=this.parentNode;p.className=p.className.includes('gu go')?'gu':'gu go'"><span>设备指南</span><span class="a">&#9660;</span></div>
<div class="gb">
<div class="sh">按钮操作</div>
<table><tr><td>BOOT</td><td>长按3秒重置所有配置</td></tr><tr><td>GPIO13</td><td>短按翻页 / 长按调光</td></tr></table>
<div class="sh">页面说明</div>
<table><tr><td>主页</td><td>天气 + 时钟 + 7小时预报</td></tr><tr><td>预警</td><td>气象预警自动弹窗</td></tr><tr><td>降水</td><td>未来2小时降水图</td></tr><tr><td>系统</td><td>设备状态 / WiFi / NTP</td></tr><tr><td>帮助</td><td>屏幕按键指南</td></tr></table>
<div class="sh">首次使用</div>
<table><tr><td>1.</td><td>长按 BOOT 3秒进配网模式</td></tr><tr><td>2.</td><td>手机连 ESP32 热点</td></tr><tr><td>3.</td><td>在此页填 API Key 和城市</td></tr><tr><td>4.</td><td>设备自动获取天气</td></tr></table>
</div></div>

</div><div class="foot">%s</div>
<script>
var curPg=0, pct="%", deg="\u00b0";
function $(id){return document.getElementById(id)}
function F(v){if(v==undefined)return"--";return v}
function sc(el,ok,m){el.className="st "+(ok?"ok":"er");el.textContent=m;el.style.display="block"}
async function save(){
  var k=$("apiKey").value,h=$("host").value,st=$("st"),btn=st.parentNode.querySelector(".b");
  if(!k||!h){sc(st,0,"不能为空");return}
  btn.disabled=1;btn.textContent="保存中";sc(st,1,"保存中");
  try{var r=await fetch("/save",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"apiKey="+encodeURIComponent(k)+"&host="+encodeURIComponent(h)});sc(st,r.ok,r.ok?"已保存":"出错了")}catch(e){sc(st,0,"网络错误")}
  setTimeout(function(){st.style.display="none"},3000);btn.disabled=0;btn.textContent="保存"}
async function setCity(){
  var c=$("cityName").value,st=$("ctSt"),btn=st.parentNode.querySelector(".b");
  if(!c){sc(st,0,"请输入城市");return}
  btn.disabled=1;btn.textContent="设置中";sc(st,1,"解析中");
  try{var r=await fetch("/setcity",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"city="+encodeURIComponent(c)});var t=await r.text();sc(st,r.ok,r.ok?"已解析: "+c:"失败: "+t)}catch(e){sc(st,0,"网络错误")}
  setTimeout(function(){st.style.display="none"},4000);btn.disabled=0;btn.textContent="设置城市"}
async function doRefresh(){
  var st=$("rfSt"),btn=st.parentNode.querySelector(".b");
  btn.disabled=1;btn.textContent="刷新中";sc(st,1,"请求中");
  try{var r=await fetch("/refresh",{method:"POST"});sc(st,r.ok,r.ok?"刷新完成":"出错了");if(r.ok)setTimeout(function(){location.reload()},2000)}catch(e){sc(st,0,"网络错误")}
  btn.disabled=0;btn.textContent="刷新天气"}
function brightAdj(d){
  var v=parseInt($("brightVal").textContent)+d;if(v<1)v=1;if(v>255)v=255;
  $("brightVal").textContent=v;fetch("/setbrightness",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"level="+v})}
function brightSet(v){
  $("brightVal").textContent=v;fetch("/setbrightness",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"level="+v})}
async function setPg(n){
  curPg=n;var b=document.querySelectorAll(".pg .b");for(var i=0;i<b.length;i++)b[i].className="b"+(i==n?" ac":"");
  await fetch("/setpage",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"page="+n})}
async function ctrl(a,v){
  try{await fetch("/control",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"action="+a+"&val="+v})}catch(e){}
  if(a=="reboot")setTimeout(function(){location.reload()},3000)}
async function poll(){
  try{var r=await fetch("/status");if(!r.ok)return;var d=await r.json();
    $("wSsid").textContent=F(d.ssid);$("wMac").textContent=F(d.mac);
    $("wIp").textContent=F(d.ip);$("wRssi").textContent=F(d.rssi)+" dBm";
    $("wGw").textContent=F(d.gw);$("wUptime").textContent=F(d.uptime);
    if(d.wok){$("wWth").textContent="正常";$("wWth").className="v g"}else{$("wWth").textContent="等待";$("wWth").className="v r"}
    $("brightVal").textContent=d.bright;
    if(d.page!=undefined&&d.page!=curPg){curPg=d.page;var b=document.querySelectorAll(".pg .b");for(var i=0;i<b.length;i++)b[i].className="b"+(i==d.page?" ac":"")}
    if(d.temp!=undefined){$("wthContainer").style.display="block";$("wthWaiting").style.display="none";
      $("wTemp").textContent=F(d.temp)+deg;$("wFeels").textContent=F(d.feels)+deg;
      $("wHumi").textContent=F(d.humi)+pct;$("wWind").textContent=F(d.wind);
      $("wTmax").textContent=F(d.tmax)+deg;$("wTmin").textContent=F(d.tmin)+deg;
      var fc=$("fcContainer");fc.innerHTML="";
      if(d.fc&&d.fc.length){for(var i=0;i<d.fc.length;i++){var x=d.fc[i];fc.innerHTML+="<div class=fc-i><div class=t2>"+F(x.h)+"</div><div class=t1>"+F(x.t)+deg+"</div></div>"}}
      var wc=$("warnContainer");wc.innerHTML="";
      if(d.wn&&d.wn.length){var m="";for(var i=0;i<d.wn.length;i++){var w=d.wn[i];var cl=w.s=="红色"?"rd":w.s=="橙色"?"or":w.s=="黄色"?"yl":"bl";m+="<span class=wg-i "+cl+">"+w.n+"</span>"}wc.innerHTML="<div class=wg>"+m+"</div>"}
      var pc=$("precipContainer");pc.innerHTML="";
      if(d.precip){pc.innerHTML="<div style=font-size:10px;color:var(--teal);font-family:Courier New,monospace;margin:2px 0>"+d.precip+"</div>"}
    }else{$("wthContainer").style.display="none";$("wthWaiting").style.display="block"}
    $("dbgContainer").innerHTML="<span>堆内存: "+F(d.heap)+"KB</span><span>NTP: "+(d.ntp?"已同步":"等待")+"</span><span>服务器: "+F(d.ntpS)+"</span><span>更新: "+F(d.wut)+"</span>";
    if(d.wint)$("wIntervalSel").value=d.wint
  }catch(e){console.log("poll err",e)}}
setInterval(poll,5000);poll();
</script></body></html>
)rawliteral";

// ---- Helpers ----
static int pageIdToWeb(PageId p) {
    switch (p) {
        case PAGE_MAIN: return 0;
        case PAGE_WARNING: return 1;
        case PAGE_MINUTELY: return 2;
        case PAGE_SYSTEM_INFO: return 3;
        case PAGE_HELP: return 4;
        default: return 0;
    }
}

static String fmtUptime() {
    unsigned long s = millis() / 1000;
    char buf[32];
    int h = s / 3600, m = (s % 3600) / 60, sec = s % 60;
    if (h > 0) snprintf(buf, sizeof(buf), "%dh %02dm", h, m);
    else if (m > 0) snprintf(buf, sizeof(buf), "%dm %02ds", m, sec);
    else snprintf(buf, sizeof(buf), "%ds", sec);
    return String(buf);
}

// ---- Route handlers ----
static void handleRoot() {
    Serial.println("[WEB] GET /");
    char ipStr[16], rssiStr[12];
    WiFi.localIP().toString().toCharArray(ipStr, sizeof(ipStr));
    snprintf(rssiStr, sizeof(rssiStr), "%d dBm", WiFi.RSSI());

    const char *wthCls = weather.valid ? "g" : "r";
    const char *wthTxt = weather.valid ? "正常" : "等待";

    PageId cur = getCurrentPage();
    int wp = pageIdToWeb(cur);
    char a0[4]="",a1[4]="",a2[4]="",a3[4]="",a4[4]="";
    switch (wp) { case 0: strcpy(a0,"ac"); break; case 1: strcpy(a1,"ac"); break;
                  case 2: strcpy(a2,"ac"); break; case 3: strcpy(a3,"ac"); break;
                  case 4: strcpy(a4,"ac"); break; }

    char *buf = new char[32768];
    int len = snprintf(buf, 32768, PAGE_TEMPLATE,
        ipStr, rssiStr, wthCls, wthTxt,
        weatherApiKey.c_str(), weatherHost.c_str(), weatherName.c_str(),
        g_brightness,
        a0, a1, a2, a3, a4, ipStr);
    if (len >= 32768)
        Serial.printf("[WEB] WARN: page truncated! need=%d, buf=%d\n", len + 1, 32768);
    server.send(200, "text/html; charset=utf-8", buf);
    Serial.printf("[WEB] GET / 200: %d bytes\n", len);
    delete[] buf;
}

static void handleSave() {
    bool hasK = server.hasArg("apiKey"), hasH = server.hasArg("host");
    String k = hasK ? server.arg("apiKey") : "";
    String h = hasH ? server.arg("host") : "";
    Serial.printf("[WEB] POST /save: hasApiKey=%d hasHost=%d apiKey=\"%s\" host=\"%s\"\n", hasK, hasH, k.c_str(), h.c_str());
    if (!hasK || !hasH) { server.send(400, "text/plain", "Missing"); return; }
    k.trim(); h.trim();
    if (k.length() == 0 || h.length() == 0) { server.send(400, "text/plain", "Empty"); return; }
    saveConfig(k, h);
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) { state.lastWeatherFetch = 0; xSemaphoreGive(dataMutex); }
    server.send(200, "text/plain", "OK");
}

static void handleSetCity() {
    bool hasC = server.hasArg("city");
    String c = hasC ? server.arg("city") : "";
    Serial.printf("[WEB] POST /setcity: hasCity=%d city=\"%s\"\n", hasC, c.c_str());
    if (!hasC) { server.send(400, "text/plain", "Missing city"); return; }
    c.trim();
    if (c.length() == 0) { server.send(400, "text/plain", "Empty"); return; }
    bool ok = setCityByName(c);
    server.send(ok ? 200 : 400, "text/plain", ok ? "OK" : "Failed");
}

static void handleRefresh() {
    Serial.println("[WEB] POST /refresh");
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        state.lastWeatherFetch = 0; state.lastMinutelyFetch = 0;
        xSemaphoreGive(dataMutex);
    }
    server.send(200, "text/plain", "OK");
}

static void handleBrightness() {
    bool hasL = server.hasArg("level");
    String lv = hasL ? server.arg("level") : "";
    Serial.printf("[WEB] POST /setbrightness: hasLevel=%d level=\"%s\"\n", hasL, lv.c_str());
    if (!hasL) { server.send(400, "text/plain", "Missing"); return; }
    int l = constrain(lv.toInt(), 0, 255);
    setBrightness((uint8_t)l);
    Serial.printf("[WEB] POST /setbrightness: 200 level=%d\n", l);
    server.send(200, "text/plain", "OK");
}

static void handleSetPage() {
    bool hasP = server.hasArg("page");
    String pv = hasP ? server.arg("page") : "";
    Serial.printf("[WEB] POST /setpage: hasPage=%d page=\"%s\"\n", hasP, pv.c_str());
    if (!hasP) { server.send(400, "text/plain", "Missing"); return; }
    int p = constrain(pv.toInt(), 0, 4);
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) { state.remotePage = (int8_t)p; xSemaphoreGive(dataMutex); }
    server.send(200, "text/plain", "OK");
}

static void handleControl() {
    String act = server.arg("action");
    String val = server.arg("val");
    Serial.printf("[WEB] POST /control: action=\"%s\" val=\"%s\"\n", act.c_str(), val.c_str());
    if (act == "rotate") {
        int r = constrain(val.toInt(), 0, 3);
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) { state.pendingRotation = (int8_t)r; xSemaphoreGive(dataMutex); }
        server.send(200, "text/plain", "OK");
    } else if (act == "wake") {
        setBrightness(255);
        server.send(200, "text/plain", "OK");
    } else if (act == "sleep") {
        setBrightness(1);
        server.send(200, "text/plain", "OK");
    } else if (act == "winterval") {
        uint32_t ms = (uint32_t)val.toInt();
        if (ms < 60000) ms = 60000;
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) { state.weatherIntervalMs = ms; xSemaphoreGive(dataMutex); }
        server.send(200, "text/plain", "OK");
    } else if (act == "reboot") {
        server.send(200, "text/plain", "OK");
        delay(500);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Unknown action");
    }
}

static void handleStatus() {
    String json = "{";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
    json += ",\"ssid\":\"" + WiFi.SSID() + "\"";
    json += ",\"gw\":\"" + WiFi.gatewayIP().toString() + "\"";
    json += ",\"mac\":\"" + WiFi.macAddress() + "\"";
    json += ",\"rssi\":" + String(WiFi.RSSI());
    json += ",\"uptime\":\"" + fmtUptime() + "\"";
    json += ",\"wok\":" + String(weather.valid ? "true" : "false");
    json += ",\"bright\":" + String(g_brightness);
    json += ",\"page\":" + String(pageIdToWeb(getCurrentPage()));
    json += ",\"heap\":" + String(ESP.getFreeHeap() / 1024);
    json += ",\"ntp\":" + String(state.timeSynced ? "true" : "false");
    json += ",\"ntpS\":\"" + String(state.ntpServer) + "\"";
    json += ",\"wint\":" + String(state.weatherIntervalMs);
    if (weather.valid) {
        json += ",\"temp\":\"" + weather.temp + "\"";
        json += ",\"feels\":\"" + weather.feelsLike + "\"";
        json += ",\"humi\":\"" + weather.humidity + "\"";
        json += ",\"wind\":\"" + weather.windDir + " " + weather.windScale + "级\"";
        json += ",\"tmax\":\"" + weather.tempMax + "\"";
        json += ",\"tmin\":\"" + weather.tempMin + "\"";
        json += ",\"wut\":\"" + weather.updateTime + "\"";
        json += ",\"fc\":[";
        if (hourly.valid) {
            for (int i = 0; i < 7; i++) {
                if (i > 0) json += ",";
                json += "{\"h\":\"" + hourly.hourLabel[i] + "\",\"t\":\"" + hourly.temp[i] + "\"}";
            }
        }
        json += "]";
        json += ",\"wn\":[";
        for (int i = 0; i < warningCount; i++) {
            if (i > 0) json += ",";
            json += "{\"n\":\"" + warnings[i].eventName + "\",\"s\":\"" + warnings[i].severity + "\"}";
        }
        json += "]";
        if (minutely.valid && minutely.summary.length() > 0) {
            json += ",\"precip\":\"" + minutely.summary + "\"";
        }
    }
    json += "}";
    server.send(200, "application/json", json);
}

// ---- OTA Firmware Update Page ----
static const char OTA_PAGE[] = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<meta charset="utf-8">
<style>
*{margin:0;padding:0;box-sizing:border-box}
:root{--bg:#1a1a1a;--card:#242424;--border:#3a3a3a;--amber:#e8c580;--gold:#f5d998;--teal:#00bfa5;--mute:#8a7a6a;--bg2:#202020;--red:#e85a4f;--fs:12px;--fs-s:10px;--fs-lg:14px;--p:16px;--r:6px}
@media(min-width:768px){:root{--fs:14px;--fs-s:12px;--fs-lg:18px;--p:28px;--r:8px}}
body{font-family:'Courier New',monospace;background:var(--bg);color:var(--amber);min-height:100vh;display:flex;justify-content:center;align-items:flex-start;margin:0;padding:var(--p)}
.panel{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:var(--p);width:100%;max-width:640px;box-shadow:0 4px 24px rgba(0,0,0,.5)}
h1{font-size:var(--fs-lg);color:var(--gold);text-align:center;letter-spacing:2px;font-weight:400;margin-bottom:12px}
.sec{margin-bottom:10px}
.sec .t{font-size:var(--fs-s);text-transform:uppercase;letter-spacing:1.5px;color:var(--mute);padding:4px 0 3px;border-bottom:1px solid var(--border);margin-bottom:6px}
.step{padding:4px 0;font-size:var(--fs-s);color:var(--amber);line-height:1.6}
.step .n{color:var(--teal);margin-right:6px}
.note{font-size:var(--fs-xs);color:var(--mute);padding:3px 0}
.warn{font-size:var(--fs-xs);color:var(--red);padding:3px 0}
.code{background:var(--bg2);border:1px solid var(--border);border-radius:4px;padding:5px 8px;font-size:var(--fs-xs);color:var(--teal);font-family:'Courier New',monospace;margin:3px 0 6px;word-break:break-all}
.uparea{border:2px dashed var(--border);border-radius:var(--r);padding:16px;text-align:center;margin:8px 0}
.uparea input[type=file]{display:none}
.uparea .flabel{display:inline-block;padding:7px 16px;border:1px solid var(--teal);border-radius:var(--r);background:linear-gradient(135deg,#00bfa522,#00bfa511);color:var(--teal);font-size:var(--fs-s);cursor:pointer;transition:all .2s;user-select:none;font-family:'Courier New',monospace}
.uparea .flabel:hover{background:linear-gradient(135deg,#00bfa544,#00bfa522);box-shadow:0 0 12px #00bfa533}
.uparea .fname{font-size:var(--fs-xs);color:var(--mute);margin-top:6px}
.btn{width:100%;padding:9px;border:1px solid var(--teal);border-radius:var(--r);background:linear-gradient(135deg,#00bfa522,#00bfa511);color:var(--teal);font-size:var(--fs-s);font-family:'Courier New',monospace;text-transform:uppercase;letter-spacing:2px;cursor:pointer;transition:all .2s;user-select:none;margin-top:8px}
.btn:hover{background:linear-gradient(135deg,#00bfa544,#00bfa522);box-shadow:0 0 12px #00bfa533}
.btn:active{transform:scale(.97)}
.btn:disabled{opacity:.4;cursor:not-allowed;transform:none}
.st{display:none;margin-top:8px;padding:8px;border-radius:var(--r);text-align:center;font-size:var(--fs-s);font-family:'Courier New',monospace}
.st.ok{display:block;background:#00bfa511;color:var(--teal);border:1px solid #00bfa533}
.st.er{display:block;background:var(--red)11;color:var(--red);border:1px solid var(--red)33}
.prow{display:none;margin-top:8px;background:var(--bg2);border-radius:var(--r);overflow:hidden;height:20px;position:relative}
.pro{height:100%;width:0%;background:linear-gradient(90deg,#00bfa5,#00d9b0);border-radius:var(--r);transition:width .3s}
.pt{position:absolute;top:0;left:0;right:0;height:20px;line-height:20px;text-align:center;font-size:var(--fs-xs);color:var(--card);font-weight:700;font-family:'Courier New',monospace}
.back{margin-top:12px;text-align:center}
.back a{color:var(--mute);font-size:var(--fs-xs);text-decoration:none;letter-spacing:1px}
.back a:hover{color:var(--teal)}
</style></head><body>
<div class="panel">
<h1>⚡ 固件升级</h1>
<div class="sec"><div class="t">操作步骤</div>
<div class="step"><span class="n">①</span>编译固件 — 在项目目录运行：</div>
<div class="code">pio run -e featheresp32</div>
<div class="step"><span class="n">②</span>选择生成的固件文件</div>
<div class="code">.pio/build/featheresp32/firmware.bin</div>
<div class="step"><span class="n">③</span>点击下方按钮上传</div>
<div class="step"><span class="n">④</span>等待约 10~20 秒</div>
<div class="step"><span class="n">⑤</span>设备自动重启，完成升级</div>
</div>
<div class="sec"><div class="t">注意事项</div>
<div class="warn">⚠ 升级过程中请勿断开电源</div>
<div class="note">✓ 采用 A/B 双槽设计，升级失败自动回退</div>
<div class="note">✓ 当前版本可放心测试</div>
</div>
<div class="uparea" id="dropArea">
<label class="flabel" id="fileLabel" for="fileInput">+ 选择固件文件</label>
<input type="file" id="fileInput" name="firmware" accept=".bin" onchange="onFilePick()" />
<div class="fname" id="fileName">未选择文件</div>
</div>
<button class="btn" id="upBtn" onclick="startUpload()" disabled>上传固件</button>
<div class="prow" id="proW"><div class="pro" id="proB"></div><div class="pt" id="proT">0%</div></div>
<div id="st" class="st"></div>
<div class="back"><a href="/">← 返回主页</a></div>
</div>
<script>
var picked=false;
function onFilePick(){var f=document.getElementById('fileInput');if(f.files.length>0){document.getElementById('fileName').textContent=f.files[0].name;document.getElementById('upBtn').disabled=false;picked=true}}
function startUpload(){if(!picked)return;var f=document.getElementById('fileInput');if(!f||!f.files||!f.files[0])return;var file=f.files[0];var btn=document.getElementById('upBtn');btn.disabled=true;btn.textContent='上传中...';document.getElementById('dropArea').style.display='none';document.getElementById('upBtn').style.display='none';document.getElementById('proW').style.display='block';var xhr=new XMLHttpRequest();xhr.upload.onprogress=function(e){if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);document.getElementById('proB').style.width=p+'%';document.getElementById('proT').textContent=p+'%'}};xhr.onload=function(){document.body.innerHTML=xhr.responseText};xhr.onerror=function(){document.getElementById('st').className='st er';document.getElementById('st').textContent='上传失败'};var fd=new FormData();fd.append('firmware',file);xhr.open('POST','/update?size='+file.size,true);xhr.send(fd)}
</script></body></html>
)rawliteral";

static void handleOtaGet() {
    Serial.println("[WEB] GET /update");
    server.send(200, "text/html; charset=utf-8", OTA_PAGE);
}

static void handleOtaUpload() {
    HTTPUpload &upload = server.upload();
    static unsigned long totalWritten = 0;
    static unsigned long totalSize = 0;

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[OTA] Start: %s\n", upload.filename.c_str());
        totalWritten = 0;
        totalSize = server.arg("size").toInt();
        otaProgress = 0;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        size_t written = Update.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize) {
            Update.printError(Serial);
        }
        totalWritten += written;
        if (totalSize > 0) {
            otaProgress = (int)(totalWritten * 100 / totalSize);
            if (otaProgress > 99) otaProgress = 99;
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end()) {
            Serial.printf("[OTA] Success: %lu bytes\n", totalWritten);
            otaProgress = 100;
        } else {
            Update.printError(Serial);
            otaProgress = -1;
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        otaProgress = -1;
        Serial.println("[OTA] Aborted");
    }
}

static void handleOtaPost() {
    if (Update.hasError()) {
        String err = Update.errorString();
        Serial.printf("[OTA] Error: %s\n", err.c_str());
        otaProgress = -1;
        String page = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><meta http-equiv=\"refresh\" content=\"5;url=/update\"><style>body{background:#1a1a1a;color:#e8c580;font-family:'Courier New',monospace;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;padding:20px}.c{background:#242424;border:1px solid #3a3a3a;border-radius:12px;padding:28px;max-width:480px;text-align:center}h2{color:#e85a4f;margin-bottom:12px}p{color:#8a7a6a;font-size:12px}</style></head><body><div class=\"c\"><h2>❌ 升级失败</h2><p>" + err + "</p><p style=\"margin-top:12px\">5 秒后返回升级页面</p></div></body></html>";
        server.send(200, "text/html; charset=utf-8", page);
    } else {
        Serial.println("[OTA] POST success, restarting...");
        String page = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><style>body{background:#1a1a1a;color:#e8c580;font-family:'Courier New',monospace;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;padding:20px}.c{background:#242424;border:1px solid #3a3a3a;border-radius:12px;padding:28px;max-width:480px;text-align:center}h2{color:#00bfa5;margin-bottom:12px}p{color:#8a7a6a;font-size:12px}.sp{display:inline-block;width:12px;height:12px;border:2px solid #00bfa5;border-top-color:transparent;border-radius:50%;animation:s .8s linear infinite;margin-top:12px}@keyframes s{to{transform:rotate(360deg)}}</style></head><body><div class=\"c\"><h2>✅ 升级成功</h2><p>设备正在重启...</p><div class=\"sp\"></div></div></body></html>";
        server.send(200, "text/html; charset=utf-8", page);
        delay(1000);
        ESP.restart();
    }
}

// ---- Lifecycle ----
void startConfigServer() {
    if (serverStarted) return;
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/setcity", HTTP_POST, handleSetCity);
    server.on("/refresh", HTTP_POST, handleRefresh);
    server.on("/setbrightness", HTTP_POST, handleBrightness);
    server.on("/setpage", HTTP_POST, handleSetPage);
    server.on("/control", HTTP_POST, handleControl);
    server.on("/status", handleStatus);
    server.on("/wifi", handleStatus);
    server.on("/update", HTTP_GET, handleOtaGet);
    server.on("/update", HTTP_POST, handleOtaPost, handleOtaUpload);
    server.begin();
    serverStarted = true;
    Serial.printf("[WEB] Server started on %s:80\n", WiFi.localIP().toString().c_str());
}

void handleConfigClient() {
    if (serverStarted) server.handleClient();
}
