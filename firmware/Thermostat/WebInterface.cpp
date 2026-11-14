#include "WebInterface.h"
#include "NetManager.h"

static const char WIFI_PAGE[] PROGMEM = R"rawliteral(<!doctype html><html><head><meta charset='utf-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'><title>WiFi Settings</title>
<style>body{font-family:Arial;max-width:560px;margin:20px auto;padding:0 16px;background:#f5f5f5;color:#333}
h1{font-size:22px}fieldset{background:#fff;border:1px solid #ddd;border-radius:8px;margin:14px 0;padding:12px}
legend{font-weight:bold}.row{display:flex;gap:6px;margin:6px 0}.row input{flex:1;min-width:0;padding:7px;border:1px solid #ccc;border-radius:4px}
button{padding:8px 14px;border:none;border-radius:6px;color:#fff;background:#2196F3;cursor:pointer;margin:4px 4px 0 0}
.del{background:#f44336;padding:7px 10px}.muted{color:#777;font-size:13px}#msg{margin:10px 0;font-weight:bold}</style></head><body>
<a href='/'>&larr; Back</a><h1>WiFi Settings</h1>
<div id='status' class='muted'>Loading...</div>
<fieldset><legend>Networks (joined by best signal)</legend>
<div class='muted'>Up to <span id='max'></span> networks. Leave a password blank to keep the saved one.
The built-in network <b id='builtin'></b> is always tried as a last resort.</div>
<div id='nets'></div><button type='button' onclick='addRow("","")'>+ Add network</button></fieldset>
<fieldset><legend>Fallback access point</legend>
<div class='muted'>Started automatically after 5 minutes without any network. Connect to it and open <b>http://192.168.4.1</b>.</div>
<div class='row'><input id='apSsid' placeholder='Access point name'></div>
<div class='row'><input id='apPassword' type='password' placeholder='Password (8+ characters, blank = keep)'></div>
<button type='button' onclick='testAp()'>Test access point (3 min)</button></fieldset>
<button type='button' onclick='save()'>Save</button><div id='msg'></div>
<script>
const H={'X-Requested-With':'thermostat','Content-Type':'application/json'};let max=5;
function esc(s){return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function addRow(ssid){const n=document.getElementById('nets');if(n.children.length>=max){alert('Maximum '+max+' networks');return}
const d=document.createElement('div');d.className='row';d.innerHTML="<input class='ssid' placeholder='Network name' value='"+esc(ssid)+"'><input class='pw' type='password' placeholder='Password'><button type='button' class='del' onclick='this.parentNode.remove()'>&times;</button>";n.appendChild(d)}
async function getState(){return (await fetch('/wifi.json')).json()}
// The form is filled once on page load and after a successful save; the
// periodic refresh only touches the status line, so typing is never lost.
async function loadForm(){const d=await getState();max=d.maxNetworks;
document.getElementById('max').textContent=max;document.getElementById('builtin').textContent=d.builtinSsid;
document.getElementById('nets').innerHTML='';d.networks.forEach(n=>addRow(n.ssid));
document.getElementById('apSsid').value=d.apSsid;showStatus(d)}
async function refreshStatus(){try{showStatus(await getState())}catch(e){}}
function showStatus(d){
let s=d.connected?('Connected to <b>'+esc(d.currentSsid)+'</b> ('+d.rssi+' dBm), '+esc(d.ip)):'<b>Not connected</b>';
if(d.apActive)s+=' &middot; Access point active at '+esc(d.apIp);if(d.pendingConfirm)s+=' &middot; new settings not yet confirmed (auto-rollback in 3 min)';
document.getElementById('status').innerHTML=s}
async function save(){const nets=[...document.querySelectorAll('#nets .row')].map(r=>({ssid:r.querySelector('.ssid').value.trim(),password:r.querySelector('.pw').value})).filter(n=>n.ssid);
const r=await fetch('/wifi',{method:'POST',headers:H,body:JSON.stringify({networks:nets,apSsid:document.getElementById('apSsid').value.trim(),apPassword:document.getElementById('apPassword').value})});
const d=await r.json();const m=document.getElementById('msg');m.style.color=r.ok?'green':'red';m.textContent=r.ok?'Saved.':(d.error||'Error');if(r.ok){document.getElementById('apPassword').value='';loadForm()}}
async function testAp(){const r=await fetch('/wifi/testap',{method:'POST',headers:H});const m=document.getElementById('msg');m.style.color=r.ok?'green':'red';
m.textContent=r.ok?'Access point started for 3 minutes. Look for it on your phone.':'Error';setTimeout(refreshStatus,1500)}
loadForm();setInterval(refreshStatus,10000);
</script></body></html>)rawliteral";
#include <time.h>

// Format a log entry's timestamp for display: HH:MM:SS UTC when NTP is
// synced, or a compact uptime like "+1h23m" / "+5m25s" / "+12s" before that.
static String formatLogTime(const LogEntry& entry) {
  if (entry.epoch >= 1000000000) {
    // The device keeps UTC; browsers convert to their own local time
    // (see the data-epoch script on /logs and the epoch field in logs.json).
    struct tm tinfo;
    gmtime_r(&entry.epoch, &tinfo);
    char buf[12];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             tinfo.tm_hour, tinfo.tm_min, tinfo.tm_sec);
    return String(buf);
  }
  unsigned long sec = entry.timestamp / 1000;
  unsigned long h = sec / 3600;
  unsigned long m = (sec % 3600) / 60;
  unsigned long s = sec % 60;
  char buf[16];
  if (h > 0)      snprintf(buf, sizeof(buf), "+%luh%lum", h, m);
  else if (m > 0) snprintf(buf, sizeof(buf), "+%lum%lus", m, s);
  else            snprintf(buf, sizeof(buf), "+%lus", s);
  return String(buf);
}

// Escape text for safe inclusion in HTML element content / attribute values.
static String htmlEscape(const String& in) {
  String out;
  out.reserve(in.length() + 16);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '&':  out += F("&amp;"); break;
      case '<':  out += F("&lt;"); break;
      case '>':  out += F("&gt;"); break;
      case '"':  out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default:   out += c;
    }
  }
  return out;
}

// Strict float parse: the whole argument must be a finite number.
static bool parseFloatArg(const String& s, float& out) {
  if (s.length() == 0) return false;
  char* end = nullptr;
  double v = strtod(s.c_str(), &end);
  if (end == s.c_str() || *end != '\0' || !isfinite(v)) return false;
  out = (float)v;
  return true;
}

// Strict int parse: the whole argument must be an integer.
static bool parseIntArg(const String& s, int& out) {
  if (s.length() == 0) return false;
  char* end = nullptr;
  long v = strtol(s.c_str(), &end, 10);
  if (end == s.c_str() || *end != '\0') return false;
  out = (int)v;
  return true;
}

// Collapses runs of routine heartbeat / temp-post / command-poll messages so
// only the newest of each kind is shown. Call newest-to-oldest.
struct RepetitiveFilter {
  bool seenHeartbeat = false, seenTempPost = false, seenPoll = false;
  bool isRepetitive(const String& msg) {
    if (msg.indexOf("eartbeat") >= 0) {
      if (seenHeartbeat) return true;
      seenHeartbeat = true;
    } else if (msg.indexOf("Posted temp") >= 0 || msg.indexOf("Temperature posted") >= 0 || msg.indexOf("/temperature :") >= 0) {
      if (seenTempPost) return true;
      seenTempPost = true;
    } else if (msg.indexOf("No pending commands") >= 0 || msg.indexOf("Polling commands") >= 0 || msg.indexOf("/commands/pending") >= 0) {
      if (seenPoll) return true;
      seenPoll = true;
    }
    return false;
  }
};

WebInterface::WebInterface(TemperatureManager& tempMgr, RelayController& relayCtrl,
                           ConfigManager& cfgMgr, ApiClient& apiCli, int& updateFreq, bool& useFahr)
  : server(WEB_SERVER_PORT),
    tempManager(tempMgr),
    relayController(relayCtrl),
    configManager(cfgMgr),
    apiClient(apiCli),
    updateFrequency(updateFreq),
    useFahrenheit(useFahr) {
}

void WebInterface::begin() {
  // Setup web server routes
  // Read-only pages: GET.
  server.on("/", HTTP_GET, [this]() { this->handleRoot(); });
  server.on("/status", HTTP_GET, [this]() { this->handleStatus(); });
  server.on("/logs", HTTP_GET, [this]() { this->handleLogs(); });
  server.on("/logs.json", HTTP_GET, [this]() { this->handleLogsJson(); });
  server.on("/data", HTTP_GET, [this]() { this->handleData(); });
  server.on("/wifi", HTTP_GET, [this]() { server.send_P(200, "text/html", WIFI_PAGE); });
  server.on("/wifi.json", HTTP_GET, [this]() {
    JsonDocument doc;
    net.toJson(doc);
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });
  server.on("/wifi", HTTP_POST, [this]() {
    if (!checkCsrf()) return;
    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain"))) { sendError("Invalid JSON"); return; }
    String err = net.applySettings(doc);
    if (err.length()) { sendError(err); return; }
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/wifi/testap", HTTP_POST, [this]() {
    if (!checkCsrf()) return;
    net.startTestAp();
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/sensordiag", HTTP_GET, [this]() {
    server.send(200, "application/json", tempManager.diagnostics());
  });

  // State-changing actions: POST + custom header only. A custom header can't
  // be sent cross-origin without a CORS preflight (which we never approve),
  // so other web pages can't drive the relays through a LAN user's browser.
  server.on("/setmode", HTTP_POST, [this]() { if (checkCsrf()) this->handleSetMode(); });
  server.on("/settype", HTTP_POST, [this]() { if (checkCsrf()) this->handleSetType(); });
  server.on("/setthresholds", HTTP_POST, [this]() { if (checkCsrf()) this->handleSetThresholds(); });
  server.on("/setfreq", HTTP_POST, [this]() { if (checkCsrf()) this->handleSetFrequency(); });
  server.on("/setunit", HTTP_POST, [this]() { if (checkCsrf()) this->handleSetUnit(); });
  server.on("/cleardata", HTTP_POST, [this]() { if (checkCsrf()) this->handleClearData(); });

  server.onNotFound([this]() {
    // Captive portal: phones joining our access point probe URLs like
    // /generate_204 (Android) or /hotspot-detect.html (Apple) on other
    // hosts; DNS sends them here, and a redirect makes the phone pop up the
    // WiFi settings page. Requests over the normal LAN get a plain 404.
    if (NetManager::requestViaAp(server.client().localIP())) {
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/wifi", true);
      server.send(302, "text/plain", "");
      return;
    }
    server.send(404, "text/plain", "Not found");
  });

  server.collectHeaders(CSRF_HEADER);

  server.begin();
  logger.addLog("Web server started");
}

void WebInterface::handleClient() {
  server.handleClient();
}

bool WebInterface::checkCsrf() {
  if (server.header(CSRF_HEADER) == CSRF_VALUE) return true;
  server.send(403, "application/json", "{\"error\":\"Forbidden\"}");
  return false;
}

void WebInterface::sendError(const String& message) {
  JsonDocument doc;
  doc["error"] = message;
  String out;
  serializeJson(doc, out);
  server.send(400, "application/json", out);
}

// Store large static content in PROGMEM to avoid RAM usage
static const char CSS_CONTENT[] PROGMEM = R"rawliteral(<style>
body{font-family:Arial;text-align:center;background:#f5f5f5}
h1{color:#333}
.temp{font-size:28px;font-weight:bold;color:#2196F3;margin:20px 0}
table{margin:20px auto;border-collapse:collapse;background:#fff;box-shadow:0 2px 8px rgba(0,0,0,0.1)}
td,th{padding:10px 14px;border-bottom:1px solid #ddd}
th{background:#f9f9f9;font-weight:bold}
.status-cell{color:white;padding:8px;font-weight:bold;border-radius:4px}
.on{background:#4CAF50}
.off{background:#f44336}
button{padding:6px 12px;margin:2px;border:none;border-radius:6px;cursor:pointer;color:white;font-size:13px}
.btn-auto{background:#2196F3}
.btn-on{background:#4CAF50}
.btn-off{background:#f44336}
button:hover{opacity:0.8}
input[type="number"],select{width:60px;padding:6px;text-align:center;border:1px solid #ddd;border-radius:4px}
#chart-container{width:90%;max-width:800px;height:300px;margin:30px auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 8px rgba(0,0,0,0.1)}
</style>
)rawliteral";

static const char BODY_HTML[] PROGMEM = R"rawliteral(<h1>Thermostat Control</h1>
<div style='margin-bottom:15px;'>
<a href='/logs' style='color:#2196F3;text-decoration:none;font-size:14px;'>View System Logs</a>
<span style='margin:0 15px;'>|</span>
<a href='/wifi' style='color:#2196F3;text-decoration:none;font-size:14px;'>WiFi Settings</a>
<span style='margin:0 15px;'>|</span>
<button id='unitToggle' class='btn-auto' onclick='toggleUnit()' style='padding:4px 10px;font-size:12px;'>°F</button>
</div>
<div class='temp'>Temperature: <span id='temp'>--</span><span id='unit'>&deg;C</span></div>
<div id='fault' style='display:none;margin:10px auto;max-width:600px;padding:10px;background:#f44336;color:#fff;border-radius:6px;font-weight:bold;'>SENSOR FAULT: no valid temperature. AUTO relays are OFF until the sensor recovers.</div>
<table>
<tr><th>Relay</th><th>State</th><th>Type</th><th>Mode</th><th>ON</th><th>OFF</th><th>Actions</th></tr>
<tr><td><b>1</b></td><td><span class='status-cell' id='status0'>--</span></td>
<td><select id='type0' onchange='setType(0)'><option value='HEATING'>Heat</option><option value='COOLING'>Cool</option><option value='GENERIC'>Gen</option><option value='MANUAL_ONLY'>Man</option></select></td>
<td id='mode0'>--</td><td><input type='number' id='on0' step='0.1'><span class='unit-label'>&deg;C</span></td>
<td><input type='number' id='off0' step='0.1'><span class='unit-label'>&deg;C</span></td>
<td><button class='btn-auto' onclick='setMode(0,"AUTO")'>A</button><button class='btn-on' onclick='setMode(0,"ON")'>On</button><button class='btn-off' onclick='setMode(0,"OFF")'>Off</button><button class='btn-auto' onclick='saveThreshold(0)'>Save</button></td></tr>
<tr><td><b>2</b></td><td><span class='status-cell' id='status1'>--</span></td>
<td><select id='type1' onchange='setType(1)'><option value='HEATING'>Heat</option><option value='COOLING'>Cool</option><option value='GENERIC'>Gen</option><option value='MANUAL_ONLY'>Man</option></select></td>
<td id='mode1'>--</td><td><input type='number' id='on1' step='0.1'><span class='unit-label'>&deg;C</span></td>
<td><input type='number' id='off1' step='0.1'><span class='unit-label'>&deg;C</span></td>
<td><button class='btn-auto' onclick='setMode(1,"AUTO")'>A</button><button class='btn-on' onclick='setMode(1,"ON")'>On</button><button class='btn-off' onclick='setMode(1,"OFF")'>Off</button><button class='btn-auto' onclick='saveThreshold(1)'>Save</button></td></tr>
<tr><td><b>3</b></td><td><span class='status-cell' id='status2'>--</span></td>
<td><select id='type2' onchange='setType(2)'><option value='HEATING'>Heat</option><option value='COOLING'>Cool</option><option value='GENERIC'>Gen</option><option value='MANUAL_ONLY'>Man</option></select></td>
<td id='mode2'>--</td><td><input type='number' id='on2' step='0.1'><span class='unit-label'>&deg;C</span></td>
<td><input type='number' id='off2' step='0.1'><span class='unit-label'>&deg;C</span></td>
<td><button class='btn-auto' onclick='setMode(2,"AUTO")'>A</button><button class='btn-on' onclick='setMode(2,"ON")'>On</button><button class='btn-off' onclick='setMode(2,"OFF")'>Off</button><button class='btn-auto' onclick='saveThreshold(2)'>Save</button></td></tr>
<tr><td><b>4</b></td><td><span class='status-cell' id='status3'>--</span></td>
<td><select id='type3' onchange='setType(3)'><option value='HEATING'>Heat</option><option value='COOLING'>Cool</option><option value='GENERIC'>Gen</option><option value='MANUAL_ONLY'>Man</option></select></td>
<td id='mode3'>--</td><td><input type='number' id='on3' step='0.1'><span class='unit-label'>&deg;C</span></td>
<td><input type='number' id='off3' step='0.1'><span class='unit-label'>&deg;C</span></td>
<td><button class='btn-auto' onclick='setMode(3,"AUTO")'>A</button><button class='btn-on' onclick='setMode(3,"ON")'>On</button><button class='btn-off' onclick='setMode(3,"OFF")'>Off</button><button class='btn-auto' onclick='saveThreshold(3)'>Save</button></td></tr>
</table>
<div style='margin:20px auto;max-width:600px;'>
<p><label style='font-weight:bold;'>Update Frequency:</label>
<input id='freq' type='number' min='5' max='300' value='5' style='width:80px;padding:6px;margin:0 10px;'>sec
<button class='btn-auto' onclick='setFrequency()'>Set</button></p>
<p><button class='btn-off' onclick='clearData()' style='padding:8px 16px;'>Clear Data</button>
<span id='clearStatus' style='margin-left:10px;font-size:12px;'></span></p>
</div>
<div id='chart-container'><canvas id='tempChart'></canvas></div>
)rawliteral";

static const char JS_CONTENT[] PROGMEM = R"rawliteral(<script src='https://cdn.jsdelivr.net/npm/chart.js@3.9.1/dist/chart.min.js'></script>
<script>
const editedInputs=new Set();let useFahrenheit=false;
async function post(u){const r=await fetch(u,{method:'POST',headers:{'X-Requested-With':'thermostat'}});const d=await r.json();if(!r.ok){alert(d.error||'Request failed');await updateStatus();return null}return d}
function c2f(c){return(c*9/5)+32}
function f2c(f){return(f-32)*5/9}
async function toggleUnit(){try{useFahrenheit=!useFahrenheit;const d=await post('/setunit?fahrenheit='+(useFahrenheit?'1':'0'));updateUnitLabels();if(d)updateFromData(d)}catch(e){await updateStatus()}}
function updateUnitLabels(){const u=useFahrenheit?'°F':'°C';document.getElementById('unit').innerHTML=u;document.getElementById('unitToggle').innerHTML=useFahrenheit?'°C':'°F';document.querySelectorAll('.unit-label').forEach(el=>el.innerHTML=u)}
window.addEventListener('DOMContentLoaded',()=>{for(let i=0;i<4;i++){document.getElementById('on'+i).addEventListener('input',e=>editedInputs.add(e.target.id));document.getElementById('off'+i).addEventListener('input',e=>editedInputs.add(e.target.id))}});
async function setMode(relay,mode){try{const d=await post('/setmode?relay='+relay+'&mode='+mode);if(d)updateFromData(d)}catch(e){await updateStatus()}}
async function setType(relay){try{const t=document.getElementById('type'+relay).value;const d=await post('/settype?relay='+relay+'&type='+t);if(d)updateFromData(d)}catch(e){await updateStatus()}}
async function saveThreshold(relay){try{let on=parseFloat(document.getElementById('on'+relay).value);let off=parseFloat(document.getElementById('off'+relay).value);if(isNaN(on)||isNaN(off)){alert('Enter both ON and OFF temperatures');return}if(useFahrenheit){on=f2c(on);off=f2c(off)}const d=await post('/setthresholds?relay='+relay+'&on='+on.toFixed(2)+'&off='+off.toFixed(2));if(d){editedInputs.delete('on'+relay);editedInputs.delete('off'+relay);updateFromData(d)}}catch(e){await updateStatus()}}
async function setFrequency(){try{const f=parseInt(document.getElementById('freq').value);if(isNaN(f)||f<5){alert('Min 5s');return}if(f>300){alert('Max 300s');return}const d=await post('/setfreq?sec='+f);if(d)updateFromData(d)}catch(e){await updateStatus()}}
async function clearData(){if(!confirm('Clear all data?'))return;const s=document.getElementById('clearStatus');s.innerText='Clearing...';try{const d=await post('/cleardata');s.innerText=d?(d.message||'Done'):'Error';setTimeout(()=>{s.innerText='';updateChart()},3000)}catch(e){s.innerText='Error'}}
function updateFromData(d){if(d.useFahrenheit!==undefined&&d.useFahrenheit!==useFahrenheit){useFahrenheit=d.useFahrenheit;updateUnitLabels()}const te=document.getElementById('temp');if(typeof d.temp!=='number'){te.innerText='--'}else{te.innerText=(useFahrenheit?c2f(d.temp):d.temp).toFixed(1)}const fe=document.getElementById('fault');if(fe)fe.style.display=d.sensorFault?'block':'none';if(d.freq){const fi=document.getElementById('freq');if(document.activeElement!==fi)fi.value=d.freq}for(let i=0;i<4;i++){const st=document.getElementById('status'+i);st.innerText=d.relays[i].state?'ON':'OFF';st.className='status-cell '+(d.relays[i].state?'on':'off');document.getElementById('mode'+i).innerText=d.relays[i].mode;const ts=document.getElementById('type'+i);if(document.activeElement!==ts&&d.relays[i].type)ts.value=d.relays[i].type;const oi=document.getElementById('on'+i),fi=document.getElementById('off'+i);const don=useFahrenheit?c2f(d.relays[i].tempOn):d.relays[i].tempOn;const doff=useFahrenheit?c2f(d.relays[i].tempOff):d.relays[i].tempOff;if(document.activeElement!==oi&&!editedInputs.has('on'+i))oi.value=don.toFixed(1);if(document.activeElement!==fi&&!editedInputs.has('off'+i))fi.value=doff.toFixed(1)}}
async function updateStatus(){try{const r=await fetch('/status');updateFromData(await r.json())}catch(e){}}
const ctx=document.getElementById('tempChart').getContext('2d');
const tempChart=new Chart(ctx,{type:'line',data:{labels:[],datasets:[{label:'Temp',data:[],borderColor:'#2196F3',backgroundColor:'rgba(33,150,243,0.1)',borderWidth:2,tension:0.4,pointRadius:1}]},options:{responsive:true,maintainAspectRatio:true,interaction:{intersect:false,mode:'index'},scales:{y:{beginAtZero:false},x:{ticks:{maxRotation:45,autoSkip:true,maxTicksLimit:12}}},plugins:{legend:{display:true}},animation:false}});
async function updateChart(){try{const r=await fetch('/data');const t=await r.text();const lines=t.trim().split('\n');const temps=[],times=[];for(const line of lines){if(line.startsWith('#')||!line)continue;const p=line.split(',');if(p.length>=2){const ts=parseInt(p[0]);let temp=parseFloat(p[1]);if(!isNaN(ts)&&!isNaN(temp)){if(useFahrenheit)temp=c2f(temp);let dt;if(ts>1e9)dt=new Date(ts*1000).toLocaleTimeString();else{const h=Math.floor(ts/3600),m=Math.floor((ts%3600)/60);dt=h+'h'+m+'m'}temps.push(temp);times.push(dt)}}}tempChart.data.labels=times;tempChart.data.datasets[0].data=temps;tempChart.update('none')}catch(e){}}
updateStatus();setInterval(updateStatus,2000);updateChart();setInterval(updateChart,30000);
</script>
)rawliteral";


void WebInterface::handleRoot() {
  // Use chunked transfer to stream from PROGMEM without RAM allocation
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  // Send HTML in chunks directly from flash memory
  server.sendContent_P(PSTR("<!doctype html><html><head><meta charset='utf-8'>"));
  server.sendContent_P(PSTR("<meta name='viewport' content='width=device-width,initial-scale=1'>"));
  server.sendContent_P(PSTR("<title>Thermostat</title>"));
  server.sendContent_P(CSS_CONTENT);
  server.sendContent_P(PSTR("</head><body>"));
  server.sendContent_P(BODY_HTML);
  server.sendContent_P(JS_CONTENT);
  server.sendContent_P(PSTR("</body></html>"));
}

void WebInterface::handleStatus() {
  // Build response via ArduinoJson + a single reserved String buffer to
  // avoid the heap-fragmenting String concat that the /status endpoint hits
  // every 2 seconds from the browser.
  JsonDocument doc;
  if (tempManager.hasValidTemp()) doc["temp"] = tempManager.getCurrentTemp();
  else doc["temp"] = nullptr;
  doc["sensorFault"] = relayController.hasSensorFault();
  doc["freq"] = updateFrequency;
  doc["useFahrenheit"] = useFahrenheit;
  JsonArray relays = doc["relays"].to<JsonArray>();
  for (int i = 0; i < 4; i++) {
    JsonObject r = relays.add<JsonObject>();
    r["state"]   = relayController.getRelayState(i);
    r["mode"]    = RelayController::modeToString(relayController.getRelayMode(i));
    r["type"]    = RelayController::typeToString(relayController.getRelayType(i));
    r["tempOn"]  = relayController.getTempOn(i);
    r["tempOff"] = relayController.getTempOff(i);
  }
  String out;
  out.reserve(measureJson(doc) + 1);
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// Parse and range-check the "relay" argument (0-3). Sends 400 on failure.
bool WebInterface::parseRelayArg(int& relay) {
  if (!parseIntArg(server.arg("relay"), relay) || relay < 0 || relay > 3) {
    sendError("Invalid relay");
    return false;
  }
  return true;
}

void WebInterface::handleSetMode() {
  int relay;
  if (!parseRelayArg(relay)) return;
  Mode mode;
  if (!RelayController::parseMode(server.arg("mode"), mode)) {
    sendError("Invalid mode");
    return;
  }
  relayController.setRelayMode(relay, mode);
  logger.addLog("Relay " + String(relay + 1) + " mode: " + RelayController::modeToString(mode));
  relayController.applyRelayLogic(tempManager.getCurrentTemp());
  configManager.saveSettings(updateFrequency, useFahrenheit);
  apiClient.markRelayDirty(relay);
  handleStatus();
}

void WebInterface::handleSetType() {
  int relay;
  if (!parseRelayArg(relay)) return;
  RelayType type;
  if (!RelayController::parseType(server.arg("type"), type)) {
    sendError("Invalid type");
    return;
  }
  relayController.setRelayType(relay, type);
  logger.addLog("Relay " + String(relay + 1) + " type: " + RelayController::typeToString(type));
  relayController.applyRelayLogic(tempManager.getCurrentTemp());
  configManager.saveSettings(updateFrequency, useFahrenheit);
  apiClient.markRelayDirty(relay);
  handleStatus();
}

void WebInterface::handleSetThresholds() {
  int relay;
  if (!parseRelayArg(relay)) return;
  float tempOn, tempOff;
  if (!parseFloatArg(server.arg("on"), tempOn) || !parseFloatArg(server.arg("off"), tempOff) ||
      !RelayController::isValidThreshold(tempOn) || !RelayController::isValidThreshold(tempOff)) {
    sendError("Thresholds must be numbers between " + String(THRESHOLD_MIN, 0) +
              " and " + String(THRESHOLD_MAX, 0) + " C");
    return;
  }
  if (!relayController.setTempThresholds(relay, tempOn, tempOff)) {
    sendError(String("Invalid thresholds: ") +
              RelayController::thresholdRule(relayController.getRelayType(relay)));
    return;
  }
  logger.addLog("Relay " + String(relay + 1) + " thresholds: ON=" +
                String(tempOn, 1) + "C, OFF=" + String(tempOff, 1) + "C");
  relayController.applyRelayLogic(tempManager.getCurrentTemp());
  configManager.saveSettings(updateFrequency, useFahrenheit);
  apiClient.markRelayDirty(relay);
  handleStatus();
}

void WebInterface::handleSetFrequency() {
  int sec;
  if (!parseIntArg(server.arg("sec"), sec)) {
    sendError("Invalid frequency");
    return;
  }
  if (sec < MIN_UPDATE_FREQUENCY) sec = MIN_UPDATE_FREQUENCY;
  if (sec > MAX_UPDATE_FREQUENCY) sec = MAX_UPDATE_FREQUENCY;
  updateFrequency = sec;
  logger.addLog("Update frequency set to " + String(sec) + "s");
  configManager.saveSettings(updateFrequency, useFahrenheit);
  handleStatus();
}

void WebInterface::handleClearData() {
  if (LittleFS.exists(LOG_FILE)) {
    LittleFS.remove(LOG_FILE);
    tempManager.onLogCleared();
    logger.addLog("Temperature data cleared by user");
    server.send(200, "application/json", "{\"message\":\"Data cleared successfully\"}");
  } else {
    server.send(200, "application/json", "{\"message\":\"No data to clear\"}");
  }
}

void WebInterface::handleSetUnit() {
  String f = server.arg("fahrenheit");
  if (f != "0" && f != "1") {
    sendError("Invalid unit");
    return;
  }
  useFahrenheit = (f == "1");
  logger.addLog("Unit changed to " + String(useFahrenheit ? "Fahrenheit" : "Celsius"));
  configManager.saveSettings(updateFrequency, useFahrenheit);
  server.send(200, "application/json", "{\"useFahrenheit\":" + String(useFahrenheit ? "true" : "false") + "}");
}

void WebInterface::handleLogs() {
  bool filterRepetitive = !server.hasArg("all");
  int page = 1;
  if (!parseIntArg(server.arg("page"), page) || page < 1) page = 1;
  const int logsPerPage = 100;

  const size_t logCount = logger.getLogCount();

  // Build filtered index list, newest first. Indices are into the logger's
  // age order (0 = oldest).
  std::vector<int> filteredIndices;
  filteredIndices.reserve(logCount);
  RepetitiveFilter filter;
  for (int i = (int)logCount - 1; i >= 0; i--) {
    if (!filterRepetitive || !filter.isRepetitive(logger.getLog(i).message)) {
      filteredIndices.push_back(i);
    }
  }

  int totalFiltered = filteredIndices.size();
  int totalPages = (totalFiltered + logsPerPage - 1) / logsPerPage;
  if (totalPages == 0) totalPages = 1;
  if (page > totalPages) page = totalPages;
  int startIdx = (page - 1) * logsPerPage;
  int endIdx = min(startIdx + logsPerPage, totalFiltered);

  // Stream the response so we never hold the full page in RAM at once.
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  server.sendContent_P(PSTR(
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<meta http-equiv='refresh' content='10'><title>System Logs</title>"
    "<style>body{font-family:monospace;background:#1e1e1e;color:#d4d4d4;padding:20px;}"
    ".log{background:#252526;padding:8px;margin:3px 0;border-radius:3px;font-size:13px;}"
    ".log.relay{border-left:3px solid #4CAF50;}"
    ".log.error{border-left:3px solid #f44336;}"
    ".log.cmd{border-left:3px solid #2196F3;}"
    "h2{color:#4CAF50;}"
    ".nav{margin:10px 0;}.nav a{color:#2196F3;margin-right:15px;}"
    ".filter{margin:10px 0;padding:10px;background:#252526;border-radius:5px;}"
    "</style></head><body>"));

  server.sendContent("<h2>System Logs (" + String(logCount) + " total)</h2>");
  server.sendContent_P(PSTR("<a href='/' style='color:#2196F3;'>Back to Main</a><div class='filter'>"));
  if (filterRepetitive) {
    server.sendContent_P(PSTR("Showing filtered logs (hiding repetitive heartbeats/posts) - <a href='/logs?all=1'>Show All</a>"));
  } else {
    server.sendContent_P(PSTR("Showing all logs - <a href='/logs'>Filter Repetitive</a>"));
  }
  server.sendContent_P(PSTR("</div><div class='nav'>"));

  if (page > 1) {
    server.sendContent("<a href='/logs?page=" + String(page - 1) + (filterRepetitive ? "" : "&all=1") + "'>&lt; Prev</a>");
  }
  server.sendContent("Page " + String(page) + " of " + String(totalPages) + " (" + String(totalFiltered) + " entries)");
  if (page < totalPages) {
    server.sendContent("<a href='/logs?page=" + String(page + 1) + (filterRepetitive ? "" : "&all=1") + "'>Next &gt;</a>");
  }
  server.sendContent_P(PSTR("</div>"));

  for (int i = startIdx; i < endIdx; i++) {
    const LogEntry& entry = logger.getLog(filteredIndices[i]);
    const String& msg = entry.message;

    const char* logClass = "log";
    if (msg.indexOf("Relay") >= 0) logClass = "log relay";
    else if (msg.indexOf("ERROR") >= 0 || msg.indexOf("Error") >= 0 || msg.indexOf("failed") >= 0 ||
             msg.indexOf("FAULT") >= 0) logClass = "log error";
    else if (msg.indexOf("command") >= 0 || msg.indexOf("Command") >= 0) logClass = "log cmd";

    // Log text can contain request arguments — always escape it.
    server.sendContent(String("<div class='") + logClass + "'>[<span" +
                       (entry.epoch >= 1000000000 ? " data-epoch='" + String((unsigned long)entry.epoch) + "'" : "") +
                       ">" + formatLogTime(entry) + "</span>] " +
                       htmlEscape(msg) + "</div>");
    if (((i - startIdx) & 0x0F) == 0) yield(); // feed watchdog every 16 entries
  }

  // Device times are UTC; show them in the viewer's local time.
  server.sendContent_P(PSTR("<script>document.querySelectorAll('[data-epoch]').forEach(e=>{"
    "e.textContent=new Date(e.dataset.epoch*1000).toLocaleTimeString([], {hour:'2-digit',minute:'2-digit',second:'2-digit'})})</script>"));
  server.sendContent_P(PSTR("</body></html>"));
  server.sendContent("");
}

void WebInterface::handleLogsJson() {
  // Return logs as JSON for remote access (read by the backend dashboard
  // from the user's browser, hence the CORS header).
  int limit = 100;
  if (!parseIntArg(server.arg("limit"), limit)) limit = 100;
  if (limit < 1) limit = 1;
  if (limit > (int)MAX_SYSTEM_LOGS) limit = MAX_SYSTEM_LOGS;

  bool filterRepetitive = !server.hasArg("all");
  const size_t logCount = logger.getLogCount();

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET");

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("{\"total\":" + String(logCount) + ",\"logs\":[");

  RepetitiveFilter filter;
  int count = 0;
  bool first = true;

  for (int i = (int)logCount - 1; i >= 0 && count < limit; i--) {
    const LogEntry& entry = logger.getLog(i);
    if (filterRepetitive && filter.isRepetitive(entry.message)) continue;

    JsonDocument doc;
    doc["ts"] = entry.timestamp / 1000;
    doc["time"] = formatLogTime(entry);  // HH:MM:SS UTC, or uptime before NTP
    // UTC seconds for clients to show in their own timezone (null pre-NTP).
    if (entry.epoch >= 1000000000) doc["epoch"] = (unsigned long)entry.epoch;
    else doc["epoch"] = nullptr;
    doc["msg"] = entry.message;
    // Note: serializeJson(doc, String&) replaces the string's contents, so
    // the separator must be sent on its own.
    if (!first) server.sendContent(",");
    first = false;
    String item;
    item.reserve(measureJson(doc) + 1);
    serializeJson(doc, item);
    server.sendContent(item);
    count++;
  }

  server.sendContent("]}");
  server.sendContent("");
}

void WebInterface::handleData() {
  if (!LittleFS.exists(LOG_FILE)) {
    server.send(200, "text/plain", "# No data logged yet\n");
    return;
  }

  File f = LittleFS.open(LOG_FILE, "r");
  if (!f) {
    server.send(500, "text/plain", "# Error: Cannot open log file\n");
    return;
  }

  size_t fileSize = f.size();
  if (fileSize == 0) {
    f.close();
    server.send(200, "text/plain", "# Log file is empty\n");
    return;
  }

  // Calculate lines needed for 24 hours based on update frequency
  // Cap at MAX_DATA_LINES for memory safety
  int freq = updateFrequency > 0 ? updateFrequency : DEFAULT_UPDATE_FREQUENCY;
  int linesFor24h = 86400 / freq;
  if (linesFor24h > MAX_DATA_LINES) linesFor24h = MAX_DATA_LINES;
  if (linesFor24h < MIN_DATA_LINES) linesFor24h = MIN_DATA_LINES;

  // For large files, seek to approximately the right position
  size_t estimatedBytes = (size_t)linesFor24h * BYTES_PER_LINE_ESTIMATE;
  if (fileSize > estimatedBytes) {
    f.seek(fileSize - estimatedBytes);
    f.readStringUntil('\n'); // Skip partial line
  }

  // Use chunked transfer - send headers first
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/plain", "");

  // Stream data line by line
  int lineCount = 0;
  String line;

  while (f.available() && lineCount < linesFor24h) {
    line = f.readStringUntil('\n');
    if (line.length() > 0) {
      line += "\n";
      server.sendContent(line);
      lineCount++;
      if (lineCount % 10 == 0) yield(); // Feed watchdog every 10 lines
    }
  }

  f.close();
  server.sendContent(""); // Signal end of chunked response
}
