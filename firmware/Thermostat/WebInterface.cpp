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
  server.on("/setthresholds", [this]() { this->handleSetThresholds(); });
  server.on("/setfreq", [this]() { this->handleSetFrequency(); });
  server.on("/setunit", [this]() { this->handleSetUnit(); });
  server.on("/cleardata", [this]() { this->handleClearData(); });
  server.on("/logs", [this]() { this->handleLogs(); });
  server.on("/data", [this]() { this->handleData(); });

  server.begin();
  logger.addLog("Web server started");
}

void WebInterface::handleClient() {
  server.handleClient();
}

void WebInterface::handleRoot() {
  // Use chunked transfer to avoid allocating large String in RAM
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  // Send HTML in chunks directly from flash memory
  server.sendContent_P(PSTR("<!doctype html>\n<html>\n<head>\n"));
  server.sendContent_P(PSTR("<meta charset='utf-8'>\n"));
  server.sendContent_P(PSTR("<meta name='viewport' content='width=device-width, initial-scale=1'>\n"));
  server.sendContent_P(PSTR("<title>Thermostat Control</title>\n"));
  server.sendContent(generateCSS());
  server.sendContent_P(PSTR("</head>\n<body>\n"));
  server.sendContent(generateBodyHTML());
  server.sendContent(generateJavaScript());
  server.sendContent_P(PSTR("</body>\n</html>"));
}

