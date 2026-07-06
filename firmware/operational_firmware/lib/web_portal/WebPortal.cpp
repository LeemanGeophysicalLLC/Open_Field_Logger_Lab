#include "WebPortal.h"

#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <string.h>

#include "logo_png.h"
#include "version.h"

namespace {

WebPortalHooks g_hooks;
char g_ssid[33] = "OpenFieldLogger";
bool g_wifi_on = false;  // radio starts off by design; see webPortalSetWifiOn()
WebServer server(80);

// ---------------------------------------------------------------------------
// Small JSON helpers. There's no ArduinoJson dependency here on purpose —
// every response this firmware sends is a handful of known fields, so a
// hand-built string is simpler to read than pulling in a JSON library for
// it. jsonEscape() is the one thing that has to be careful: it's what
// keeps a stray '"' or '\' in, say, an error message from producing
// invalid JSON.
// ---------------------------------------------------------------------------
String jsonEscape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in[i];
    if (c == '\\' || c == '"') out += '\\';
    out += c;
  }
  return out;
}

void sendJsonOk() { server.send(200, "application/json", "{\"ok\":true}"); }

void sendJsonError(int code, const String &msg) {
  String body = "{\"ok\":false,\"error\":\"" + jsonEscape(msg) + "\"}";
  server.send(code, "application/json", body);
}

