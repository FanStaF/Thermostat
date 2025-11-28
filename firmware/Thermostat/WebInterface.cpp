#include "WebInterface.h"

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
  server.on("/", [this]() { this->handleRoot(); });
  server.on("/status", [this]() { this->handleStatus(); });
  server.on("/setmode", [this]() { this->handleSetMode(); });
  server.on("/settype", [this]() { this->handleSetType(); });
  server.on("/setthresholds", [this]() { this->handleSetThresholds(); });
  server.on("/setfreq", [this]() { this->handleSetFrequency(); });
  server.on("/setunit", [this]() { this->handleSetUnit(); });
  server.on("/cleardata", [this]() { this->handleClearData(); });
  server.on("/logs", [this]() { this->handleLogs(); });
  server.on("/logs.json", [this]() { this->handleLogsJson(); });
  server.on("/data", [this]() { this->handleData(); });

  server.begin();
  logger.addLog("Web server started");
}

void WebInterface::handleClient() {
  server.handleClient();
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
<button id='unitToggle' class='btn-auto' onclick='toggleUnit()' style='padding:4px 10px;font-size:12px;'>°F</button>
</div>
<div class='temp'>Temperature: <span id='temp'>--</span><span id='unit'>&deg;C</span></div>
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
function c2f(c){return(c*9/5)+32}
function f2c(f){return(f-32)*5/9}
async function toggleUnit(){try{useFahrenheit=!useFahrenheit;const r=await fetch('/setunit?fahrenheit='+(useFahrenheit?'1':'0'));updateUnitLabels();updateFromData(await r.json())}catch(e){await updateStatus()}}
function updateUnitLabels(){const u=useFahrenheit?'°F':'°C';document.getElementById('unit').innerHTML=u;document.getElementById('unitToggle').innerHTML=useFahrenheit?'°C':'°F';document.querySelectorAll('.unit-label').forEach(el=>el.innerHTML=u)}
window.addEventListener('DOMContentLoaded',()=>{for(let i=0;i<4;i++){document.getElementById('on'+i).addEventListener('input',e=>editedInputs.add(e.target.id));document.getElementById('off'+i).addEventListener('input',e=>editedInputs.add(e.target.id))}});
async function setMode(relay,mode){try{const r=await fetch('/setmode?relay='+relay+'&mode='+mode);updateFromData(await r.json())}catch(e){await updateStatus()}}
async function setType(relay){try{const t=document.getElementById('type'+relay).value;const r=await fetch('/settype?relay='+relay+'&type='+t);updateFromData(await r.json())}catch(e){await updateStatus()}}
async function saveThreshold(relay){try{let on=parseFloat(document.getElementById('on'+relay).value);let off=parseFloat(document.getElementById('off'+relay).value);if(useFahrenheit){on=f2c(on);off=f2c(off)}const r=await fetch('/setthresholds?relay='+relay+'&on='+on+'&off='+off);editedInputs.delete('on'+relay);editedInputs.delete('off'+relay);updateFromData(await r.json())}catch(e){await updateStatus()}}
async function setFrequency(){try{const f=parseInt(document.getElementById('freq').value);if(isNaN(f)||f<5){alert('Min 5s');return}if(f>300){alert('Max 300s');return}const r=await fetch('/setfreq?sec='+f);updateFromData(await r.json())}catch(e){await updateStatus()}}
async function clearData(){if(!confirm('Clear all data?'))return;const s=document.getElementById('clearStatus');s.innerText='Clearing...';try{const r=await fetch('/cleardata');const d=await r.json();s.innerText=d.message||'Done';setTimeout(()=>{s.innerText='';updateChart()},3000)}catch(e){s.innerText='Error'}}
function updateFromData(d){if(d.useFahrenheit!==undefined&&d.useFahrenheit!==useFahrenheit){useFahrenheit=d.useFahrenheit;updateUnitLabels()}const dt=useFahrenheit?c2f(d.temp):d.temp;document.getElementById('temp').innerText=dt.toFixed(1);if(d.freq){const fi=document.getElementById('freq');if(document.activeElement!==fi)fi.value=d.freq}for(let i=0;i<4;i++){const st=document.getElementById('status'+i);st.innerText=d.relays[i].state?'ON':'OFF';st.className='status-cell '+(d.relays[i].state?'on':'off');document.getElementById('mode'+i).innerText=d.relays[i].mode;const ts=document.getElementById('type'+i);if(document.activeElement!==ts&&d.relays[i].type)ts.value=d.relays[i].type;const oi=document.getElementById('on'+i),fi=document.getElementById('off'+i);const don=useFahrenheit?c2f(d.relays[i].tempOn):d.relays[i].tempOn;const doff=useFahrenheit?c2f(d.relays[i].tempOff):d.relays[i].tempOff;if(document.activeElement!==oi&&!editedInputs.has('on'+i))oi.value=don.toFixed(1);if(document.activeElement!==fi&&!editedInputs.has('off'+i))fi.value=doff.toFixed(1)}}
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
  String json = "{\"temp\":" + String(tempManager.getCurrentTemp(), 1);
  json += ",\"freq\":" + String(updateFrequency);
  json += ",\"useFahrenheit\":" + String(useFahrenheit ? "true" : "false");
  json += ",\"relays\":[";
  for (int i = 0; i < 4; i++) {
    if (i > 0) json += ",";
    json += "{\"state\":" + String(relayController.getRelayState(i) ? "true" : "false");
    json += ",\"mode\":\"" + RelayController::modeToString(relayController.getRelayMode(i)) + "\"";
    json += ",\"type\":\"" + RelayController::typeToString(relayController.getRelayType(i)) + "\"";
    json += ",\"tempOn\":" + String(relayController.getTempOn(i), 1);
    json += ",\"tempOff\":" + String(relayController.getTempOff(i), 1) + "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void WebInterface::handleSetMode() {
  if (server.hasArg("relay") && server.hasArg("mode")) {
    int relay = server.arg("relay").toInt();
    String mode = server.arg("mode");
    if (relay >= 0 && relay < 4) {
      if (mode == "AUTO") relayController.setRelayMode(relay, AUTO);
      else if (mode == "ON") relayController.setRelayMode(relay, MANUAL_ON);
      else if (mode == "OFF") relayController.setRelayMode(relay, MANUAL_OFF);
      logger.addLog("Relay " + String(relay + 1) + " mode: " + mode);
      relayController.applyRelayLogic(tempManager.getCurrentTemp());
      configManager.saveSettings(updateFrequency, useFahrenheit);

      // Send updated relay state to API
      if (apiClient.isRegistered() && WiFi.status() == WL_CONNECTED) {
        apiClient.sendRelayState(
          relay + 1,
          relayController.getRelayState(relay),
          RelayController::modeToString(relayController.getRelayMode(relay)),
          relayController.getTempOn(relay),
          relayController.getTempOff(relay),
          "Relay " + String(relay + 1)
        );
      }
    }
  }
  handleStatus();
}

void WebInterface::handleSetType() {
  if (server.hasArg("relay") && server.hasArg("type")) {
    int relay = server.arg("relay").toInt();
    String type = server.arg("type");
    if (relay >= 0 && relay < 4) {
      if (type == "HEATING") relayController.setRelayType(relay, HEATING);
      else if (type == "COOLING") relayController.setRelayType(relay, COOLING);
      else if (type == "GENERIC") relayController.setRelayType(relay, GENERIC);
      else if (type == "MANUAL_ONLY") relayController.setRelayType(relay, MANUAL_ONLY);
      relayController.applyRelayLogic(tempManager.getCurrentTemp());
      configManager.saveSettings(updateFrequency, useFahrenheit);

      // Send updated relay state to API
      if (apiClient.isRegistered() && WiFi.status() == WL_CONNECTED) {
        apiClient.sendRelayState(
          relay + 1,
          relayController.getRelayState(relay),
          RelayController::modeToString(relayController.getRelayMode(relay)),
          relayController.getTempOn(relay),
          relayController.getTempOff(relay),
          "Relay " + String(relay + 1)
        );
      }
    }
  }
  handleStatus();
}

void WebInterface::handleSetThresholds() {
  if (server.hasArg("relay") && server.hasArg("on") && server.hasArg("off")) {
    int relay = server.arg("relay").toInt();
    if (relay >= 0 && relay < 4) {
      float tempOn = server.arg("on").toFloat();
      float tempOff = server.arg("off").toFloat();
      relayController.setTempThresholds(relay, tempOn, tempOff);
      logger.addLog("Relay " + String(relay + 1) + " thresholds: ON=" +
                    String(tempOn, 1) + "C, OFF=" + String(tempOff, 1) + "C");
      relayController.applyRelayLogic(tempManager.getCurrentTemp());
      configManager.saveSettings(updateFrequency, useFahrenheit);

      // Send updated relay state to API
      if (apiClient.isRegistered() && WiFi.status() == WL_CONNECTED) {
        apiClient.sendRelayState(
          relay + 1,
          relayController.getRelayState(relay),
          RelayController::modeToString(relayController.getRelayMode(relay)),
          relayController.getTempOn(relay),
          relayController.getTempOff(relay),
          "Relay " + String(relay + 1)
        );
      }
    }
  }
  handleStatus();
}

void WebInterface::handleSetFrequency() {
  if (server.hasArg("sec")) {
    int sec = server.arg("sec").toInt();
    if (sec < 5) sec = 5;
    if (sec > 300) sec = 300;
    updateFrequency = sec;
    logger.addLog("Update frequency set to " + String(sec) + "s");
    configManager.saveSettings(updateFrequency, useFahrenheit);
  }
  handleStatus();
}

void WebInterface::handleClearData() {
  if (LittleFS.exists(LOG_FILE)) {
    LittleFS.remove(LOG_FILE);
    logger.addLog("Temperature data cleared by user");
    server.send(200, "application/json", "{\"message\":\"Data cleared successfully\"}");
  } else {
    server.send(200, "application/json", "{\"message\":\"No data to clear\"}");
  }
}

void WebInterface::handleSetUnit() {
  if (server.hasArg("fahrenheit")) {
    useFahrenheit = server.arg("fahrenheit") == "1";
    logger.addLog("Unit changed to " + String(useFahrenheit ? "Fahrenheit" : "Celsius"));
    configManager.saveSettings(updateFrequency, useFahrenheit);
  }
  server.send(200, "application/json", "{\"useFahrenheit\":" + String(useFahrenheit ? "true" : "false") + "}");
}

void WebInterface::handleLogs() {
  // Check for filter and page parameters
  bool filterRepetitive = !server.hasArg("all");
  int page = server.hasArg("page") ? server.arg("page").toInt() : 1;
  if (page < 1) page = 1;
  const int logsPerPage = 100;

  String html = "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta http-equiv='refresh' content='10'><title>System Logs</title>";
  html += "<style>body{font-family:monospace;background:#1e1e1e;color:#d4d4d4;padding:20px;}";
  html += ".log{background:#252526;padding:8px;margin:3px 0;border-radius:3px;font-size:13px;}";
  html += ".log.relay{border-left:3px solid #4CAF50;}";
  html += ".log.error{border-left:3px solid #f44336;}";
  html += ".log.cmd{border-left:3px solid #2196F3;}";
  html += "h2{color:#4CAF50;}";
  html += ".nav{margin:10px 0;}.nav a{color:#2196F3;margin-right:15px;}";
  html += ".filter{margin:10px 0;padding:10px;background:#252526;border-radius:5px;}";
  html += "</style></head><body>";
  html += "<h2>System Logs (" + String(logger.getLogCount()) + " total)</h2>";
  html += "<a href='/' style='color:#2196F3;'>Back to Main</a>";

  // Filter toggle
  html += "<div class='filter'>";
  if (filterRepetitive) {
    html += "Showing filtered logs (hiding repetitive heartbeats/posts) - ";
    html += "<a href='/logs?all=1'>Show All</a>";
  } else {
    html += "Showing all logs - ";
    html += "<a href='/logs'>Filter Repetitive</a>";
  }
  html += "</div>";

  const auto& logs = logger.getLogs();

  // Build filtered list
  std::vector<int> filteredIndices;
  int lastHeartbeat = -1;
  int lastTempPost = -1;
  int lastCommandPoll = -1;

  for (int i = logs.size() - 1; i >= 0; i--) {
    const String& msg = logs[i].message;
    bool isRepetitive = false;

    if (filterRepetitive) {
      // Check for repetitive messages - only show the most recent of each type
      if (msg.indexOf("Heartbeat") >= 0 || msg.indexOf("heartbeat") >= 0 || msg.indexOf("/heartbeat") >= 0) {
        if (lastHeartbeat >= 0) isRepetitive = true;
        else lastHeartbeat = i;
      } else if (msg.indexOf("Posted temp") >= 0 || msg.indexOf("Temperature posted") >= 0 || msg.indexOf("/temperature :") >= 0) {
        if (lastTempPost >= 0) isRepetitive = true;
        else lastTempPost = i;
      } else if (msg.indexOf("No pending commands") >= 0 || msg.indexOf("Polling commands") >= 0 || msg.indexOf("/commands/pending") >= 0) {
        if (lastCommandPoll >= 0) isRepetitive = true;
        else lastCommandPoll = i;
      }
    }

    if (!isRepetitive) {
      filteredIndices.push_back(i);
    }
  }

  // Pagination
  int totalFiltered = filteredIndices.size();
  int totalPages = (totalFiltered + logsPerPage - 1) / logsPerPage;
  if (page > totalPages) page = totalPages;
  int startIdx = (page - 1) * logsPerPage;
  int endIdx = min(startIdx + logsPerPage, totalFiltered);

  // Page navigation
  html += "<div class='nav'>";
  if (page > 1) {
    html += "<a href='/logs?page=" + String(page - 1) + (filterRepetitive ? "" : "&all=1") + "'>&lt; Prev</a>";
  }
  html += "Page " + String(page) + " of " + String(totalPages) + " (" + String(totalFiltered) + " entries)";
  if (page < totalPages) {
    html += "<a href='/logs?page=" + String(page + 1) + (filterRepetitive ? "" : "&all=1") + "'>Next &gt;</a>";
  }
  html += "</div>";

  // Display logs
  for (int i = startIdx; i < endIdx; i++) {
    int logIdx = filteredIndices[i];
    const String& msg = logs[logIdx].message;

    // Color-code log types
    String logClass = "log";
    if (msg.indexOf("Relay") >= 0) logClass += " relay";
    else if (msg.indexOf("ERROR") >= 0 || msg.indexOf("Error") >= 0 || msg.indexOf("failed") >= 0) logClass += " error";
    else if (msg.indexOf("command") >= 0 || msg.indexOf("Command") >= 0) logClass += " cmd";

    html += "<div class='" + logClass + "'>[" + String(logs[logIdx].timestamp / 1000) + "s] " + msg + "</div>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void WebInterface::handleLogsJson() {
  // Return logs as JSON for remote access
  int limit = server.hasArg("limit") ? server.arg("limit").toInt() : 100;
  if (limit < 1) limit = 1;
  if (limit > 500) limit = 500;

  bool filterRepetitive = !server.hasArg("all");

  const auto& logs = logger.getLogs();

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("{\"total\":");
  server.sendContent(String(logs.size()));
  server.sendContent(",\"logs\":[");

  int lastHeartbeat = -1;
  int lastTempPost = -1;
  int lastCommandPoll = -1;
  int count = 0;
  bool first = true;

  for (int i = logs.size() - 1; i >= 0 && count < limit; i--) {
    const String& msg = logs[i].message;
    bool isRepetitive = false;

    if (filterRepetitive) {
      if (msg.indexOf("Heartbeat") >= 0 || msg.indexOf("heartbeat") >= 0 || msg.indexOf("/heartbeat") >= 0) {
        if (lastHeartbeat >= 0) isRepetitive = true;
        else lastHeartbeat = i;
      } else if (msg.indexOf("Posted temp") >= 0 || msg.indexOf("Temperature posted") >= 0 || msg.indexOf("/temperature :") >= 0) {
        if (lastTempPost >= 0) isRepetitive = true;
        else lastTempPost = i;
      } else if (msg.indexOf("No pending commands") >= 0 || msg.indexOf("Polling commands") >= 0 || msg.indexOf("/commands/pending") >= 0) {
        if (lastCommandPoll >= 0) isRepetitive = true;
        else lastCommandPoll = i;
      }
    }

    if (!isRepetitive) {
      if (!first) server.sendContent(",");
      first = false;

      // Escape JSON string
      String escaped = logs[i].message;
      escaped.replace("\\", "\\\\");
      escaped.replace("\"", "\\\"");

      server.sendContent("{\"ts\":");
      server.sendContent(String(logs[i].timestamp / 1000));
      server.sendContent(",\"msg\":\"");
      server.sendContent(escaped);
      server.sendContent("\"}");
      count++;
    }
  }

  server.sendContent("]}");
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
  int linesFor24h = (86400 / updateFrequency);
  if (linesFor24h > MAX_DATA_LINES) linesFor24h = MAX_DATA_LINES;
  if (linesFor24h < MIN_DATA_LINES) linesFor24h = MIN_DATA_LINES;

  // For large files, seek to approximately the right position
  long estimatedBytes = linesFor24h * BYTES_PER_LINE_ESTIMATE;
  if (fileSize > estimatedBytes) {
    f.seek(fileSize - estimatedBytes);
    f.readStringUntil('\n'); // Skip partial line
  }

  // Use chunked transfer - send headers first
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/plain", "");

  // Stream data line by line
  int lineCount = 0;
  String line = "";

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
