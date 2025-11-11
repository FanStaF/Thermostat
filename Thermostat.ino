#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>
#include <time.h>

// ---- WiFi Settings ----
const char* ssid = "LOTR-TM";
const char* password = "Sverige1976$";

// ---- DS18B20 Setup ----
#define ONE_WIRE_BUS D2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ---- Relay Setup ----
const int relayPins[4] = { D1, D5, D6, D7 };
bool relayStates[4] = { false, false, false, false };

// ---- Control Mode ----
enum Mode { AUTO = 0, MANUAL_ON = 1, MANUAL_OFF = 2 };
Mode relayModes[4] = { MANUAL_OFF, MANUAL_OFF, MANUAL_OFF, MANUAL_OFF };

// ---- Temperature ----
float currentTemp = 0.0;
float tempOn[4] = { 25.0, 25.0, 25.0, 25.0 };
float tempOff[4] = { 23.0, 23.0, 23.0, 23.0 };
unsigned long lastTempUpdate = 0;
int updateFrequency = 5; // seconds
bool useFahrenheit = false; // Temperature display unit preference

// ---- Web Server ----
ESP8266WebServer server(80);

// ---- Config File ----
const char *CONFIG_FILE = "/config.json";
const char *LOG_FILE = "/temp_log.csv";

// ---- System Logs ----
struct LogEntry {
  String message;
  unsigned long timestamp;
};
std::vector<LogEntry> systemLogs;
const size_t MAX_LOGS = 100;

void addLog(String message) {
  LogEntry log;
  log.message = message;
  log.timestamp = millis();
  systemLogs.push_back(log);
  if (systemLogs.size() > MAX_LOGS) {
    systemLogs.erase(systemLogs.begin());
  }
  Serial.println(message);
}

// Temperature conversion helpers
float celsiusToFahrenheit(float c) {
  return (c * 9.0 / 5.0) + 32.0;
}

float fahrenheitToCelsius(float f) {
  return (f - 32.0) * 5.0 / 9.0;
}

// Format temperature for display based on current unit
String formatTemp(float tempC, int decimals = 1) {
  if (useFahrenheit) {
    return String(celsiusToFahrenheit(tempC), decimals);
  }
  return String(tempC, decimals);
}

String getTempUnit() {
  return useFahrenheit ? "F" : "C";
}

// ---- Config Persistence ----
void saveSettings() {
  addLog("Saving settings...");
  StaticJsonDocument<1024> doc;
  JsonArray modes = doc.createNestedArray("modes");
  for (int i = 0; i < 4; i++) modes.add((int)relayModes[i]);
  JsonArray onArr = doc.createNestedArray("tempOn");
  for (int i = 0; i < 4; i++) onArr.add(tempOn[i]);
  JsonArray offArr = doc.createNestedArray("tempOff");
  for (int i = 0; i < 4; i++) offArr.add(tempOff[i]);
  doc["updateFrequency"] = updateFrequency;
  doc["useFahrenheit"] = useFahrenheit;

  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) {
    addLog("ERROR: Failed to open config file for writing");
    return;
  }

  size_t bytesWritten = serializeJson(doc, f);
  f.close();

  addLog("Settings saved (" + String(bytesWritten) + " bytes)");

  // Print what was saved for debugging
  String jsonStr;
  serializeJson(doc, jsonStr);
  addLog("JSON: " + jsonStr);
}

void loadSettings() {
  addLog("Loading settings...");
  if (!LittleFS.exists(CONFIG_FILE)) {
    addLog("Config file not found, using defaults");
    return;
  }
  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) {
    addLog("ERROR: Failed to open config file");
    return;
  }

  size_t fileSize = f.size();
  addLog("Config file size: " + String(fileSize) + " bytes");

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, f);
  f.close();

  if (error) {
    addLog("ERROR: Failed to parse config: " + String(error.c_str()));
    return;
  }

  // Print loaded JSON for debugging
  String jsonStr;
  serializeJson(doc, jsonStr);
  addLog("Loaded JSON: " + jsonStr);

  if (doc.containsKey("modes")) {
    JsonArray modes = doc["modes"];
    int i = 0;
    for (int m : modes) {
      if (i < 4) relayModes[i++] = (Mode)m;
    }
  }
  if (doc.containsKey("tempOn")) {
    JsonArray a = doc["tempOn"];
    int i = 0;
    for (float v : a) {
      if (i < 4) tempOn[i++] = v;
    }
  }
  if (doc.containsKey("tempOff")) {
    JsonArray a = doc["tempOff"];
    int i = 0;
    for (float v : a) {
      if (i < 4) tempOff[i++] = v;
    }
  }
  if (doc.containsKey("updateFrequency")) {
    updateFrequency = doc["updateFrequency"];
  }
  if (doc.containsKey("useFahrenheit")) {
    useFahrenheit = doc["useFahrenheit"];
  }
  addLog("Settings loaded successfully");
}