// ---------------------------------------------------------------------------
// The single page. Every value on it is filled in client-side via fetch(),
// so this is a plain constant — nothing here is built per-request.
// ---------------------------------------------------------------------------
const char PAGE_HTML[] = R"RAWPAGE(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Open Field Logger</title>
<style>
:root{--bg:#eef2f3;--ink:#12303b;--muted:#567784;--brand:#5d8897;--brand-deep:#3f6674;
--accent:#f59b1b;--accent-ink:#8a5402;--line:#d2dde1;--good:#2f8f5b;--warn:#d9a200;--bad:#cc4639;}
*{box-sizing:border-box}
body{margin:0;background:radial-gradient(circle at top left,#f7f8f8 0,var(--bg) 45%,#dbe4e7 100%);
color:var(--ink);font:15px/1.45 'Segoe UI',Arial,sans-serif}
.wrap{max-width:1100px;margin:0 auto;padding:20px 16px 50px}
.hero-card{background:rgba(255,255,255,.92);border:1px solid var(--line);border-radius:20px;
box-shadow:0 14px 34px rgba(69,100,112,.14);padding:20px 22px;margin-bottom:16px}
.brand-row{display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:12px}
.brand-logo{display:block;height:56px;width:auto}
.subtitle{font-size:1rem;letter-spacing:.16em;color:var(--brand);margin-top:6px}
.logging-badge{padding:10px 18px;border-radius:999px;font-weight:800;letter-spacing:.06em;
background:#e7ebec;color:var(--muted)}
.logging-badge.on{background:rgba(204,70,57,.14);color:var(--bad);animation:pulse 1.6s infinite}
@keyframes pulse{0%{opacity:1}50%{opacity:.55}100%{opacity:1}}
.status-strip{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:10px;margin-top:16px}
.kv{padding:10px 12px;border-radius:14px;background:linear-gradient(180deg,#fff,#f7fbfc);border:1px solid #dfe7ea}
.k{display:block;font-size:.7rem;letter-spacing:.05em;color:var(--muted);margin-bottom:2px;text-transform:uppercase}
.v{font-size:1rem;font-weight:700}
.error-banner{margin-top:14px;padding:10px 14px;border-radius:12px;background:rgba(204,70,57,.12);
color:var(--bad);font-weight:700}
.tabs{display:flex;gap:8px;margin-bottom:16px;flex-wrap:wrap}
.tab-btn{border:0;border-radius:999px;padding:10px 18px;font-weight:700;background:#e3ebee;
color:var(--brand-deep);cursor:pointer}
.tab-btn.active{background:linear-gradient(135deg,var(--brand),var(--brand-deep));color:#fff}
.tab-panel{display:none}
.tab-panel.active{display:block}
.card{background:rgba(255,255,255,.92);border:1px solid var(--line);border-radius:20px;
box-shadow:0 14px 34px rgba(69,100,112,.1);padding:20px 22px;margin-bottom:16px}
.card h2{margin:0 0 14px;font-size:.85rem;letter-spacing:.1em;text-transform:uppercase;color:var(--brand-deep)}
.form-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:14px}
label{display:block;font-size:.82rem;font-weight:700;color:var(--brand-deep);margin-bottom:6px}
select,input{width:100%;padding:9px 11px;border-radius:12px;border:1px solid var(--line);
background:#fff;color:var(--ink);font:inherit}
.actions{display:flex;gap:10px;flex-wrap:wrap;margin-top:14px}
.btn{appearance:none;border:0;border-radius:999px;padding:10px 16px;font-weight:800;cursor:pointer;
text-decoration:none;display:inline-flex;align-items:center}
.btn-accent{background:linear-gradient(135deg,var(--accent),#ffb648);color:#442600}
.btn-ghost{background:#eef4f6;color:var(--brand-deep)}
.msg{margin-top:10px;font-weight:700;min-height:1.2em}
.msg.err{color:var(--bad)}
.msg.ok{color:var(--good)}
.muted{color:var(--muted)}
.note{padding:10px 14px;border-radius:14px;background:rgba(204,70,57,.08);color:var(--bad);
font-size:.85rem;font-weight:600;margin-bottom:12px}
.progress-track{background:#e3ebee;border-radius:999px;height:10px;margin-top:14px;overflow:hidden}
.progress-fill{background:linear-gradient(135deg,var(--brand),var(--brand-deep));height:100%;
width:0%;transition:width .2s}
.lcd-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:16px}
.lcd-card{background:#0b1f16;border-radius:18px;padding:18px;text-align:center;
box-shadow:inset 0 0 24px rgba(0,0,0,.5)}
.lcd-label{color:#7fdba0;font-size:.75rem;letter-spacing:.14em;text-transform:uppercase;margin-bottom:8px}
.lcd-display{font-family:'Courier New',monospace;font-variant-numeric:tabular-nums;font-size:2.4rem;
font-weight:700;color:#39ff88;text-shadow:0 0 8px rgba(57,255,136,.85),0 0 18px rgba(57,255,136,.4)}
.lcd-unit{color:#4f8f68;font-size:.72rem;letter-spacing:.1em;margin-top:6px}
.file-table{width:100%;border-collapse:collapse;margin-top:10px}
.file-table th,.file-table td{padding:8px 6px;border-bottom:1px solid #edf2f4;text-align:left;font-size:.9rem}
.file-table th{color:var(--brand-deep);font-size:.72rem;text-transform:uppercase;letter-spacing:.05em}
.help-table{width:100%;border-collapse:collapse}
.help-table td{padding:10px 8px;border-bottom:1px solid #edf2f4;text-align:left;font-size:.92rem;vertical-align:top}
.help-table td:first-child{width:180px;font-weight:700;color:var(--brand-deep);white-space:nowrap}
.help-table code{background:#eef4f6;padding:1px 6px;border-radius:6px;font-size:.85em}
.channel-toggles{display:flex;gap:16px;flex-wrap:wrap;margin-bottom:12px}
#graphCanvas{width:100%;max-width:900px;height:auto;background:#0b1f27;border-radius:14px}
.foot{margin-top:10px;color:var(--muted);font-size:.82rem;text-align:center}
@media(max-width:700px){.brand-row{flex-direction:column;align-items:flex-start}}
</style>
</head>
<body>
<div class="wrap">

<header class="hero-card">
  <div class="brand-row">
    <div class="brand-text">
      <img class="brand-logo" src="/logo.png" alt="Leeman Geophysical">
      <div class="subtitle">OPEN FIELD LOGGER</div>
    </div>
    <div class="logging-badge" id="logging_badge">IDLE</div>
  </div>
  <div class="status-strip">
    <div class="kv"><span class="k">SD Card</span><span class="v" id="sd_label">--</span></div>
    <div class="kv"><span class="k">RTC</span><span class="v" id="rtc_label">--</span></div>
    <div class="kv"><span class="k">Device Time</span><span class="v" id="rtc_time">--</span></div>
    <div class="kv"><span class="k">Log File</span><span class="v" id="log_file_label">--</span></div>
    <div class="kv"><span class="k">Achieved Rate</span><span class="v" id="achieved_rate">--</span></div>
  </div>
  <div class="actions" style="margin-top:12px">
    <button class="btn btn-ghost" onclick="turnOffWifi()">Turn Off WiFi</button>
  </div>
  <div class="error-banner" id="error_banner" style="display:none"></div>
</header>

<nav class="tabs">
  <button class="tab-btn active" id="tabbtn-reading" onclick="showTab('reading')">Reading</button>
  <button class="tab-btn" id="tabbtn-configuration" onclick="showTab('configuration')">Configuration</button>
  <button class="tab-btn" id="tabbtn-files" onclick="showTab('files')">File Explorer</button>
  <button class="tab-btn" id="tabbtn-graph" onclick="showTab('graph')">Graph</button>
  <button class="tab-btn" id="tabbtn-help" onclick="showTab('help')">Help</button>
</nav>

<section id="tab-reading" class="tab-panel active">
  <div class="lcd-grid">
    <div class="lcd-card"><div class="lcd-label">Channel 1 (AIN0)</div><div class="lcd-display" id="reading0">--.----</div><div class="lcd-unit">VOLTS</div></div>
    <div class="lcd-card"><div class="lcd-label">Channel 2 (AIN1)</div><div class="lcd-display" id="reading1">--.----</div><div class="lcd-unit">VOLTS</div></div>
    <div class="lcd-card"><div class="lcd-label">Channel 3 (AIN2)</div><div class="lcd-display" id="reading2">--.----</div><div class="lcd-unit">VOLTS</div></div>
    <div class="lcd-card"><div class="lcd-label">Channel 4 (AIN3)</div><div class="lcd-display" id="reading3">--.----</div><div class="lcd-unit">VOLTS</div></div>
  </div>
</section>

<section id="tab-configuration" class="tab-panel">
  <div class="card">
    <h2>ADC &amp; Logging Settings</h2>
    <div class="form-grid">
      <div><label for="gain">ADC Input Range (PGA)</label>
        <select id="gain" onchange="saveSettings()">
          <option value="0">0 - 6.144 V</option>
          <option value="1">0 - 4.096 V</option>
          <option value="2">0 - 2.048 V</option>
          <option value="3">0 - 1.024 V</option>
          <option value="4">0 - 0.512 V</option>
          <option value="5">0 - 0.256 V</option>
        </select>
      </div>
      <div><label for="rate_hz">Data Rate (Hz)</label>
        <select id="rate_hz" onchange="saveSettings()">
          <option value="1">1</option>
          <option value="2">2</option>
          <option value="5">5</option>
          <option value="10">10</option>
        </select>
      </div>
      <div><label for="averages">Points To Average</label>
        <select id="averages" onchange="saveSettings()">
          <option value="1">1</option>
          <option value="2">2</option>
          <option value="4">4</option>
          <option value="8">8</option>
          <option value="16">16</option>
        </select>
      </div>
    </div>
    <div class="msg" id="config_msg"></div>
  </div>

  <div class="card">
    <h2>Set Clock (MCP7940 RTC)</h2>
    <div class="form-grid">
      <div><label for="rtc_datetime">Date &amp; Time</label><input id="rtc_datetime" type="datetime-local" step="1"></div>
    </div>
    <div class="actions">
      <button class="btn btn-accent" onclick="syncFromBrowser()">Sync From Browser Clock</button>
      <button class="btn btn-ghost" onclick="syncManual()">Set Manual Time</button>
    </div>
    <div class="msg" id="rtc_msg"></div>
  </div>

  <div class="card">
    <h2>Firmware Update (OTA)</h2>
    <div class="kv" style="margin-bottom:12px;display:inline-block">
      <span class="k">Running Version</span><span class="v" id="firmware_version_display">--</span>
    </div>
    <div class="note">
      Only upload a .bin file built for this exact board. An incorrect or corrupt firmware
      file can leave the device unusable until it is reflashed over USB — double-check the
      file before uploading.
    </div>
    <input type="file" id="firmware_file" accept=".bin">
    <div class="actions">
      <button class="btn btn-accent" onclick="uploadFirmware()">Upload And Restart</button>
    </div>
    <div class="progress-track"><div class="progress-fill" id="firmware_progress"></div></div>
    <div class="msg" id="firmware_msg"></div>
  </div>
</section>

<section id="tab-files" class="tab-panel">
  <div class="card">
    <h2>SD Card Files</h2>
    <div class="actions"><button class="btn btn-ghost" onclick="refreshFiles()">Refresh</button></div>
    <table class="file-table">
      <thead><tr><th>File</th><th>Size</th><th></th><th></th></tr></thead>
      <tbody id="files_body"><tr><td colspan="4" class="muted">Loading...</td></tr></tbody>
    </table>
  </div>
</section>

<section id="tab-graph" class="tab-panel">
  <div class="card">
    <h2>Last 2 Minutes</h2>
    <div class="channel-toggles">
      <label><input type="checkbox" id="ch_toggle_0" checked> Channel 1</label>
      <label><input type="checkbox" id="ch_toggle_1" checked> Channel 2</label>
      <label><input type="checkbox" id="ch_toggle_2" checked> Channel 3</label>
      <label><input type="checkbox" id="ch_toggle_3" checked> Channel 4</label>
    </div>
    <canvas id="graphCanvas" width="900" height="360"></canvas>
  </div>
</section>

<section id="tab-help" class="tab-panel">
  <div class="card">
    <h2>Quick Start</h2>
    <table class="help-table">
      <tr><td>1. Turn on WiFi</td><td>WiFi is off by default to save power. <b>Double-click</b> the physical Log button to turn it on — there is no timeout, so it stays on until you turn it off again (another double-click, or the "Turn Off WiFi" button on this page).</td></tr>
      <tr><td>2. Connect</td><td>Join this device's WiFi network (shown as the device's SSID — printed to the serial console, and it's also the network name you'll see in your WiFi list once step 1 is done). No password is required. Then browse to <code>http://192.168.4.1/</code> if the page doesn't open automatically.</td></tr>
      <tr><td>3. Configure</td><td>Use the <b>Configuration</b> tab to set the ADC input range, data rate, and averaging before you start logging, and to set the clock.</td></tr>
      <tr><td>4. Start/stop logging</td><td><b>Single-click</b> the physical Log button on the board. This is the only way to start or stop logging — the web page cannot do it for you, so it always shows the true state of the device even if you're not looking at it when you click.</td></tr>
      <tr><td>5. Watch it work</td><td>The <b>Reading</b> tab shows live voltages on all four channels at all times, whether or not logging is active. The <b>Graph</b> tab plots the last two minutes.</td></tr>
      <tr><td>6. Get your data</td><td>Use the <b>File Explorer</b> tab to download (or delete) log files from the SD card. Files are named <code>log001.txt</code>, <code>log002.txt</code>, etc. — a new number every time you start a logging session.</td></tr>
      <tr><td>7. Done for now?</td><td>Turn WiFi back off with another <b>double-click</b>, or press "Turn Off WiFi" at the top of this page, to save battery power. Logging keeps running either way — WiFi is only for watching/configuring.</td></tr>
    </table>
  </div>

  <div class="card">
    <h2>Choosing Data Rate &amp; Averaging</h2>
    <table class="help-table">
      <tr><td>Data Rate</td><td>How many logged rows per second you want (1, 2, 5, or 10 Hz).</td></tr>
      <tr><td>Points To Average</td><td>How many raw ADC readings get combined into each logged row (1, 2, 4, 8, or 16). Higher averaging smooths out noise, but each extra reading takes real time on the chip.</td></tr>
      <tr><td>Why they trade off</td><td>Every logged row requires reading all four channels, each averaged that many times, so a higher data rate <i>and</i> higher averaging together ask the ADC to do a lot more work per second. The ADS1115 can only convert so fast — a demanding combination like 10 Hz with 16x averaging may be more than it can keep up with.</td></tr>
      <tr><td>What happens if it can't keep up</td><td>Nothing breaks. The firmware never blocks or errors over this — it just does its best and logs at whatever real rate results, and every row is timestamped with the actual time it was taken rather than the time you asked for.</td></tr>
      <tr><td>How to check</td><td><b>Achieved Rate</b> in the status strip at the top of every tab shows the real, measured logging rate while logging is active. Compare it to the Data Rate you selected.</td></tr>
      <tr><td>If Achieved Rate is noticeably lower</td><td>Try reducing <b>Points To Average</b> first — it has the biggest effect on how much work each row takes. Reduce the <b>Data Rate</b> next if you still need it lower. Use higher averaging for slowly-changing signals where cleaner data matters more than speed, and lower averaging when you need to keep up with a fast-changing signal at the full rate you picked.</td></tr>
    </table>
  </div>

  <div class="card">
    <h2>LED Guide</h2>
    <table class="help-table">
      <tr><td>At power-on</td><td>Both LEDs blink a few times together. This is just a self-test so you can see they both work — it doesn't mean anything is wrong.</td></tr>
      <tr><td>Log LED, off</td><td>Not logging. The LED never blinks while idle, regardless of WiFi state — a unit sitting out in the field with logging stopped stays dark.</td></tr>
      <tr><td>Log LED, 2 quick blinks (once)</td><td>WiFi was just turned <b>on</b>. A one-time acknowledgment right after a double-click (or nothing, if it happened while you were out of sight of the LED).</td></tr>
      <tr><td>Log LED, 5 quick blinks (once)</td><td>WiFi was just turned <b>off</b>. Same idea, so you know the "Turn Off WiFi" button (or a double-click) actually took effect. Remember there's no auto-timeout, so this is the only confirmation you'll get.</td></tr>
      <tr><td>Log LED, one quick pulse (~1.5s)</td><td>Logging, WiFi off. The normal state while actively recording data in the field.</td></tr>
      <tr><td>Log LED, three quick pulses (~1.5s)</td><td>Logging, WiFi on. Both are active at once — fine to do, just remember WiFi has no auto-timeout and will stay on until you turn it off.</td></tr>
      <tr><td>Error LED, on</td><td>Something needs attention: the RTC isn't running, the ADC wasn't detected, the SD card is missing (and logging was attempted), a write to the SD card failed, or all 999 log file numbers are already used. Check the status banner at the top of this page for the specific reason.</td></tr>
    </table>
  </div>

  <div class="card">
    <h2>Logged Data Format</h2>
    <div class="note" style="background:rgba(93,136,151,.08);color:var(--brand-deep)">
      Each log file is a plain CSV/text file: a header row, then one row per sample with the
      timestamp, the logger's uptime in milliseconds, and the four channel voltages —
      <code>timestamp,uptime_ms,ch1_v,ch2_v,ch3_v,ch4_v</code>. Open it in any spreadsheet or
      plotting tool.
    </div>
  </div>

  <div class="card">
    <h2>More Resources</h2>
    <div class="actions">
      <a class="btn btn-accent" href="https://github.com/LeemanGeophysicalLLC/Open_Field_Logger_Lab" target="_blank" rel="noopener">Project Repository (GitHub)</a>
      <a class="btn btn-ghost" href="https://leemangeophysical.com" target="_blank" rel="noopener">Leeman Geophysical Website</a>
    </div>
  </div>
</section>

<div class="foot">Leeman Geophysical LLC - Gentry, Arkansas, USA</div>
</div>

<script>
// Must match fullScaleVolts() in include/logger_types.h.
const GAIN_FULL_SCALE = [6.144,4.096,2.048,1.024,0.512,0.256];
let currentGainIndex = 1;
let graphBuffer = [];
// Kept in sync from pollStatus() so the File Explorer tab (refreshed on its
// own schedule, independent of the 1Hz status poll) knows which file to
// treat as untouchable without a separate round-trip.
let g_loggingActive = false;
let g_activeLogFile = '';

function showTab(name){
  document.querySelectorAll('.tab-panel').forEach(el=>el.classList.remove('active'));
  document.querySelectorAll('.tab-btn').forEach(el=>el.classList.remove('active'));
  document.getElementById('tab-'+name).classList.add('active');
  document.getElementById('tabbtn-'+name).classList.add('active');
  if(name==='files') refreshFiles();
}

function setText(id, text){ const el=document.getElementById(id); if(el) el.textContent = text; }

async function pollStatus(){
  try{
    const r = await fetch('/api/status',{cache:'no-store'});
    if(!r.ok) return;
    const s = await r.json();
    g_loggingActive = s.logging_active;
    g_activeLogFile = s.log_file;
    setText('firmware_version_display', s.firmware_version);
    setText('sd_label', s.sd_ok ? (s.sd_card_present?'Ready':'Card Missing') : 'Fault');
    setText('rtc_label', s.rtc_ok ? 'Running' : 'Fault');
    setText('rtc_time', s.rtc_time);
    setText('log_file_label', s.logging_active ? s.log_file : 'Idle');
    setText('achieved_rate', s.achieved_hz.toFixed(2)+' Hz');
    const badge = document.getElementById('logging_badge');
    if(s.logging_active){ badge.textContent = 'LOGGING - '+s.log_file; badge.classList.add('on'); }
    else { badge.textContent = 'IDLE'; badge.classList.remove('on'); }
    const banner = document.getElementById('error_banner');
    if(s.error_active){ banner.style.display='block'; banner.textContent='Fault: '+s.error_reason; }
    else { banner.style.display='none'; }
  }catch(e){}
}

// The Reading tab polls at ~5 Hz for responsiveness, but the graph only
// keeps two points per second — two minutes of 4-channel data at the full
// 5 Hz poll rate would be 2.5x the buffer for little visible benefit on a
// 900px-wide chart, so we downsample here rather than store everything and
// throw most of it away at draw time.
const GRAPH_PUSH_INTERVAL_MS = 500;
let lastGraphPushMs = -Infinity;

async function pollReading(){
  try{
    const r = await fetch('/api/reading',{cache:'no-store'});
    if(!r.ok) return;
    const s = await r.json();
    const v=[s.v0,s.v1,s.v2,s.v3];
    for(let i=0;i<4;i++) setText('reading'+i, v[i].toFixed(4));
    if(s.t_ms - lastGraphPushMs >= GRAPH_PUSH_INTERVAL_MS){
      lastGraphPushMs = s.t_ms;
      graphBuffer.push({t:s.t_ms, v});
      while(graphBuffer.length>0 && s.t_ms - graphBuffer[0].t > 120000) graphBuffer.shift();
      drawGraph();
    }
  }catch(e){}
}

function drawGraph(){
  const cv = document.getElementById('graphCanvas');
  const ctx = cv.getContext('2d');
  const padL=52, padR=14, padT=14, padB=32;
  const plotW = cv.width-padL-padR, plotH = cv.height-padT-padB;

  ctx.fillStyle = '#0b1f27'; ctx.fillRect(0,0,cv.width,cv.height);

  // X axis is a fixed 2-minute window ending "now" (the latest buffered
  // sample's device uptime), not just however much data happens to exist
  // yet — so the window position never jumps around as data fills in.
  const nowT = graphBuffer.length>0 ? graphBuffer[graphBuffer.length-1].t : 0;
  const tMin = nowT - 120000, tMax = nowT;

  // Y axis starts at the nominal 0V-to-full-scale range for the selected
  // gain (inputs are single-ended and ground-referenced, so they never go
  // negative), then autoscales to whatever the visible channels are
  // actually doing once there's data to look at.
  const fs = GAIN_FULL_SCALE[currentGainIndex] || 4.096;
  let vMin = 0, vMax = fs;
  const visibleVals = [];
  for(const p of graphBuffer){
    for(let ch=0; ch<4; ch++){
      const toggle = document.getElementById('ch_toggle_'+ch);
      if(toggle && toggle.checked) visibleVals.push(p.v[ch]);
    }
  }
  if(visibleVals.length>0){
    vMin = Math.min(...visibleVals);
    vMax = Math.max(...visibleVals);
    const pad = Math.max((vMax-vMin)*0.1, 0.01);
    vMin -= pad; vMax += pad;
    if(vMax-vMin < 1e-6) vMax = vMin+1;
  }

  const xPix = t => padL + ((t-tMin)/((tMax-tMin)||1))*plotW;
  const yPix = v => padT + plotH - ((v-vMin)/(vMax-vMin))*plotH;

  ctx.strokeStyle = 'rgba(255,255,255,.12)';
  ctx.fillStyle = '#9fb9c4';
  ctx.font = '11px monospace';
  ctx.lineWidth = 1;

  // X axis: gridlines every 30s across the fixed window, labeled in
  // seconds relative to "now" (0 = now, negative = seconds ago).
  ctx.textAlign = 'center';
  for(let secAgo=120; secAgo>=0; secAgo-=30){
    const x = xPix(tMax - secAgo*1000);
    ctx.beginPath(); ctx.moveTo(x,padT); ctx.lineTo(x,padT+plotH); ctx.stroke();
    ctx.fillText((-secAgo)+'s', x, cv.height-10);
  }
  ctx.textAlign = 'right';
  ctx.fillText('Time (s)', cv.width-padR, cv.height-10);

  // Y axis: a handful of evenly-spaced ticks labeled in volts.
  const yTicks = 4;
  ctx.textAlign = 'right';
  for(let i=0; i<=yTicks; i++){
    const v = vMin + (vMax-vMin)*i/yTicks;
    const y = yPix(v);
    ctx.beginPath(); ctx.moveTo(padL,y); ctx.lineTo(cv.width-padR,y); ctx.stroke();
    ctx.fillText(v.toFixed(2), padL-6, y+4);
  }
  ctx.save();
  ctx.translate(14, padT+plotH/2);
  ctx.rotate(-Math.PI/2);
  ctx.textAlign = 'center';
  ctx.fillText('Voltage (V)', 0, 0);
  ctx.restore();

  if(graphBuffer.length<2) return;

  const colors=['#f59b1b','#5dd6e8','#8ade5d','#e85dd6'];
  for(let ch=0; ch<4; ch++){
    const toggle = document.getElementById('ch_toggle_'+ch);
    if(toggle && !toggle.checked) continue;
    ctx.strokeStyle = colors[ch]; ctx.lineWidth=2; ctx.beginPath();
    let first=true;
    for(const p of graphBuffer){
      const x = xPix(p.t), y = yPix(p.v[ch]);
      if(first){ ctx.moveTo(x,y); first=false; } else ctx.lineTo(x,y);
    }
    ctx.stroke();
  }
}

async function loadSettings(){
  try{
    const r = await fetch('/api/settings');
    const s = await r.json();
    document.getElementById('gain').value = s.gain;
    document.getElementById('rate_hz').value = s.rate_hz;
    document.getElementById('averages').value = s.averages;
    currentGainIndex = s.gain;
  }catch(e){}
}

function showConfigMsg(text, isError){
  const el = document.getElementById('config_msg');
  el.textContent = text; el.className = 'msg ' + (isError?'err':'ok');
}
function showRtcMsg(text, isError){
  const el = document.getElementById('rtc_msg');
  el.textContent = text; el.className = 'msg ' + (isError?'err':'ok');
}

async function saveSettings(){
  const gain = document.getElementById('gain').value;
  const rate_hz = document.getElementById('rate_hz').value;
  const averages = document.getElementById('averages').value;
  try{
    const r = await fetch('/api/settings', {method:'POST', body:new URLSearchParams({gain, rate_hz, averages})});
    const j = await r.json();
    showConfigMsg(j.ok ? 'Settings applied.' : ('Error: '+j.error), !j.ok);
    if(j.ok) currentGainIndex = parseInt(gain,10);
  }catch(e){ showConfigMsg('Network error.', true); }
}

function pad2(n){ return String(n).padStart(2,'0'); }

// datetime-local wants "YYYY-MM-DDTHH:MM:SS" — built by hand from the local
// getters (not toISOString(), which is UTC) so the field shows the
// student's own wall-clock time.
function dateToLocalInputValue(d){
  return d.getFullYear()+'-'+pad2(d.getMonth()+1)+'-'+pad2(d.getDate())+'T'+
         pad2(d.getHours())+':'+pad2(d.getMinutes())+':'+pad2(d.getSeconds());
}

function fillDatetimeFromBrowser(){
  document.getElementById('rtc_datetime').value = dateToLocalInputValue(new Date());
}

async function postRtcFields(){
  const raw = document.getElementById('rtc_datetime').value;
  if(!raw){ showRtcMsg('Pick a date and time first.', true); return; }
  const [datePart, timePart] = raw.split('T');
  const [year, month, day] = datePart.split('-').map(Number);
  const timeBits = (timePart||'0:0:0').split(':').map(Number);
  const [hour, minute, second] = [timeBits[0]||0, timeBits[1]||0, timeBits[2]||0];
  const body = new URLSearchParams({year, month, day, hour, minute, second});
  try{
    const r = await fetch('/api/rtc/sync', {method:'POST', body});
    const j = await r.json();
    showRtcMsg(j.ok ? 'Clock updated.' : ('Error: '+j.error), !j.ok);
  }catch(e){ showRtcMsg('Network error.', true); }
}

function syncFromBrowser(){ fillDatetimeFromBrowser(); postRtcFields(); }
function syncManual(){ postRtcFields(); }

function showFirmwareMsg(text, isError){
  const el = document.getElementById('firmware_msg');
  el.textContent = text; el.className = 'msg ' + (isError?'err':'ok');
}

// Uses XMLHttpRequest rather than fetch() so we get real upload-progress
// events — a firmware image is big enough (hundreds of KB) that a silent
// multi-second wait would otherwise look like the page had hung.
function uploadFirmware(){
  const fileInput = document.getElementById('firmware_file');
  const file = fileInput.files[0];
  if(!file){ showFirmwareMsg('Choose a .bin file first.', true); return; }
  if(!confirm('Upload '+file.name+' and restart the device now? This cannot be undone.')) return;

  document.getElementById('firmware_progress').style.width = '0%';
  showFirmwareMsg('Uploading... 0%', false);

  const form = new FormData();
  form.append('firmware', file);

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/firmware/upload');
  xhr.upload.onprogress = (e)=>{
    if(e.lengthComputable){
      const pct = Math.round(e.loaded/e.total*100);
      document.getElementById('firmware_progress').style.width = pct+'%';
      showFirmwareMsg('Uploading... '+pct+'%', false);
    }
  };
  xhr.onload = ()=>{
    if(xhr.status===200){
      document.getElementById('firmware_progress').style.width = '100%';
      showFirmwareMsg('Upload complete. Device is restarting with new firmware...', false);
    } else {
      let msg = 'Upload failed.';
      try{ msg = 'Upload failed: '+JSON.parse(xhr.responseText).error; }catch(e){}
      showFirmwareMsg(msg, true);
    }
  };
  xhr.onerror = ()=>{ showFirmwareMsg('Network error during upload.', true); };
  xhr.send(form);
}

// Closing WiFi from here disconnects this very page, so there's no "done"
// state to show afterward — the browser will just report the request as
// failed/cancelled once the radio drops, which is expected and fine.
function turnOffWifi(){
  if(!confirm("Turn off WiFi now? You'll be disconnected from this page and will need to double-click the Log button on the device to reconnect.")) return;
  fetch('/api/wifi/off', {method:'POST'}).catch(()=>{});
}

async function refreshFiles(){
  const tbody = document.getElementById('files_body');
  try{
    const r = await fetch('/api/files');
    const j = await r.json();
    tbody.innerHTML = '';
    if(j.files.length===0){
      tbody.innerHTML = '<tr><td colspan="4" class="muted">No files on the SD card yet.</td></tr>';
      return;
    }
    j.files.forEach(f=>{
      const tr = document.createElement('tr');
      const kb = (f.size/1024).toFixed(1);
      const isActive = g_loggingActive && f.name === g_activeLogFile;
      const nameCell = document.createElement('td'); nameCell.textContent = f.name;
      const sizeCell = document.createElement('td'); sizeCell.textContent = kb+' KB';
      const dlCell = document.createElement('td');
      const delCell = document.createElement('td');
      if(isActive){
        // Actively being written by the logger — opening or removing it
        // from underneath that write crashes the controller, so there's
        // nothing to click here at all, just a reason why.
        dlCell.className = 'muted'; dlCell.textContent = 'Logging...';
      } else {
        const dl = document.createElement('a'); dl.className='btn btn-ghost'; dl.textContent='Download';
        dl.href = '/api/files/download?name='+encodeURIComponent(f.name);
        dlCell.appendChild(dl);
        const delBtn = document.createElement('button'); delBtn.className='btn btn-ghost'; delBtn.textContent='Delete';
        delBtn.onclick = ()=>deleteFile(f.name);
        delCell.appendChild(delBtn);
      }
      tr.appendChild(nameCell); tr.appendChild(sizeCell); tr.appendChild(dlCell); tr.appendChild(delCell);
      tbody.appendChild(tr);
    });
  }catch(e){
    tbody.innerHTML = '<tr><td colspan="4" class="muted">Could not load file list.</td></tr>';
  }
}

async function deleteFile(name){
  if(!confirm('Delete '+name+'? This cannot be undone.')) return;
  try{
    const r = await fetch('/api/files/delete', {method:'POST', body:new URLSearchParams({name})});
    const j = await r.json();
    if(!j.ok) alert('Could not delete: '+j.error);
  }catch(e){ alert('Network error.'); }
  refreshFiles();
}

loadSettings();
pollStatus(); setInterval(pollStatus, 1000);
pollReading(); setInterval(pollReading, 200);
</script>
</body>
</html>
)RAWPAGE";

// ---------------------------------------------------------------------------
// Route handlers
// ---------------------------------------------------------------------------
void handleRoot() { server.send(200, "text/html", PAGE_HTML); }

void handleLogo() {
  server.send_P(200, "image/png", reinterpret_cast<PGM_P>(LOGO_PNG), LOGO_PNG_LEN);
}

void handleStatusGet() {
  LiveStatus st;
  if (g_hooks.get_status != nullptr) g_hooks.get_status(st);

  String j = "{";
  j += "\"firmware_version\":\"" + jsonEscape(String(OFL_FIRMWARE_VERSION)) + "\",";
  j += "\"logging_active\":" + String(st.logging_active ? "true" : "false") + ",";
  j += "\"log_file\":\"" + jsonEscape(String(st.current_log_file)) + "\",";
  j += "\"sd_ok\":" + String(st.sd_ok ? "true" : "false") + ",";
  j += "\"sd_card_present\":" + String(st.sd_card_present ? "true" : "false") + ",";
  j += "\"rtc_ok\":" + String(st.rtc_ok ? "true" : "false") + ",";
  j += "\"rtc_time\":\"" + jsonEscape(String(st.rtc_time_str)) + "\",";
  j += "\"error_active\":" + String(st.error_active ? "true" : "false") + ",";
  j += "\"error_reason\":\"" + jsonEscape(String(st.error_reason)) + "\",";
  j += "\"achieved_hz\":" + String(st.achieved_sample_hz, 2) + ",";
  j += "\"uptime_ms\":" + String(st.uptime_ms) + ",";
  j += "\"samples_logged\":" + String(st.samples_logged);
  j += "}";
  server.send(200, "application/json", j);
}

void handleReadingGet() {
  ChannelSample cs;
  uint32_t uptime_ms = 0;
  bool logging_active = false;
  if (g_hooks.get_reading != nullptr) g_hooks.get_reading(cs, uptime_ms, logging_active);

  String j = "{";
  j += "\"t_ms\":" + String(uptime_ms) + ",";
  j += "\"v0\":" + String(cs.volts[0], 4) + ",";
  j += "\"v1\":" + String(cs.volts[1], 4) + ",";
  j += "\"v2\":" + String(cs.volts[2], 4) + ",";
  j += "\"v3\":" + String(cs.volts[3], 4) + ",";
  j += "\"logging\":" + String(logging_active ? "true" : "false");
  j += "}";
  server.send(200, "application/json", j);
}

void handleSettingsGet() {
  LoggerSettings s;
  if (g_hooks.get_settings != nullptr) g_hooks.get_settings(s);

  String j = "{";
  j += "\"gain\":" + String(static_cast<int>(s.adc_gain)) + ",";
  j += "\"rate_hz\":" + String(s.sample_rate_hz) + ",";
  j += "\"averages\":" + String(s.averages_per_sample);
  j += "}";
  server.send(200, "application/json", j);
}

void handleSettingsPost() {
  if (!server.hasArg("gain") || !server.hasArg("rate_hz") || !server.hasArg("averages")) {
    sendJsonError(400, "missing settings fields");
    return;
  }

  const int gain_raw = server.arg("gain").toInt();
  const int rate_raw = server.arg("rate_hz").toInt();
  const int avg_raw = server.arg("averages").toInt();

  if (gain_raw < 0 || !isValidAdcGain(static_cast<uint8_t>(gain_raw))) {
    sendJsonError(400, "invalid gain");
    return;
  }
  if (!isValidSampleRateHz(static_cast<uint8_t>(rate_raw))) {
    sendJsonError(400, "rate must be 1-10 Hz");
    return;
  }
  if (!isValidAverages(static_cast<uint8_t>(avg_raw))) {
    sendJsonError(400, "averages must be 1-16");
    return;
  }

  LoggerSettings s;
  s.adc_gain = static_cast<AdcGain>(gain_raw);
  s.sample_rate_hz = static_cast<uint8_t>(rate_raw);
  s.averages_per_sample = static_cast<uint8_t>(avg_raw);

  char err[48] = "";
  const bool ok = g_hooks.apply_settings != nullptr && g_hooks.apply_settings(s, err, sizeof(err));
  if (ok) {
    sendJsonOk();
  } else {
    sendJsonError(400, err[0] != '\0' ? err : "could not apply settings");
  }
}

void handleRtcSync() {
  if (!server.hasArg("year") || !server.hasArg("month") || !server.hasArg("day") ||
      !server.hasArg("hour") || !server.hasArg("minute") || !server.hasArg("second")) {
    sendJsonError(400, "missing time fields");
    return;
  }

  RtcDateTime dt;
  dt.year = static_cast<uint16_t>(server.arg("year").toInt());
  dt.month = static_cast<uint8_t>(server.arg("month").toInt());
  dt.day = static_cast<uint8_t>(server.arg("day").toInt());
  dt.hour = static_cast<uint8_t>(server.arg("hour").toInt());
  dt.minute = static_cast<uint8_t>(server.arg("minute").toInt());
  dt.second = static_cast<uint8_t>(server.arg("second").toInt());

  char err[48] = "";
  const bool ok = g_hooks.sync_rtc != nullptr && g_hooks.sync_rtc(dt, err, sizeof(err));
  if (ok) {
    sendJsonOk();
  } else {
    sendJsonError(400, err[0] != '\0' ? err : "could not set clock");
  }
}

void handleFilesGet() {
  static constexpr int kMaxFiles = 64;
  static SdFileInfo files[kMaxFiles];  // static: keeps this off the stack
  const int count = g_hooks.list_files != nullptr ? g_hooks.list_files(files, kMaxFiles) : 0;

  String j = "{\"files\":[";
  for (int i = 0; i < count; i++) {
    if (i > 0) j += ",";
    j += "{\"name\":\"" + jsonEscape(String(files[i].name)) + "\",\"size\":" +
         String(files[i].size_bytes) + "}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}

void handleFileDownload() {
  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "missing name");
    return;
  }
  if (g_hooks.open_file_for_download == nullptr) {
    server.send(500, "text/plain", "not available");
    return;
  }

  const String name = server.arg("name");

  // Opening a second handle onto the file the SD logger is actively
  // writing crashes the controller (same reason handleFileDelete refuses
  // the active file below) — refuse it here too rather than letting the
  // SD library's non-reentrant access do that.
  LiveStatus st;
  if (g_hooks.get_status != nullptr) g_hooks.get_status(st);
  if (st.logging_active && name == st.current_log_file) {
    server.send(409, "text/plain", "cannot download the active log file while logging");
    return;
  }

  File f = g_hooks.open_file_for_download(name.c_str());
  if (!f) {
    server.send(404, "text/plain", "file not found");
    return;
  }

  server.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
  server.streamFile(f, "text/csv");
  f.close();
}

void handleFileDelete() {
  if (!server.hasArg("name")) {
    sendJsonError(400, "missing name");
    return;
  }
  if (g_hooks.delete_file == nullptr) {
    sendJsonError(500, "not available");
    return;
  }

  const String name = server.arg("name");
  char err[48] = "";
  const bool ok = g_hooks.delete_file(name.c_str(), err, sizeof(err));
  if (ok) {
    sendJsonOk();
  } else {
    sendJsonError(409, err[0] != '\0' ? err : "could not delete file");
  }
}

// ---------------------------------------------------------------------------
// OTA firmware upload. The ESP32 Arduino core's Update library writes the
// incoming .bin straight into the inactive OTA partition (this board's
// default partition table has two — app0/app1 — specifically so this
// works), then the chip boots from whichever partition was written last.
// This is a genuinely destructive operation: an incompatible or corrupt
// image can leave the board needing a USB reflash to recover, since the
// Arduino core doesn't enable automatic rollback-on-crash by default. The
// page-side confirm() dialog and warning text are there for that reason.
//
// handleFirmwareUploadChunk() is called repeatedly by the WebServer library
// while the multipart upload is streaming in; handleFirmwareUploadResult()
// is called once at the very end to send the actual HTTP response.
// ---------------------------------------------------------------------------
void handleFirmwareUploadChunk() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    Update.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    Update.end(true);
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
  }
}

void handleFirmwareUploadResult() {
  const bool ok = !Update.hasError();
  if (!ok) {
    sendJsonError(500, "firmware update failed — device was not restarted");
    return;
  }
  sendJsonOk();
  delay(300);  // give the response time to actually reach the browser
  ESP.restart();
}

// Lets a student close out their own session from the page instead of
// waiting to walk back over and double-click the button. Same "respond
// first, then do the disruptive thing" pattern as the firmware upload
// restart above — the response has to actually reach the browser before we
// pull the radio out from under the connection carrying it.
void handleWifiOff() {
  sendJsonOk();
  delay(300);
  webPortalSetWifiOn(false);
}

}  // namespace

void webPortalInit(const WebPortalHooks &hooks, const char *ssid) {
  g_hooks = hooks;
  strlcpy(g_ssid, ssid, sizeof(g_ssid));
}

void webPortalBegin() {
  // WebServer needs the underlying LWIP TCP/IP task (and its message
  // queue) already running before server.begin() can succeed — confirmed
  // on hardware that WiFi.mode(WIFI_OFF) alone does NOT create it (that
  // call crashed server.begin() with "assert failed: tcpip_send_msg_wait_sem
  // ... Invalid mbox"). Only a real transition into an active mode
  // actually creates that task, so we briefly bring the AP up once here to
  // force it, then immediately take it back down. This is an internal
  // implementation detail, not a real "WiFi turned on" event, so it
  // deliberately bypasses webPortalSetWifiOn() — going through it would
  // fire the on_wifi_changed hook and blink the LED as if a student had
  // just double-clicked the button.
  WiFi.mode(WIFI_AP);
  WiFi.softAP(g_ssid, nullptr);
  delay(100);  // let the driver's own async bring-up settle before tearing it straight back down
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/logo.png", HTTP_GET, handleLogo);
  server.on("/api/status", HTTP_GET, handleStatusGet);
  server.on("/api/reading", HTTP_GET, handleReadingGet);
  server.on("/api/settings", HTTP_GET, handleSettingsGet);
  server.on("/api/settings", HTTP_POST, handleSettingsPost);
  server.on("/api/rtc/sync", HTTP_POST, handleRtcSync);
  server.on("/api/files", HTTP_GET, handleFilesGet);
  server.on("/api/files/download", HTTP_GET, handleFileDownload);
  server.on("/api/files/delete", HTTP_POST, handleFileDelete);
  server.on("/api/firmware/upload", HTTP_POST, handleFirmwareUploadResult, handleFirmwareUploadChunk);
  server.on("/api/wifi/off", HTTP_POST, handleWifiOff);
  server.begin();
}

void webPortalSetWifiOn(bool on) {
  if (on == g_wifi_on) {
    return;
  }
  if (on) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(g_ssid, nullptr);  // no password — a learning tool, open by design
  } else {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
  }
  g_wifi_on = on;
  if (g_hooks.on_wifi_changed != nullptr) {
    g_hooks.on_wifi_changed(on);
  }
}

bool webPortalIsWifiOn() { return g_wifi_on; }

void webPortalHandleClient() { server.handleClient(); }
