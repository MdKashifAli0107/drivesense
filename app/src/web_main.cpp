/**
 * DriveSense Live Browser Dashboard & Telemetry Web Server
 * 
 * Provides an embedded HTTP server in C++17 serving real-time vehicle telemetry
 * directly from /dev/drivesense to modern web browsers.
 */

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstring>
#include <csignal>
#include <iomanip>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include "SensorDevice.hpp"
#include "SensorData.hpp"
#include "AlertManager.hpp"
#include "drivesense_ioctl.h"

namespace {

std::atomic<bool> g_running{true};

void sigHandler(int sig) {
    (void)sig;
    g_running = false;
}

const char* INDEX_HTML = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>DriveSense - Virtual Car Sensor Live Dashboard</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;700;900&family=JetBrains+Mono:wght@400;600&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg: #090d16;
      --card-bg: rgba(18, 24, 38, 0.75);
      --card-border: rgba(255, 255, 255, 0.08);
      --primary: #00f2fe;
      --primary-glow: rgba(0, 242, 254, 0.35);
      --accent: #4facfe;
      --warn: #ffb703;
      --warn-glow: rgba(255, 183, 3, 0.35);
      --danger: #ff0055;
      --danger-glow: rgba(255, 0, 85, 0.4);
      --success: #00e676;
      --text: #f0f4f8;
      --text-muted: #8292a8;
      --font: 'Outfit', -apple-system, BlinkMacSystemFont, sans-serif;
      --font-mono: 'JetBrains Mono', monospace;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      background: radial-gradient(circle at 50% 10%, #152238 0%, var(--bg) 70%);
      color: var(--text);
      font-family: var(--font);
      min-height: 100vh;
      overflow-x: hidden;
      padding: 24px;
    }
    .header {
      max-width: 1300px;
      margin: 0 auto 24px auto;
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 16px 28px;
      background: var(--card-bg);
      backdrop-filter: blur(16px);
      border: 1px solid var(--card-border);
      border-radius: 18px;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.4);
    }
    .brand-wrap { display: flex; align-items: center; gap: 14px; }
    .brand-logo {
      width: 44px; height: 44px;
      background: linear-gradient(135deg, var(--primary), var(--accent));
      border-radius: 12px;
      display: flex; align-items: center; justify-content: center;
      box-shadow: 0 0 20px var(--primary-glow);
    }
    .brand-logo svg { width: 26px; height: 26px; fill: #070d18; }
    .brand-text h1 {
      font-size: 24px; font-weight: 900; letter-spacing: 1.5px;
      background: linear-gradient(90deg, #fff, var(--primary));
      -webkit-background-clip: text; -webkit-text-fill-color: transparent;
    }
    .brand-text span {
      font-size: 11px; font-family: var(--font-mono); color: var(--text-muted);
      letter-spacing: 2px; text-transform: uppercase;
    }
    .status-badge {
      display: flex; align-items: center; gap: 10px;
      padding: 8px 16px; border-radius: 30px;
      background: rgba(0, 0, 0, 0.4); border: 1px solid var(--card-border);
      font-size: 13px; font-weight: 600;
    }
    .status-dot {
      width: 10px; height: 10px; border-radius: 50%;
      background: var(--success); box-shadow: 0 0 12px var(--success);
      animation: pulse 1.5s infinite;
    }
    @keyframes pulse { 0%, 100% { transform: scale(1); opacity: 1; } 50% { transform: scale(1.3); opacity: 0.6; } }

    .main-grid {
      max-width: 1300px;
      margin: 0 auto;
      display: grid;
      grid-template-columns: 320px 1fr 320px;
      gap: 24px;
    }
    @media (max-width: 1050px) {
      .main-grid { grid-template-columns: 1fr; }
    }
    .card {
      background: var(--card-bg);
      backdrop-filter: blur(16px);
      border: 1px solid var(--card-border);
      border-radius: 20px;
      padding: 24px;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.35);
      position: relative;
      overflow: hidden;
    }
    .card-title {
      font-size: 13px; font-weight: 700; text-transform: uppercase;
      letter-spacing: 1.5px; color: var(--text-muted);
      margin-bottom: 20px; display: flex; justify-content: space-between; align-items: center;
    }

    /* Speedometer Gauge in Center */
    .speed-card {
      display: flex; flex-direction: column; align-items: center; justify-content: center;
      text-align: center;
    }
    .gauge-wrapper {
      position: relative; width: 340px; height: 340px;
      display: flex; align-items: center; justify-content: center;
    }
    .gauge-svg {
      width: 100%; height: 100%;
      transform: rotate(-135deg);
    }
    .gauge-bg-circle {
      fill: none; stroke: rgba(255, 255, 255, 0.06);
      stroke-width: 18; stroke-dasharray: 615; stroke-dashoffset: 154;
      stroke-linecap: round;
    }
    .gauge-val-circle {
      fill: none; stroke: url(#speedGradient);
      stroke-width: 18; stroke-dasharray: 615; stroke-dashoffset: 615;
      stroke-linecap: round;
      transition: stroke-dashoffset 0.35s ease, stroke 0.35s ease;
      filter: drop-shadow(0 0 12px var(--primary-glow));
    }
    .gauge-center-content {
      position: absolute; display: flex; flex-direction: column; align-items: center; justify-content: center;
    }
    .speed-number {
      font-size: 82px; font-weight: 900; line-height: 1;
      font-family: var(--font-mono);
      background: linear-gradient(180deg, #ffffff, #d2e4ff);
      -webkit-background-clip: text; -webkit-text-fill-color: transparent;
      text-shadow: 0 0 30px var(--primary-glow);
    }
    .speed-unit {
      font-size: 16px; font-weight: 700; letter-spacing: 3px;
      color: var(--primary); text-transform: uppercase; margin-top: 4px;
    }
    .state-pill {
      margin-top: 14px; padding: 6px 18px; border-radius: 20px;
      font-size: 13px; font-weight: 700; letter-spacing: 1.5px;
      background: rgba(0, 242, 254, 0.12); border: 1px solid var(--primary);
      color: var(--primary);
      text-transform: uppercase;
    }

    /* Sub Gauges */
    .metric-row {
      display: flex; flex-direction: column; gap: 20px;
    }
    .sub-meter {
      background: rgba(0, 0, 0, 0.3); border-radius: 14px;
      padding: 16px; border: 1px solid rgba(255, 255, 255, 0.05);
    }
    .sub-meter-header {
      display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px;
    }
    .sub-meter-name { font-size: 14px; font-weight: 600; color: var(--text-muted); }
    .sub-meter-val { font-size: 22px; font-weight: 700; font-family: var(--font-mono); }
    .bar-track {
      width: 100%; height: 10px; background: rgba(255, 255, 255, 0.07);
      border-radius: 10px; overflow: hidden; position: relative;
    }
    .bar-fill {
      height: 100%; width: 0%; border-radius: 10px;
      transition: width 0.35s ease, background 0.35s ease;
    }
    .fill-fuel { background: linear-gradient(90deg, #ff0055, #ffb703 40%, var(--success) 70%); }
    .fill-temp { background: linear-gradient(90deg, var(--primary), var(--warn) 60%, var(--danger) 90%); }

    /* Tyre Monitor */
    .tyre-diagram {
      display: grid; grid-template-columns: 1fr 1fr; gap: 14px;
      margin: 15px 0;
    }
    .tyre-box {
      background: rgba(0, 0, 0, 0.35); border: 1px solid rgba(255, 255, 255, 0.06);
      border-radius: 12px; padding: 12px; text-align: center;
      transition: all 0.3s ease;
    }
    .tyre-box.tyre-warn {
      border-color: var(--danger); background: rgba(255, 0, 85, 0.1);
      box-shadow: 0 0 15px var(--danger-glow);
    }
    .tyre-pos { font-size: 11px; font-weight: 700; color: var(--text-muted); letter-spacing: 1px; }
    .tyre-val { font-size: 20px; font-weight: 800; font-family: var(--font-mono); margin: 4px 0; }

    /* Controls & Fault Injection Deck */
    .control-deck {
      max-width: 1300px; margin: 24px auto 0 auto;
      display: grid; grid-template-columns: 1.2fr 1fr; gap: 24px;
    }
    @media (max-width: 900px) {
      .control-deck { grid-template-columns: 1fr; }
    }
    .btn-group { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 12px; }
    .btn {
      padding: 12px 18px; border-radius: 10px; border: none;
      font-size: 13px; font-weight: 700; cursor: pointer;
      display: flex; align-items: center; gap: 8px;
      transition: all 0.2s ease; text-transform: uppercase; letter-spacing: 1px;
    }
    .btn:hover { transform: translateY(-2px); }
    .btn:active { transform: translateY(1px); }
    .btn-start {
      background: linear-gradient(135deg, #00e676, #00b0ff); color: #070d18;
      box-shadow: 0 4px 15px rgba(0, 230, 118, 0.3);
    }
    .btn-stop {
      background: rgba(255, 255, 255, 0.1); color: #fff;
      border: 1px solid rgba(255, 255, 255, 0.2);
    }
    .btn-reset {
      background: linear-gradient(135deg, #4facfe, #00f2fe); color: #070d18;
      box-shadow: 0 4px 15px var(--primary-glow);
    }
    .btn-fault {
      background: rgba(255, 0, 85, 0.15); color: #ff5588;
      border: 1px solid rgba(255, 0, 85, 0.3);
    }
    .btn-fault:hover {
      background: var(--danger); color: #fff;
      box-shadow: 0 4px 20px var(--danger-glow);
    }

    /* Alert Banner & Log */
    .alert-banner {
      max-width: 1300px; margin: 0 auto 20px auto;
      padding: 14px 24px; border-radius: 12px;
      display: none; align-items: center; gap: 14px;
      background: rgba(255, 0, 85, 0.2); border: 1px solid var(--danger);
      box-shadow: 0 0 25px var(--danger-glow);
      font-weight: 600;
    }
    .alert-banner.show { display: flex; animation: slideDown 0.3s ease; }
    @keyframes slideDown { from { transform: translateY(-10px); opacity: 0; } to { transform: translateY(0); opacity: 1; } }

    .log-box {
      background: rgba(0, 0, 0, 0.45); border-radius: 10px;
      border: 1px solid rgba(255, 255, 255, 0.05);
      height: 180px; overflow-y: auto; padding: 12px;
      font-family: var(--font-mono); font-size: 12px;
      display: flex; flex-direction: column-reverse; gap: 6px;
    }
    .log-entry { display: flex; gap: 10px; line-height: 1.4; }
    .log-time { color: var(--text-muted); }
    .log-CRITICAL { color: #ff3366; font-weight: 700; }
    .log-WARNING { color: #ffb703; font-weight: 700; }
    .log-INFO { color: #00f2fe; }
  </style>
</head>
<body>

  <!-- Top Alert Banner -->
  <div id="alertBanner" class="alert-banner">
    <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/></svg>
    <span id="alertBannerText">CRITICAL WARNING</span>
  </div>

  <!-- Header -->
  <div class="header">
    <div class="brand-wrap">
      <div class="brand-logo">
        <svg viewBox="0 0 24 24"><path d="M18.92 6.01C18.72 5.42 18.16 5 17.5 5h-11c-.66 0-1.21.42-1.42 1.01L3 12v8c0 .55.45 1 1 1h1c.55 0 1-.45 1-1v-1h12v1c0 .55.45 1 1 1h1c.55 0 1-.45 1-1v-8l-2.08-5.99zM6.5 16c-.83 0-1.5-.67-1.5-1.5S5.67 13 6.5 13s1.5.67 1.5 1.5S7.33 16 6.5 16zm11 0c-.83 0-1.5-.67-1.5-1.5s.67-1.5 1.5-1.5 1.5.67 1.5 1.5-.67 1.5-1.5 1.5zM5 11l1.5-4.5h11L19 11H5z"/></svg>
      </div>
      <div class="brand-text">
        <h1>DRIVESENSE</h1>
        <span>Virtual Linux Kernel Telemetry System</span>
      </div>
    </div>
    <div class="status-badge">
      <div id="statusDot" class="status-dot"></div>
      <span id="kernelStatusText">DRIVER CONNECTED (/dev/drivesense)</span>
    </div>
  </div>

  <!-- Main Telemetry Grid -->
  <div class="main-grid">
    
    <!-- Left Column: Fuel & Engine Temp -->
    <div class="card">
      <div class="card-title">
        <span>Engine & Fuel Diagnostics</span>
        <span style="font-family: var(--font-mono);" id="seqTag">#0</span>
      </div>
      <div class="metric-row">
        <!-- Fuel Meter -->
        <div class="sub-meter">
          <div class="sub-meter-header">
            <span class="sub-meter-name">Fuel Level</span>
            <span class="sub-meter-val" id="fuelVal">--%</span>
          </div>
          <div class="bar-track">
            <div id="fuelBar" class="bar-fill fill-fuel"></div>
          </div>
        </div>

        <!-- Engine Temp Meter -->
        <div class="sub-meter">
          <div class="sub-meter-header">
            <span class="sub-meter-name">Coolant Temperature</span>
            <span class="sub-meter-val" id="tempVal">--°C</span>
          </div>
          <div class="bar-track">
            <div id="tempBar" class="bar-fill fill-temp"></div>
          </div>
        </div>

        <!-- Kernel Operational Statistics -->
        <div class="sub-meter" style="margin-top: 10px;">
          <div class="card-title" style="margin-bottom: 8px;">Kernel Stats (/proc)</div>
          <div style="font-family: var(--font-mono); font-size: 12px; display: flex; flex-direction: column; gap: 6px; color: var(--text-muted);">
            <div>Syscall Reads: <span id="statReads" style="color: #fff;">0</span></div>
            <div>IOCTL Commands: <span id="statIoctls" style="color: #fff;">0</span></div>
            <div>Driver Updates: <span id="statUpdates" style="color: #fff;">0</span></div>
            <div>Faults Injected: <span id="statFaults" style="color: #fff;">0</span></div>
          </div>
        </div>
      </div>
    </div>

    <!-- Center Column: Cyber Speedometer -->
    <div class="card speed-card">
      <div class="card-title" style="width: 100%;">
        <span>Instrument Speedometer</span>
        <span id="activeFaultTag" style="color: var(--danger); font-weight: 700;">FAULT: NONE</span>
      </div>
      <div class="gauge-wrapper">
        <svg class="gauge-svg" viewBox="0 0 240 240">
          <defs>
            <linearGradient id="speedGradient" x1="0%" y1="0%" x2="100%" y2="100%">
              <stop offset="0%" stop-color="#00f2fe" />
              <stop offset="70%" stop-color="#4facfe" />
              <stop offset="100%" stop-color="#ff0055" />
            </linearGradient>
          </defs>
          <circle class="gauge-bg-circle" cx="120" cy="120" r="98" />
          <circle id="speedCircle" class="gauge-val-circle" cx="120" cy="120" r="98" />
        </svg>
        <div class="gauge-center-content">
          <div class="speed-number" id="speedVal">0</div>
          <div class="speed-unit">KM / H</div>
          <div class="state-pill" id="statePill">IDLE</div>
        </div>
      </div>
    </div>

    <!-- Right Column: Tyre Pressure Monitor -->
    <div class="card">
      <div class="card-title">
        <span>Tyre Pressure System (TPMS)</span>
        <span id="tyreStatusTag" style="color: var(--success); font-weight: 700;">NORMAL</span>
      </div>
      <div class="tyre-diagram">
        <div class="tyre-box" id="tyreFL">
          <div class="tyre-pos">FRONT LEFT</div>
          <div class="tyre-val" id="psiFL">-- PSI</div>
          <div style="font-size: 10px; color: var(--text-muted);">NOMINAL: 32</div>
        </div>
        <div class="tyre-box" id="tyreFR">
          <div class="tyre-pos">FRONT RIGHT</div>
          <div class="tyre-val" id="psiFR">-- PSI</div>
          <div style="font-size: 10px; color: var(--text-muted);">NOMINAL: 32</div>
        </div>
        <div class="tyre-box" id="tyreRL">
          <div class="tyre-pos">REAR LEFT</div>
          <div class="tyre-val" id="psiRL">-- PSI</div>
          <div style="font-size: 10px; color: var(--text-muted);">NOMINAL: 32</div>
        </div>
        <div class="tyre-box" id="tyreRR">
          <div class="tyre-pos">REAR RIGHT</div>
          <div class="tyre-val" id="psiRR">-- PSI</div>
          <div style="font-size: 10px; color: var(--text-muted);">NOMINAL: 32</div>
        </div>
      </div>
      <div style="font-size: 11px; color: var(--text-muted); text-align: center; margin-top: 10px;">
        Warning threshold: &lt; 26 PSI (Edge Triggered Alert)
      </div>
    </div>

  </div>

  <!-- Bottom Deck: Controls & Live Event Log -->
  <div class="control-deck">
    
    <!-- Controls & Fault Injection Deck -->
    <div class="card">
      <div class="card-title">
        <span>Kernel IOCTL Simulation Controls</span>
      </div>
      <div style="font-size: 12px; color: var(--text-muted); margin-bottom: 8px;">Vehicle State Transitions:</div>
      <div class="btn-group">
        <button class="btn btn-start" onclick="sendCmd('start')">🚀 Start Engine</button>
        <button class="btn btn-stop" onclick="sendCmd('stop')">🛑 Stop Engine</button>
        <button class="btn btn-reset" onclick="sendCmd('reset')">🔄 Reset Faults</button>
      </div>

      <div style="font-size: 12px; color: var(--text-muted); margin-top: 18px; margin-bottom: 8px;">Real-Time Fault Injections (ioctl DS_IOC_INJECT_FAULT):</div>
      <div class="btn-group">
        <button class="btn btn-fault" onclick="sendCmd('fault', 'overheat')">🔥 Overheat Engine</button>
        <button class="btn btn-fault" onclick="sendCmd('fault', 'lowfuel')">⛽ Low Fuel</button>
        <button class="btn btn-fault" onclick="sendCmd('fault', 'flattyre')">🛞 Flat Tyre</button>
        <button class="btn btn-fault" onclick="sendCmd('fault', 'overspeed')">⚡ Overspeed</button>
      </div>
    </div>

    <!-- Live Event Log -->
    <div class="card">
      <div class="card-title">
        <span>Live Alert & Transition Log</span>
        <button style="background: none; border: none; color: var(--text-muted); cursor: pointer; font-size: 11px;" onclick="clearLogs()">CLEAR</button>
      </div>
      <div class="log-box" id="logBox">
        <!-- Log entries stream in dynamically -->
      </div>
    </div>

  </div>

  <script>
    const MAX_SPEED = 200;
    const CIRCLE_CIRCUMFERENCE = 615; // 2 * PI * 98
    const GAUGE_SPAN = 461; // 75% arc

    function setSpeed(speed) {
      document.getElementById('speedVal').innerText = Math.round(speed);
      const ratio = Math.min(Math.max(speed / MAX_SPEED, 0), 1);
      const offset = CIRCLE_CIRCUMFERENCE - (ratio * GAUGE_SPAN);
      document.getElementById('speedCircle').style.strokeDashoffset = offset;
    }

    function addLogEntry(level, msg) {
      const logBox = document.getElementById('logBox');
      const time = new Date().toTimeString().split(' ')[0];
      const div = document.createElement('div');
      div.className = 'log-entry';
      div.innerHTML = `<span class="log-time">[${time}]</span> <span class="log-${level}">[${level}]</span> <span>${msg}</span>`;
      logBox.prepend(div);
      while (logBox.children.length > 50) {
        logBox.removeChild(logBox.lastChild);
      }
    }

    function clearLogs() {
      document.getElementById('logBox').innerHTML = '';
    }

    let lastFault = "NONE";
    let lastState = "";

    async function fetchTelemetry() {
      try {
        const res = await fetch('/api/telemetry');
        if (!res.ok) throw new Error('HTTP ' + res.status);
        const data = await res.json();

        // 1. Center Speedometer
        setSpeed(data.speed_kmh);
        const statePill = document.getElementById('statePill');
        statePill.innerText = data.state;
        if (data.state === 'FAULT') {
          statePill.style.borderColor = 'var(--danger)';
          statePill.style.color = 'var(--danger)';
          statePill.style.background = 'rgba(255, 0, 85, 0.15)';
        } else if (data.state === 'DRIVING') {
          statePill.style.borderColor = 'var(--primary)';
          statePill.style.color = 'var(--primary)';
          statePill.style.background = 'rgba(0, 242, 254, 0.12)';
        } else {
          statePill.style.borderColor = 'var(--text-muted)';
          statePill.style.color = 'var(--text-muted)';
          statePill.style.background = 'rgba(255, 255, 255, 0.05)';
        }

        // Active fault tag
        const faultTag = document.getElementById('activeFaultTag');
        faultTag.innerText = 'FAULT: ' + data.active_fault;
        faultTag.style.color = (data.active_fault === 'NONE') ? 'var(--text-muted)' : 'var(--danger)';

        // 2. Fuel & Temp
        document.getElementById('fuelVal').innerText = data.fuel_pct + '%';
        document.getElementById('fuelBar').style.width = Math.min(Math.max(data.fuel_pct, 0), 100) + '%';

        document.getElementById('tempVal').innerText = data.engine_temp_c + '°C';
        const tempPct = Math.min(Math.max((data.engine_temp_c - 20) / 110 * 100, 0), 100);
        document.getElementById('tempBar').style.width = tempPct + '%';

        // 3. Tyres
        const psi = data.tyre_psi;
        ['FL', 'FR', 'RL', 'RR'].forEach(id => {
          document.getElementById('psi' + id).innerText = psi + ' PSI';
          const el = document.getElementById('tyre' + id);
          if (psi < 26) {
            el.classList.add('tyre-warn');
          } else {
            el.classList.remove('tyre-warn');
          }
        });
        const tyreTag = document.getElementById('tyreStatusTag');
        if (psi < 26) {
          tyreTag.innerText = 'LOW PRESSURE';
          tyreTag.style.color = 'var(--danger)';
        } else {
          tyreTag.innerText = 'NORMAL';
          tyreTag.style.color = 'var(--success)';
        }

        // 4. Stats & Sequence
        document.getElementById('seqTag').innerText = '#' + data.sequence;
        if (data.stats) {
          document.getElementById('statReads').innerText = data.stats.reads;
          document.getElementById('statIoctls').innerText = data.stats.ioctls;
          document.getElementById('statUpdates').innerText = data.stats.updates;
          document.getElementById('statFaults').innerText = data.stats.faults_injected;
        }

        // 5. Alert Banner
        const alertBanner = document.getElementById('alertBanner');
        const alertText = document.getElementById('alertBannerText');
        if (data.active_fault !== 'NONE') {
          alertBanner.classList.add('show');
          alertText.innerText = 'CRITICAL FAULT DETECTED: ' + data.active_fault + ' IN KERNEL SPACE';
        } else if (data.speed_kmh > 120) {
          alertBanner.classList.add('show');
          alertText.innerText = 'WARNING: SPEED LIMIT EXCEEDED (' + data.speed_kmh + ' km/h > 120 km/h)';
        } else if (data.engine_temp_c > 105) {
          alertBanner.classList.add('show');
          alertText.innerText = 'CRITICAL: ENGINE OVERHEATING (' + data.engine_temp_c + '°C > 105°C)';
        } else if (data.fuel_pct < 10) {
          alertBanner.classList.add('show');
          alertText.innerText = 'WARNING: LOW FUEL LEVEL (' + data.fuel_pct + '% < 10%)';
        } else if (data.tyre_psi < 26) {
          alertBanner.classList.add('show');
          alertText.innerText = 'WARNING: TYRE PRESSURE LOW (' + data.tyre_psi + ' PSI < 26 PSI)';
        } else {
          alertBanner.classList.remove('show');
        }

        // State changes to log
        if (data.state !== lastState && lastState !== "") {
          addLogEntry('INFO', 'State transitioned from ' + lastState + ' -> ' + data.state);
        }
        lastState = data.state;

        if (data.active_fault !== lastFault && lastFault !== "") {
          if (data.active_fault !== "NONE") {
            addLogEntry('CRITICAL', 'Fault injected: ' + data.active_fault);
          } else {
            addLogEntry('INFO', 'Faults cleared. System restored to normal operation.');
          }
        }
        lastFault = data.active_fault;

        // Process newly evaluated alerts
        if (data.new_alerts && data.new_alerts.length > 0) {
          data.new_alerts.forEach(a => addLogEntry(a.level, a.message));
        }

      } catch (err) {
        document.getElementById('kernelStatusText').innerText = 'RECONNECTING TO DRIVER...';
        document.getElementById('statusDot').style.background = 'var(--danger)';
      }
    }

    async function sendCmd(action, type = '') {
      try {
        let url = '/api/control?action=' + action;
        if (type) url += '&type=' + type;
        const res = await fetch(url, { method: 'POST' });
        const json = await res.json();
        addLogEntry('INFO', 'Command [' + action + (type ? ':' + type : '') + ']: ' + (json.message || 'OK'));
        setTimeout(fetchTelemetry, 100);
      } catch (err) {
        addLogEntry('CRITICAL', 'Failed to send command: ' + err);
      }
    }

    // Initial log
    addLogEntry('INFO', 'DriveSense Web Cockpit initialized. Connecting to /dev/drivesense...');
    setInterval(fetchTelemetry, 300);
    fetchTelemetry();
  </script>
</body>
</html>
)rawliteral";

struct TelemetrySnapshot {
    SensorData data;
    struct ds_stats stats{};
    std::vector<Alert> new_alerts;
};

std::mutex g_telemetry_mutex;
TelemetrySnapshot g_latest_snapshot;

void readerWorker(SensorDevice& device, AlertManager& alert_mgr) {
    while (g_running) {
        try {
            SensorData current_data = device.read();
            struct ds_stats current_stats = device.getStats();
            std::vector<Alert> alerts = alert_mgr.evaluate(current_data);

            {
                std::lock_guard<std::mutex> lock(g_telemetry_mutex);
                g_latest_snapshot.data = current_data;
                g_latest_snapshot.stats = current_stats;
                if (!alerts.empty()) {
                    g_latest_snapshot.new_alerts.insert(
                        g_latest_snapshot.new_alerts.end(),
                        alerts.begin(),
                        alerts.end()
                    );
                }
            }
        } catch (const std::exception& e) {
            // Wait briefly before retrying
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
}

std::string buildTelemetryJson() {
    TelemetrySnapshot snap;
    {
        std::lock_guard<std::mutex> lock(g_telemetry_mutex);
        snap = g_latest_snapshot;
        g_latest_snapshot.new_alerts.clear();
    }

    std::ostringstream oss;
    oss << "{\n"
        << "  \"speed_kmh\": " << snap.data.speed_kmh << ",\n"
        << "  \"fuel_pct\": " << snap.data.fuel_pct << ",\n"
        << "  \"engine_temp_c\": " << snap.data.engine_temp_c << ",\n"
        << "  \"tyre_psi\": " << snap.data.tyre_psi << ",\n"
        << "  \"state\": \"" << snap.data.getStateString() << "\",\n"
        << "  \"active_fault\": \"" << snap.data.getFaultString() << "\",\n"
        << "  \"sequence\": " << snap.data.sequence << ",\n"
        << "  \"timestamp_ns\": " << snap.data.timestamp_ns << ",\n"
        << "  \"stats\": {\n"
        << "    \"reads\": " << snap.stats.reads << ",\n"
        << "    \"ioctls\": " << snap.stats.ioctls << ",\n"
        << "    \"updates\": " << snap.stats.updates << ",\n"
        << "    \"faults_injected\": " << snap.stats.faults_injected << ",\n"
        << "    \"open_count\": " << snap.stats.open_count << "\n"
        << "  },\n"
        << "  \"new_alerts\": [\n";

    for (size_t i = 0; i < snap.new_alerts.size(); ++i) {
        const auto& a = snap.new_alerts[i];
        oss << "    {\"sensor\": \"" << a.sensor_name << "\", \"level\": \"" << a.severity
            << "\", \"message\": \"" << a.message << "\"}";
        if (i + 1 < snap.new_alerts.size()) oss << ",";
        oss << "\n";
    }

    oss << "  ]\n}";
    return oss.str();
}

void handleClient(int client_fd, SensorDevice& device, AlertManager& alert_mgr) {
    char buffer[4096];
    ssize_t bytes = ::recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        ::close(client_fd);
        return;
    }
    buffer[bytes] = '\0';

    std::string request(buffer);
    std::string method, path;
    {
        std::istringstream iss(request);
        iss >> method >> path;
    }

    if (method == "GET") {
        if (path == "/" || path == "/index.html") {
            std::string content = INDEX_HTML;
            std::ostringstream response;
            response << "HTTP/1.1 200 OK\r\n"
                     << "Content-Type: text/html; charset=utf-8\r\n"
                     << "Content-Length: " << content.size() << "\r\n"
                     << "Connection: close\r\n\r\n"
                     << content;
            std::string res_str = response.str();
            (void)::send(client_fd, res_str.c_str(), res_str.size(), 0);
        } else if (path == "/api/telemetry") {
            std::string json = buildTelemetryJson();
            std::ostringstream response;
            response << "HTTP/1.1 200 OK\r\n"
                     << "Content-Type: application/json\r\n"
                     << "Access-Control-Allow-Origin: *\r\n"
                     << "Content-Length: " << json.size() << "\r\n"
                     << "Connection: close\r\n\r\n"
                     << json;
            std::string res_str = response.str();
            (void)::send(client_fd, res_str.c_str(), res_str.size(), 0);
        } else {
            std::string not_found = "404 Not Found";
            std::ostringstream response;
            response << "HTTP/1.1 404 Not Found\r\n"
                     << "Content-Length: " << not_found.size() << "\r\n"
                     << "Connection: close\r\n\r\n"
                     << not_found;
            std::string res_str = response.str();
            (void)::send(client_fd, res_str.c_str(), res_str.size(), 0);
        }
    } else if (method == "POST") {
        if (path.rfind("/api/control", 0) == 0) {
            std::string action;
            std::string type;

            size_t qpos = path.find('?');
            if (qpos != std::string::npos) {
                std::string query = path.substr(qpos + 1);
                std::istringstream qss(query);
                std::string param;
                while (std::getline(qss, param, '&')) {
                    size_t eq = param.find('=');
                    if (eq != std::string::npos) {
                        std::string k = param.substr(0, eq);
                        std::string v = param.substr(eq + 1);
                        if (k == "action") action = v;
                        else if (k == "type") type = v;
                    }
                }
            }

            std::string reply_msg = "Command processed";
            bool success = true;

            try {
                if (action == "start") {
                    device.start();
                    reply_msg = "Engine started (DS_IOC_START)";
                } else if (action == "stop") {
                    device.stop();
                    reply_msg = "Engine stopped (DS_IOC_STOP)";
                } else if (action == "reset") {
                    device.reset();
                    alert_mgr.clear();
                    reply_msg = "Faults reset (DS_IOC_RESET)";
                } else if (action == "fault") {
                    if (type == "overheat") {
                        device.injectFault(DS_FAULT_OVERHEAT);
                        reply_msg = "Injected Overheat Fault";
                    } else if (type == "lowfuel") {
                        device.injectFault(DS_FAULT_LOW_FUEL);
                        reply_msg = "Injected Low Fuel Fault";
                    } else if (type == "flattyre") {
                        device.injectFault(DS_FAULT_FLAT_TYRE);
                        reply_msg = "Injected Flat Tyre Fault";
                    } else if (type == "overspeed") {
                        device.injectFault(DS_FAULT_OVERSPEED);
                        reply_msg = "Injected Overspeed Fault";
                    } else {
                        success = false;
                        reply_msg = "Unknown fault type";
                    }
                } else {
                    success = false;
                    reply_msg = "Unknown action";
                }
            } catch (const std::exception& ex) {
                success = false;
                reply_msg = ex.what();
            }

            std::ostringstream json_resp;
            json_resp << "{\"success\": " << (success ? "true" : "false")
                      << ", \"message\": \"" << reply_msg << "\"}";
            std::string json_str = json_resp.str();

            std::ostringstream response;
            response << "HTTP/1.1 200 OK\r\n"
                     << "Content-Type: application/json\r\n"
                     << "Access-Control-Allow-Origin: *\r\n"
                     << "Content-Length: " << json_str.size() << "\r\n"
                     << "Connection: close\r\n\r\n"
                     << json_str;
            std::string res_str = response.str();
            (void)::send(client_fd, res_str.c_str(), res_str.size(), 0);
        } else {
            std::string not_found = "404 Not Found";
            std::ostringstream response;
            response << "HTTP/1.1 404 Not Found\r\n"
                     << "Content-Length: " << not_found.size() << "\r\n"
                     << "Connection: close\r\n\r\n"
                     << not_found;
            std::string res_str = response.str();
            (void)::send(client_fd, res_str.c_str(), res_str.size(), 0);
        }
    }

    ::close(client_fd);
}

} // namespace

int main(int argc, char* argv[]) {
    int port = 8080;
    std::string device_path = "/dev/drivesense";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--device" && i + 1 < argc) {
            device_path = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [--port <PORT>] [--device <PATH>]\n"
                      << "  --port <PORT>    HTTP listen port (default: 8080)\n"
                      << "  --device <PATH>  Driver device path (default: /dev/drivesense)\n";
            return 0;
        }
    }

    std::signal(SIGINT, sigHandler);
    std::signal(SIGTERM, sigHandler);

    std::cout << "========================================================\n"
              << "   DriveSense Live Browser Telemetry & Cockpit Server   \n"
              << "========================================================\n";

    SensorDevice device;
    try {
        device = SensorDevice(device_path);
        std::cout << "[+] Connected to Linux kernel device: " << device_path << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "[-] Error connecting to device: " << ex.what() << "\n";
        return 1;
    }

    AlertManager alert_mgr(50);

    // Launch background sensor polling thread
    std::thread reader_thread(readerWorker, std::ref(device), std::ref(alert_mgr));

    // Create server socket
    int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "[-] Failed to create socket: " << std::strerror(errno) << "\n";
        g_running = false;
        reader_thread.join();
        return 1;
    }

    int opt = 1;
    (void)::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "[-] Failed to bind to port " << port << ": " << std::strerror(errno) << "\n";
        ::close(server_fd);
        g_running = false;
        reader_thread.join();
        return 1;
    }

    if (::listen(server_fd, 32) < 0) {
        std::cerr << "[-] Failed to listen: " << std::strerror(errno) << "\n";
        ::close(server_fd);
        g_running = false;
        reader_thread.join();
        return 1;
    }

    std::cout << "[+] Web Cockpit running at: http://0.0.0.0:" << port << "\n"
              << "[+] Open in browser:        http://192.168.1.3:" << port << "\n"
              << "[+] Telemetry API endpoint: http://192.168.1.3:" << port << "/api/telemetry\n"
              << "Press Ctrl+C to stop.\n"
              << "--------------------------------------------------------\n";

    // Main accept loop with poll to allow clean exit on SIGINT
    while (g_running) {
        struct pollfd pfd;
        pfd.fd = server_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int ret = ::poll(&pfd, 1, 500); // 500ms timeout
        if (ret > 0 && (pfd.revents & POLLIN)) {
            struct sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = ::accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
            if (client_fd >= 0) {
                handleClient(client_fd, device, alert_mgr);
            }
        }
    }

    std::cout << "\n[!] Shutting down web server...\n";
    ::close(server_fd);
    reader_thread.join();
    std::cout << "[+] DriveSense Web Server stopped gracefully.\n";

    return 0;
}