String modeToString(Mode m) {
  switch (m) {
    case AUTO: return "AUTO";
    case MANUAL_ON: return "ON";
    case MANUAL_OFF: return "OFF";
  }
  return "?";
}

void applyRelayLogic() {
  for (int i = 0; i < 4; i++) {
    bool prevState = relayStates[i];

    switch (relayModes[i]) {
      case MANUAL_ON:
        relayStates[i] = true;
        break;
      case MANUAL_OFF:
        relayStates[i] = false;
        break;
      case AUTO:
        if (currentTemp >= tempOn[i]) relayStates[i] = true;
        else if (currentTemp <= tempOff[i]) relayStates[i] = false;
        // Else maintain current state (hysteresis)
        break;
    }

    digitalWrite(relayPins[i], relayStates[i] ? LOW : HIGH); // Active-LOW

    if (prevState != relayStates[i]) {
      addLog("Relay " + String(i+1) + " -> " + String(relayStates[i] ? "ON" : "OFF") + " @ " + String(currentTemp, 1) + "C");
    }
  }
}

void handleRoot() {
  String html = R"rawliteral(
<!doctype html>
<html>
<head>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Thermostat Control</title>
<style>
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
</head>
<body>
  <h1>Thermostat Control</h1>
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
  useFahrenheit = !useFahrenheit;
  await fetch('/setunit?fahrenheit=' + (useFahrenheit ? '1' : '0'));
  updateUnitLabels();
  updateStatus();
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
  await fetch('/setmode?relay=' + relay + '&mode=' + mode);
  updateStatus();
}

async function saveThreshold(relay) {
  let on = parseFloat(document.getElementById('on' + relay).value);
  let off = parseFloat(document.getElementById('off' + relay).value);

  // Convert to Celsius if displaying in Fahrenheit
  if (useFahrenheit) {
    on = fahrenheitToCelsius(on);
    off = fahrenheitToCelsius(off);
  }

  await fetch('/setthresholds?relay=' + relay + '&on=' + on + '&off=' + off);
  // Clear edited flags for this relay's inputs after saving
  editedInputs.delete('on' + relay);
  editedInputs.delete('off' + relay);
  updateStatus();
}