String WebInterface::generateCSS() {
  return R"rawliteral(<style>
  body { font-family: Arial; text-align: center; background: #f5f5f5; }
  h1 { color: #333; }
  .temp { font-size: 28px; font-weight: bold; color: #2196F3; margin: 20px 0; }
  table { margin: 20px auto; border-collapse: collapse; background: #fff; box-shadow: 0 2px 8px rgba(0,0,0,0.1); }
  td, th { padding: 10px 14px; border-bottom: 1px solid #ddd; }
  th { background: #f9f9f9; font-weight: bold; }
  .status-cell { color: white; padding: 8px; font-weight: bold; border-radius: 4px; }
  .on { background: #4CAF50; }
  .off { background: #f44336; }
  button { padding: 6px 12px; margin: 2px; border: none; border-radius: 6px; cursor: pointer; color: white; font-size: 13px; }
  .btn-auto { background: #2196F3; }
  .btn-on { background: #4CAF50; }
  .btn-off { background: #f44336; }
  button:hover { opacity: 0.8; }
  input[type="number"] { width: 60px; padding: 6px; text-align: center; border: 1px solid #ddd; border-radius: 4px; }
  #chart-container { width: 90%; max-width: 800px; height: 300px; margin: 30px auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); }
</style>
)rawliteral";
}

String WebInterface::generateBodyHTML() {
  return R"rawliteral(  <h1>Thermostat Control</h1>
  <div style='margin-bottom:15px;'>
    <a href='/logs' style='color:#2196F3;text-decoration:none;font-size:14px;'>View System Logs</a>
    <span style='margin:0 15px;'>|</span>
    <button id='unitToggle' class='btn-auto' onclick='toggleUnit()' style='padding:4px 10px;font-size:12px;'>°F</button>
  </div>
  <div class='temp'>Temperature: <span id='temp'>--</span><span id='unit'>&deg;C</span></div>
  <table>
    <tr>
      <th>Relay</th>
      <th>State</th>
      <th>Mode</th>
      <th>ON Temp</th>
      <th>OFF Temp</th>
      <th>Actions</th>
    </tr>
    <tr>
      <td><b>1</b></td>
      <td><span class='status-cell' id='status0'>--</span></td>
      <td id='mode0'>--</td>
      <td><input type='number' id='on0' step='0.1'><span class='unit-label'>&deg;C</span></td>
      <td><input type='number' id='off0' step='0.1'><span class='unit-label'>&deg;C</span></td>
      <td>
        <button class='btn-auto' onclick='setMode(0,"AUTO")'>AUTO</button>
        <button class='btn-on' onclick='setMode(0,"ON")'>ON</button>
        <button class='btn-off' onclick='setMode(0,"OFF")'>OFF</button>
        <button class='btn-auto' onclick='saveThreshold(0)'>Save</button>
      </td>
    </tr>
    <tr>
      <td><b>2</b></td>
      <td><span class='status-cell' id='status1'>--</span></td>
      <td id='mode1'>--</td>
      <td><input type='number' id='on1' step='0.1'><span class='unit-label'>&deg;C</span></td>
      <td><input type='number' id='off1' step='0.1'><span class='unit-label'>&deg;C</span></td>
      <td>
        <button class='btn-auto' onclick='setMode(1,"AUTO")'>AUTO</button>
        <button class='btn-on' onclick='setMode(1,"ON")'>ON</button>
        <button class='btn-off' onclick='setMode(1,"OFF")'>OFF</button>
        <button class='btn-auto' onclick='saveThreshold(1)'>Save</button>
      </td>
    </tr>
    <tr>
      <td><b>3</b></td>
      <td><span class='status-cell' id='status2'>--</span></td>
      <td id='mode2'>--</td>
      <td><input type='number' id='on2' step='0.1'><span class='unit-label'>&deg;C</span></td>
      <td><input type='number' id='off2' step='0.1'><span class='unit-label'>&deg;C</span></td>
      <td>
        <button class='btn-auto' onclick='setMode(2,"AUTO")'>AUTO</button>
        <button class='btn-on' onclick='setMode(2,"ON")'>ON</button>
        <button class='btn-off' onclick='setMode(2,"OFF")'>OFF</button>
        <button class='btn-auto' onclick='saveThreshold(2)'>Save</button>
      </td>
    </tr>
    <tr>
      <td><b>4</b></td>
      <td><span class='status-cell' id='status3'>--</span></td>
      <td id='mode3'>--</td>
      <td><input type='number' id='on3' step='0.1'><span class='unit-label'>&deg;C</span></td>
      <td><input type='number' id='off3' step='0.1'><span class='unit-label'>&deg;C</span></td>
      <td>
        <button class='btn-auto' onclick='setMode(3,"AUTO")'>AUTO</button>
        <button class='btn-on' onclick='setMode(3,"ON")'>ON</button>
        <button class='btn-off' onclick='setMode(3,"OFF")'>OFF</button>
        <button class='btn-auto' onclick='saveThreshold(3)'>Save</button>
      </td>
    </tr>
  </table>

  <div style='margin: 20px auto; max-width: 600px;'>
    <p>
      <label style='font-weight: bold;'>Update Frequency:</label>
      <input id='freq' type='number' min='5' max='300' value='5' style='width: 80px; padding: 6px; margin: 0 10px;'> seconds
      <button class='btn-auto' onclick='setFrequency()'>Set Frequency</button>
    </p>
    <p>
      <button class='btn-off' onclick='clearData()' style='padding: 8px 16px;'>Clear Temperature Data</button>
      <span id='clearStatus' style='margin-left: 10px; font-size: 12px;'></span>
    </p>
  </div>

  <div id='chart-container'>
    <canvas id='tempChart'></canvas>
  </div>
)rawliteral";
}

String WebInterface::generateJavaScript() {
  return R"rawliteral(
<script src='https://cdn.jsdelivr.net/npm/chart.js@3.9.1/dist/chart.min.js'></script>
<script>
// Track which inputs have been edited - persist until Save is clicked
const editedInputs = new Set();
let useFahrenheit = false;

// Temperature conversion functions
function celsiusToFahrenheit(c) {
  return (c * 9 / 5) + 32;
}

function fahrenheitToCelsius(f) {
  return (f - 32) * 5 / 9;
}

async function toggleUnit() {
  try {
    useFahrenheit = !useFahrenheit;
    const res = await fetch('/setunit?fahrenheit=' + (useFahrenheit ? '1' : '0'));
    const data = await res.json();
    updateUnitLabels();
    updateFromData(data);
  } catch (error) {
    console.error('Error toggling unit:', error);
    await updateStatus();
  }
}

function updateUnitLabels() {
  const unit = useFahrenheit ? '°F' : '°C';
  document.getElementById('unit').innerHTML = unit;
  document.getElementById('unitToggle').innerHTML = useFahrenheit ? '°C' : '°F';
  document.querySelectorAll('.unit-label').forEach(el => el.innerHTML = unit);
}

// Add input event listeners to all threshold inputs
window.addEventListener('DOMContentLoaded', () => {
  for (let i = 0; i < 4; i++) {
    document.getElementById('on' + i).addEventListener('input', (e) => {
      editedInputs.add(e.target.id);
    });
    document.getElementById('off' + i).addEventListener('input', (e) => {
      editedInputs.add(e.target.id);
    });
  }
});

async function setMode(relay, mode) {
  try {
    const res = await fetch('/setmode?relay=' + relay + '&mode=' + mode);
    const data = await res.json();
    // Use the response from setmode directly to update UI
    updateFromData(data);
  } catch (error) {
    console.error('Error setting mode:', error);
    // Fallback to fetching status
    await updateStatus();
  }
}

async function saveThreshold(relay) {
  try {
    let on = parseFloat(document.getElementById('on' + relay).value);
    let off = parseFloat(document.getElementById('off' + relay).value);

    // Convert to Celsius if displaying in Fahrenheit
    if (useFahrenheit) {
      on = fahrenheitToCelsius(on);
      off = fahrenheitToCelsius(off);
    }

    const res = await fetch('/setthresholds?relay=' + relay + '&on=' + on + '&off=' + off);
    const data = await res.json();
    // Clear edited flags for this relay's inputs after saving
    editedInputs.delete('on' + relay);
    editedInputs.delete('off' + relay);
    // Use the response from setthresholds directly to update UI
    updateFromData(data);
  } catch (error) {
    console.error('Error saving threshold:', error);
    await updateStatus();
  }
}

async function setFrequency() {
  try {
    const freq = parseInt(document.getElementById('freq').value);
    if (isNaN(freq) || freq < 5) {
      alert('Minimum frequency is 5 seconds');
      return;
    }
    if (freq > 300) {
      alert('Maximum frequency is 300 seconds');
      return;
    }
    const res = await fetch('/setfreq?sec=' + freq);
    const data = await res.json();
    updateFromData(data);
  } catch (error) {
    console.error('Error setting frequency:', error);
    await updateStatus();
  }
}

async function clearData() {
  if (!confirm('Are you sure you want to clear all temperature data? This cannot be undone.')) {
    return;
  }
  const status = document.getElementById('clearStatus');
  status.innerText = 'Clearing...';
  status.style.color = '#ff9800';

  try {
    const res = await fetch('/cleardata');
    const data = await res.json();
    status.innerText = data.message || 'Data cleared!';
    status.style.color = '#4CAF50';
    setTimeout(() => { status.innerText = ''; updateChart(); }, 3000);
  } catch (error) {
    status.innerText = 'Error clearing data';
    status.style.color = '#f44336';
  }
}

// Extract UI update logic so it can be reused
function updateFromData(data) {
  // Update unit preference if it changed on server
  if (data.useFahrenheit !== undefined && data.useFahrenheit !== useFahrenheit) {
    useFahrenheit = data.useFahrenheit;
    updateUnitLabels();
  }

  // Display current temperature in selected unit
  const displayTemp = useFahrenheit ? celsiusToFahrenheit(data.temp) : data.temp;
  document.getElementById('temp').innerText = displayTemp.toFixed(1);

  // Update frequency input if present in data
  if (data.freq) {
    const freqInput = document.getElementById('freq');
    if (document.activeElement !== freqInput) {
      freqInput.value = data.freq;
    }
  }

  for (let i = 0; i < 4; i++) {
    const status = document.getElementById('status' + i);
    status.innerText = data.relays[i].state ? 'ON' : 'OFF';
    status.className = 'status-cell ' + (data.relays[i].state ? 'on' : 'off');

    document.getElementById('mode' + i).innerText = data.relays[i].mode;

    // Only update input fields if not focused AND not edited
    const onInput = document.getElementById('on' + i);
    const offInput = document.getElementById('off' + i);

    // Convert thresholds for display
    const displayTempOn = useFahrenheit ? celsiusToFahrenheit(data.relays[i].tempOn) : data.relays[i].tempOn;
    const displayTempOff = useFahrenheit ? celsiusToFahrenheit(data.relays[i].tempOff) : data.relays[i].tempOff;

    if (document.activeElement !== onInput && !editedInputs.has('on' + i)) {
      onInput.value = displayTempOn.toFixed(1);
    }
    if (document.activeElement !== offInput && !editedInputs.has('off' + i)) {
      offInput.value = displayTempOff.toFixed(1);
    }
  }
}

async function updateStatus() {
  try {
    const res = await fetch('/status');
    const data = await res.json();
    updateFromData(data);
  } catch (error) {
    console.error('Error updating status:', error);
  }
}

// Initialize Chart.js
const ctx = document.getElementById('tempChart').getContext('2d');
const tempChart = new Chart(ctx, {
  type: 'line',
  data: {
    labels: [],
    datasets: [{
      label: 'Temperature (°C)',
      data: [],
      borderColor: '#2196F3',
      backgroundColor: 'rgba(33, 150, 243, 0.1)',
      borderWidth: 2,
      tension: 0.4,
      pointRadius: 1,
      pointHoverRadius: 4
    }]
  },
  options: {
    responsive: true,
    maintainAspectRatio: true,
    interaction: {
      intersect: false,
      mode: 'index'
    },
    scales: {
      y: {
        beginAtZero: false,
        title: {
          display: true,
          text: 'Temperature (°C)'
        }
      },
      x: {
        title: {
          display: true,
          text: 'Time'
        },
        ticks: {
          maxRotation: 45,
          minRotation: 45,
          autoSkip: true,
          maxTicksLimit: 12
        }
      }
    },
    plugins: {
      legend: {
        display: true
      }
    },
    animation: false
  }
});

async function updateChart() {
  try {
    const res = await fetch('/data');
    const text = await res.text();
    const lines = text.trim().split('\n');

    const temps = [];
    const times = [];

    for (const line of lines) {
      if (line.startsWith('#') || line.length === 0) continue;
      const parts = line.split(',');
      if (parts.length >= 2) {
        const timestamp = parseInt(parts[0]);
        let temp = parseFloat(parts[1]);

        if (!isNaN(timestamp) && !isNaN(temp)) {
          // Convert temperature to display unit
          if (useFahrenheit) {
            temp = celsiusToFahrenheit(temp);
          }

          // Check if timestamp is Unix time or millis since boot
          let date;
          if (timestamp > 1000000000) {
            // Unix timestamp
            date = new Date(timestamp * 1000);
          } else {
            // Millis since boot - use relative time
            const seconds = timestamp;
            const hours = Math.floor(seconds / 3600);
            const mins = Math.floor((seconds % 3600) / 60);
            date = hours + 'h' + mins + 'm';
          }

          temps.push(temp);
          if (typeof date === 'string') {
            times.push(date);
          } else {
            times.push(date.toLocaleTimeString());
          }
        }
      }
    }

    // Update chart title to show current unit
    tempChart.options.scales.y.title.text = 'Temperature (°' + (useFahrenheit ? 'F' : 'C') + ')';
    tempChart.data.datasets[0].label = 'Temperature (°' + (useFahrenheit ? 'F' : 'C') + ')';
    tempChart.data.labels = times;
    tempChart.data.datasets[0].data = temps;
    tempChart.update('none'); // Update without animation for better performance
  } catch (error) {
    console.error('Chart update error:', error);
  }
}

updateStatus();
setInterval(updateStatus, 2000);
updateChart();
setInterval(updateChart, 30000); // Update chart every 30 seconds
</script>
)rawliteral";
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
  String html = "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta http-equiv='refresh' content='5'><title>System Logs</title>";
  html += "<style>body{font-family:monospace;background:#1e1e1e;color:#d4d4d4;padding:20px;}";
  html += ".log{background:#252526;padding:8px;margin:3px 0;border-radius:3px;font-size:13px;}";
  html += "h2{color:#4CAF50;}</style></head><body>";
  html += "<h2>System Logs (Last " + String(logger.getLogCount()) + ")</h2>";
  html += "<a href='/' style='color:#2196F3;'>Back to Main</a><br><br>";

  const auto& logs = logger.getLogs();
  for (int i = logs.size() - 1; i >= 0; i--) {
    html += "<div class='log'>[" + String(logs[i].timestamp / 1000) + "s] " + logs[i].message + "</div>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
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

  logger.addLog("Streamed " + String(lineCount) + " lines (~" +
                String(lineCount * updateFrequency / 3600) + "h)");
}