async function setFrequency() {
  const freq = parseInt(document.getElementById('freq').value);
  if (isNaN(freq) || freq < 5) {
    alert('Minimum frequency is 5 seconds');
    return;
  }
  if (freq > 300) {
    alert('Maximum frequency is 300 seconds');
    return;
  }
  await fetch('/setfreq?sec=' + freq);
  updateStatus();
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

async function updateStatus() {
  const res = await fetch('/status');
  const data = await res.json();

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
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

void handleStatus() {
  String json = "{\"temp\":" + String(currentTemp, 1);
  json += ",\"freq\":" + String(updateFrequency);
  json += ",\"useFahrenheit\":" + String(useFahrenheit ? "true" : "false");
  json += ",\"relays\":[";
  for (int i = 0; i < 4; i++) {
    if (i > 0) json += ",";
    json += "{\"state\":" + String(relayStates[i] ? "true" : "false");
    json += ",\"mode\":\"" + modeToString(relayModes[i]) + "\"";
    json += ",\"tempOn\":" + String(tempOn[i], 1);
    json += ",\"tempOff\":" + String(tempOff[i], 1) + "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleSetMode() {
  if (server.hasArg("relay") && server.hasArg("mode")) {
    int relay = server.arg("relay").toInt();
    String mode = server.arg("mode");
    if (relay >= 0 && relay < 4) {
      if (mode == "AUTO") relayModes[relay] = AUTO;
      else if (mode == "ON") relayModes[relay] = MANUAL_ON;
      else if (mode == "OFF") relayModes[relay] = MANUAL_OFF;
      addLog("Relay " + String(relay + 1) + " mode: " + mode);
      applyRelayLogic();
      saveSettings();
    }
  }
  handleStatus();
}

void handleSetThresholds() {
  if (server.hasArg("relay") && server.hasArg("on") && server.hasArg("off")) {
    int relay = server.arg("relay").toInt();
    if (relay >= 0 && relay < 4) {
      tempOn[relay] = server.arg("on").toFloat();
      tempOff[relay] = server.arg("off").toFloat();
      addLog("Relay " + String(relay + 1) + " thresholds: ON=" + String(tempOn[relay], 1) + "C, OFF=" + String(tempOff[relay], 1) + "C");
      applyRelayLogic();
      saveSettings();
    }
  }
  handleStatus();
}

void handleSetFrequency() {
  if (server.hasArg("sec")) {
    int sec = server.arg("sec").toInt();
    if (sec < 5) sec = 5;
    if (sec > 300) sec = 300;
    updateFrequency = sec;
    addLog("Update frequency set to " + String(sec) + "s");
    saveSettings();
  }
  server.send(200, "application/json", "{\"freq\":" + String(updateFrequency) + "}");
}

void handleClearData() {
  if (LittleFS.exists(LOG_FILE)) {
    LittleFS.remove(LOG_FILE);
    addLog("Temperature data cleared by user");
    server.send(200, "application/json", "{\"message\":\"Data cleared successfully\"}");
  } else {
    server.send(200, "application/json", "{\"message\":\"No data to clear\"}");
  }
}

void handleSetUnit() {
  if (server.hasArg("fahrenheit")) {
    useFahrenheit = server.arg("fahrenheit") == "1";
    addLog("Unit changed to " + String(useFahrenheit ? "Fahrenheit" : "Celsius"));
    saveSettings();
  }
  server.send(200, "application/json", "{\"useFahrenheit\":" + String(useFahrenheit ? "true" : "false") + "}");
}

void handleLogs() {
  String html = "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta http-equiv='refresh' content='5'><title>System Logs</title>";
  html += "<style>body{font-family:monospace;background:#1e1e1e;color:#d4d4d4;padding:20px;}";
  html += ".log{background:#252526;padding:8px;margin:3px 0;border-radius:3px;font-size:13px;}";
  html += "h2{color:#4CAF50;}</style></head><body>";
  html += "<h2>System Logs (Last " + String(systemLogs.size()) + ")</h2>";
  html += "<a href='/' style='color:#2196F3;'>Back to Main</a><br><br>";

  for (int i = systemLogs.size() - 1; i >= 0; i--) {
    html += "<div class='log'>[" + String(systemLogs[i].timestamp / 1000) + "s] " + systemLogs[i].message + "</div>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Read temperature with sanity checking
// Takes 3 samples and validates they're consistent
// Returns true if valid temp obtained, false if error
bool readTemperatureWithValidation(float &outTemp) {
  const int NUM_SAMPLES = 3;
  const float MAX_DIFF = 2.0; // Max difference between samples in °C
  const float MIN_VALID_TEMP = -50.0; // DS18B20 can read -55°C but let's be safe
  const float MAX_VALID_TEMP = 85.0;  // DS18B20 max is 125°C but 85°C is realistic

  for (int attempt = 0; attempt < 2; attempt++) { // Try twice
    float samples[NUM_SAMPLES];
    bool allValid = true;

    // Take 3 samples
    for (int i = 0; i < NUM_SAMPLES; i++) {
      sensors.requestTemperatures();
      delay(100); // Small delay between readings
      samples[i] = sensors.getTempCByIndex(0);

      // Check for obvious sensor errors
      if (samples[i] == -127.0 || samples[i] < MIN_VALID_TEMP || samples[i] > MAX_VALID_TEMP) {
        allValid = false;
        break;
      }
      yield(); // Feed watchdog
    }

    if (!allValid) {
      addLog("Temp read failed (attempt " + String(attempt + 1) + "), retrying...");
      delay(200);
      continue;
    }

    // Calculate average and check consistency
    float avg = (samples[0] + samples[1] + samples[2]) / 3.0;
    float maxDiff = 0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
      float diff = abs(samples[i] - avg);
      if (diff > maxDiff) maxDiff = diff;
    }

    if (maxDiff <= MAX_DIFF) {
      // Samples are consistent
      outTemp = avg;
      return true;
    } else {
      addLog("Temp samples inconsistent (diff=" + String(maxDiff, 2) + "C), retrying...");
      delay(200);
    }
  }

  // Both attempts failed
  addLog("ERROR: Temperature read failed after 2 attempts");
  return false;
}

void logTemperature(float temp, int sensorID) {
  // Get real Unix timestamp
  time_t now = time(nullptr);

  // Log with timestamp (or use millis/1000 as fallback before NTP sync)
  unsigned long timestamp;
  if (now < 1000000000) {
    // NTP not synced yet, use millis as seconds since boot
    timestamp = millis() / 1000;
    static bool warned = false;
    if (!warned) {
      addLog("Logging temp (NTP not synced yet)");
      warned = true;
    }
  } else {
    timestamp = (unsigned long)now;
    static bool ntpSynced = false;
    if (!ntpSynced) {
      addLog("NTP synced! Using real timestamps");
      ntpSynced = true;
    }
  }

  File logFile = LittleFS.open(LOG_FILE, "a");
  if (logFile) {
    logFile.printf("%lu,%.2f,%d\n", timestamp, temp, sensorID);
    logFile.close();
  } else {
    static bool fileError = false;
    if (!fileError) {
      addLog("ERROR: Cannot open log file");
      fileError = true;
    }
  }
}

void handleData() {
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
  // Cap at 500 lines max for memory safety
  int linesFor24h = (86400 / updateFrequency);
  if (linesFor24h > 500) linesFor24h = 500;
  if (linesFor24h < 100) linesFor24h = 100;

  // For large files, seek to approximately the right position
  // Estimate ~20 bytes per line
  long estimatedBytes = linesFor24h * 20L;
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

  addLog("Streamed " + String(lineCount) + " lines (~" + String(lineCount * updateFrequency / 3600) + "h)");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=== DEBUG1 - 4 Relay Test ===");

  // Initialize LittleFS
  if (!LittleFS.begin()) {
    addLog("ERROR: LittleFS mount failed!");
  } else {
    addLog("LittleFS mounted OK");
  }

  // Load saved settings
  loadSettings();

  // Initialize all relay pins to OFF
  for (int i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);  // Active-LOW: HIGH = OFF
  }
  addLog("All 4 relays initialized OFF");

  // Initialize temperature sensor
  sensors.begin();
  addLog("DS18B20 sensor initialized");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  addLog("Connecting to WiFi...");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    addLog("WiFi connected: " + WiFi.localIP().toString());
  } else {
    addLog("ERROR: WiFi connection failed!");
  }

  // Setup OTA
  ArduinoOTA.setHostname("debug1");
  ArduinoOTA.onStart([]() {
    addLog("OTA Update Starting...");
  });
  ArduinoOTA.onEnd([]() {
    addLog("OTA Update Complete!");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    addLog("OTA Error: " + String(error));
  });
  ArduinoOTA.begin();
  addLog("OTA ready");

  // Setup NTP time sync
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  addLog("NTP time sync started");

  // Setup web server
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/setmode", handleSetMode);
  server.on("/setthresholds", handleSetThresholds);
  server.on("/setfreq", handleSetFrequency);
  server.on("/setunit", handleSetUnit);
  server.on("/cleardata", handleClearData);
  server.on("/logs", handleLogs);
  server.on("/data", handleData);
  server.begin();
  addLog("Web server started");

  // Get initial temperature with validation
  float initialTemp;
  if (readTemperatureWithValidation(initialTemp)) {
    currentTemp = initialTemp;
    addLog("Initial temp: " + String(currentTemp, 1) + "C");
  } else {
    currentTemp = 20.0; // Safe default if sensor not working
    addLog("WARNING: Sensor read failed, using default 20.0C");
  }

  // Apply loaded settings to relays
  addLog("Applying relay settings...");
  for (int i = 0; i < 4; i++) {
    String msg = "R" + String(i + 1) + ": " + modeToString(relayModes[i]);
    msg += " (ON=" + String(tempOn[i], 1) + "C, OFF=" + String(tempOff[i], 1) + "C)";
    addLog(msg);
  }
  applyRelayLogic();

  addLog("=== SETUP COMPLETE ===");
  addLog("Web: http://" + WiFi.localIP().toString());
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  // Read temperature periodically
  unsigned long now = millis();
  if (now - lastTempUpdate >= (unsigned long)updateFrequency * 1000) {
    lastTempUpdate = now;

    float newTemp;
    if (readTemperatureWithValidation(newTemp)) {
      // Valid temperature reading
      currentTemp = newTemp;
      Serial.print("Temp: ");
      Serial.print(currentTemp, 1);
      Serial.print("C | Relays: ");
      for (int i = 0; i < 4; i++) {
        Serial.print(i + 1);
        Serial.print(":");
        Serial.print(relayStates[i] ? "ON" : "OFF");
        Serial.print("(");
        Serial.print(modeToString(relayModes[i]));
        Serial.print(") ");
      }
      Serial.println();
      applyRelayLogic();
      logTemperature(currentTemp, 0);
    } else {
      // Failed to read valid temperature, keep using last known good value
      addLog("Using last known temp: " + String(currentTemp, 1) + "C");
    }
  }

  delay(10);
}
