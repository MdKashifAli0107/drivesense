/**
 * DriveSense Live Browser Dashboard & Telemetry Web Server
 * 
 * Embedded high-performance C++17 telemetry HTTP server.
 * Connects directly to the Linux kernel character device (/dev/drivesense)
 * and streams real-time telemetry to modern web browsers.
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
  <title>DriveSense HMI &mdash; Automotive Telemetry Cluster</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Plus+Jakarta+Sans:wght@300;400;500;600;700;800&family=JetBrains+Mono:wght@400;500;600;700&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg-base: #06090e;
      --bg-gradient: radial-gradient(circle at 50% 0%, #111a2e 0%, #06090e 75%);
      --card-bg: rgba(13, 19, 32, 0.82);
      --card-elevated: rgba(18, 27, 46, 0.95);
      --card-border: rgba(255, 255, 255, 0.08);
      --card-border-light: rgba(255, 255, 255, 0.14);
      --card-inner-glow: inset 0 1px 0 rgba(255, 255, 255, 0.08);

      --cobalt: #38bdf8;
      --cobalt-deep: #0284c7;
      --cobalt-glow: rgba(56, 189, 248, 0.28);
      --cobalt-subtle: rgba(56, 189, 248, 0.08);

      --emerald: #10b981;
      --emerald-glow: rgba(16, 185, 129, 0.3);
      --emerald-subtle: rgba(16, 185, 129, 0.1);

      --amber: #f59e0b;
      --amber-glow: rgba(245, 158, 11, 0.3);
      --amber-subtle: rgba(245, 158, 11, 0.1);

      --crimson: #ef4444;
      --crimson-glow: rgba(239, 68, 68, 0.4);
      --crimson-subtle: rgba(239, 68, 68, 0.12);

      --text-main: #f8fafc;
      --text-muted: #94a3b8;
      --text-dim: #64748b;
      --text-dark: #334155;

      --font-sans: 'Plus Jakarta Sans', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      --font-mono: 'JetBrains Mono', 'SF Mono', Consolas, monospace;
    }

    * { box-sizing: border-box; margin: 0; padding: 0; }

    body {
      background: var(--bg-gradient);
      background-color: var(--bg-base);
      color: var(--text-main);
      font-family: var(--font-sans);
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      padding: 16px 24px;
      overflow-x: hidden;
      line-height: 1.5;
      -webkit-font-smoothing: antialiased;
    }

    body::before {
      content: "";
      position: fixed;
      top: 0; left: 0; width: 100%; height: 100%;
      background-image: 
        linear-gradient(rgba(255, 255, 255, 0.015) 1px, transparent 1px),
        linear-gradient(90deg, rgba(255, 255, 255, 0.015) 1px, transparent 1px);
      background-size: 40px 40px;
      pointer-events: none;
      z-index: 0;
    }

    .container {
      max-width: 1440px;
      width: 100%;
      margin: 0 auto;
      position: relative;
      z-index: 1;
      display: flex;
      flex-direction: column;
      gap: 18px;
    }

    /* TOP COCKPIT HUD */
    .cockpit-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      background: var(--card-bg);
      backdrop-filter: blur(20px);
      -webkit-backdrop-filter: blur(20px);
      border: 1px solid var(--card-border);
      box-shadow: var(--card-inner-glow), 0 12px 32px rgba(0, 0, 0, 0.5);
      border-radius: 16px;
      padding: 12px 24px;
    }

    .brand-section {
      display: flex;
      align-items: center;
      gap: 14px;
    }

    .brand-emblem {
      width: 40px;
      height: 40px;
      background: linear-gradient(135deg, #1e293b, #0f172a);
      border: 1px solid var(--card-border-light);
      border-radius: 10px;
      display: flex;
      align-items: center;
      justify-content: center;
      box-shadow: 0 0 15px rgba(56, 189, 248, 0.2);
    }
    .brand-emblem svg { width: 22px; height: 22px; fill: var(--cobalt); }

    .brand-meta h1 {
      font-size: 20px;
      font-weight: 800;
      letter-spacing: 2px;
      color: #ffffff;
      line-height: 1.1;
    }
    .brand-meta .badge-arch {
      font-family: var(--font-mono);
      font-size: 10px;
      font-weight: 600;
      letter-spacing: 1.5px;
      color: var(--text-muted);
      text-transform: uppercase;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .badge-arch span.kernel-tag {
      color: var(--cobalt);
      background: var(--cobalt-subtle);
      padding: 1px 6px;
      border-radius: 4px;
      border: 1px solid rgba(56, 189, 248, 0.25);
    }

    /* Gear Selector PRND */
    .gear-cluster {
      display: flex;
      align-items: center;
      gap: 6px;
      background: rgba(0, 0, 0, 0.4);
      padding: 4px 8px;
      border-radius: 10px;
      border: 1px solid var(--card-border);
    }
    .gear-btn {
      font-family: var(--font-mono);
      font-weight: 700;
      font-size: 14px;
      padding: 6px 14px;
      border-radius: 6px;
      color: var(--text-dim);
      background: transparent;
      border: none;
      transition: all 0.25s ease;
    }
    .gear-btn.active {
      color: #ffffff;
      background: linear-gradient(180deg, rgba(56, 189, 248, 0.3), rgba(2, 132, 199, 0.5));
      border: 1px solid var(--cobalt);
      box-shadow: 0 0 14px var(--cobalt-glow);
    }
    .gear-btn.active-fault {
      color: #ffffff;
      background: linear-gradient(180deg, rgba(239, 68, 68, 0.3), rgba(185, 28, 28, 0.5));
      border: 1px solid var(--crimson);
      box-shadow: 0 0 14px var(--crimson-glow);
    }

    /* Header Right Metrics */
    .vitals-bar {
      display: flex;
      align-items: center;
      gap: 16px;
    }
    .vital-item {
      display: flex;
      flex-direction: column;
      align-items: flex-end;
    }
    .vital-label {
      font-size: 10px;
      font-weight: 600;
      letter-spacing: 1px;
      text-transform: uppercase;
      color: var(--text-dim);
      font-family: var(--font-mono);
    }
    .vital-val {
      font-size: 13px;
      font-weight: 700;
      font-family: var(--font-mono);
      color: var(--text-main);
      display: flex;
      align-items: center;
      gap: 6px;
    }
    .pulse-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: var(--emerald);
      box-shadow: 0 0 10px var(--emerald);
      animation: pulse 1.8s infinite;
    }
    .pulse-dot.danger {
      background: var(--crimson);
      box-shadow: 0 0 10px var(--crimson);
    }
    @keyframes pulse {
      0%, 100% { opacity: 1; transform: scale(1); }
      50% { opacity: 0.4; transform: scale(1.25); }
    }

    .audio-toggle-btn {
      background: rgba(255, 255, 255, 0.05);
      border: 1px solid var(--card-border);
      color: var(--text-muted);
      border-radius: 8px;
      padding: 7px 10px;
      cursor: pointer;
      display: flex;
      align-items: center;
      gap: 6px;
      font-size: 11px;
      font-family: var(--font-mono);
      transition: all 0.2s ease;
    }
    .audio-toggle-btn:hover {
      background: rgba(255, 255, 255, 0.1);
      color: #fff;
    }
    .audio-toggle-btn.on {
      color: var(--cobalt);
      border-color: rgba(56, 189, 248, 0.4);
      background: var(--cobalt-subtle);
    }

    /* CRITICAL ALERT BANNER */
    .alert-strip {
      display: none;
      align-items: center;
      justify-content: space-between;
      padding: 12px 20px;
      border-radius: 12px;
      font-size: 13px;
      font-weight: 600;
      background: rgba(239, 68, 68, 0.15);
      border: 1px solid var(--crimson);
      box-shadow: 0 0 25px var(--crimson-glow);
      backdrop-filter: blur(10px);
      animation: alertSlide 0.3s ease-out;
    }
    .alert-strip.show { display: flex; }
    .alert-strip.warn {
      background: rgba(245, 158, 11, 0.15);
      border-color: var(--amber);
      box-shadow: 0 0 25px var(--amber-glow);
    }
    @keyframes alertSlide {
      from { transform: translateY(-8px); opacity: 0; }
      to { transform: translateY(0); opacity: 1; }
    }
    .alert-left {
      display: flex;
      align-items: center;
      gap: 12px;
    }
    .alert-icon {
      width: 28px;
      height: 28px;
      border-radius: 6px;
      display: flex;
      align-items: center;
      justify-content: center;
      background: var(--crimson);
      color: #fff;
      font-weight: 900;
    }
    .alert-strip.warn .alert-icon {
      background: var(--amber);
    }
    .alert-actions {
      display: flex;
      gap: 10px;
    }
    .btn-quick-reset {
      background: rgba(255, 255, 255, 0.1);
      border: 1px solid rgba(255, 255, 255, 0.2);
      color: #fff;
      padding: 4px 12px;
      border-radius: 6px;
      font-size: 11px;
      font-family: var(--font-mono);
      font-weight: 700;
      cursor: pointer;
      text-transform: uppercase;
      transition: all 0.2s;
    }
    .btn-quick-reset:hover {
      background: #fff;
      color: #000;
    }

    /* INSTRUMENT CLUSTER GRID */
    .cluster-grid {
      display: grid;
      grid-template-columns: 340px 1fr 340px;
      gap: 18px;
      align-items: stretch;
    }
    @media (max-width: 1150px) {
      .cluster-grid { grid-template-columns: 1fr; }
    }

    .panel {
      background: var(--card-bg);
      backdrop-filter: blur(20px);
      -webkit-backdrop-filter: blur(20px);
      border: 1px solid var(--card-border);
      box-shadow: var(--card-inner-glow), 0 16px 36px rgba(0, 0, 0, 0.4);
      border-radius: 18px;
      padding: 22px;
      display: flex;
      flex-direction: column;
      position: relative;
      overflow: hidden;
    }
    .panel::after {
      content: "";
      position: absolute;
      top: 0; left: 0; right: 0;
      height: 1px;
      background: linear-gradient(90deg, transparent, rgba(255, 255, 255, 0.12), transparent);
    }

    .panel-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 18px;
      padding-bottom: 10px;
      border-bottom: 1px solid rgba(255, 255, 255, 0.05);
    }
    .panel-title {
      font-size: 11px;
      font-weight: 700;
      letter-spacing: 1.8px;
      text-transform: uppercase;
      color: var(--text-muted);
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .panel-title svg { width: 14px; height: 14px; fill: var(--cobalt); }
    .panel-tag {
      font-family: var(--font-mono);
      font-size: 11px;
      color: var(--text-dim);
    }

    /* LEFT WING: POWERTRAIN & THERMAL */
    .gauge-block {
      background: rgba(0, 0, 0, 0.35);
      border: 1px solid rgba(255, 255, 255, 0.05);
      border-radius: 14px;
      padding: 16px;
      margin-bottom: 14px;
      position: relative;
    }
    .gauge-block-top {
      display: flex;
      justify-content: space-between;
      align-items: baseline;
      margin-bottom: 10px;
    }
    .gauge-name {
      font-size: 12px;
      font-weight: 600;
      color: var(--text-muted);
      letter-spacing: 0.5px;
    }
    .gauge-val-big {
      font-family: var(--font-mono);
      font-size: 24px;
      font-weight: 800;
      color: #fff;
    }
    .gauge-val-big small {
      font-size: 12px;
      font-weight: 600;
      color: var(--text-dim);
      margin-left: 2px;
    }

    .linear-bar-wrap {
      width: 100%;
      height: 10px;
      background: rgba(255, 255, 255, 0.06);
      border-radius: 8px;
      overflow: hidden;
      position: relative;
    }
    .linear-bar-fill {
      height: 100%;
      width: 0%;
      border-radius: 8px;
      transition: width 0.3s cubic-bezier(0.4, 0, 0.2, 1), background-color 0.3s ease;
    }
    .fill-fuel-nom {
      background: linear-gradient(90deg, var(--cobalt-deep), var(--emerald));
      box-shadow: 0 0 10px var(--emerald-glow);
    }
    .fill-fuel-low {
      background: linear-gradient(90deg, var(--crimson), var(--amber));
      box-shadow: 0 0 12px var(--crimson-glow);
    }
    .fill-temp-nom {
      background: linear-gradient(90deg, var(--cobalt), var(--emerald));
    }
    .fill-temp-hot {
      background: linear-gradient(90deg, var(--amber), var(--crimson));
      box-shadow: 0 0 12px var(--crimson-glow);
    }

    .gauge-footer-meta {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-top: 8px;
      font-family: var(--font-mono);
      font-size: 11px;
      color: var(--text-dim);
    }

    /* Subsystem Stats Table */
    .kernel-stats-card {
      background: rgba(0, 0, 0, 0.25);
      border: 1px solid rgba(255, 255, 255, 0.04);
      border-radius: 12px;
      padding: 14px;
      margin-top: auto;
    }
    .kernel-stats-title {
      font-size: 10px;
      font-weight: 700;
      letter-spacing: 1.5px;
      color: var(--text-dim);
      text-transform: uppercase;
      margin-bottom: 10px;
      display: flex;
      justify-content: space-between;
    }
    .kernel-stats-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
    }
    .stat-box {
      display: flex;
      flex-direction: column;
      background: rgba(255, 255, 255, 0.02);
      border: 1px solid rgba(255, 255, 255, 0.04);
      padding: 8px 10px;
      border-radius: 8px;
    }
    .stat-box .k-label {
      font-size: 9px;
      color: var(--text-dim);
      font-family: var(--font-mono);
      text-transform: uppercase;
    }
    .stat-box .k-num {
      font-size: 14px;
      font-weight: 700;
      font-family: var(--font-mono);
      color: var(--cobalt);
    }

    /* CENTER WING: PANORAMIC SPEEDOMETER */
    .speedo-panel {
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: space-between;
      position: relative;
      padding: 24px;
    }

    .speedo-stage {
      position: relative;
      width: 380px;
      height: 380px;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    @media (max-width: 440px) {
      .speedo-stage { width: 300px; height: 300px; }
    }

    .speedo-svg {
      width: 100%;
      height: 100%;
      transform: rotate(0deg);
      filter: drop-shadow(0 8px 24px rgba(0, 0, 0, 0.6));
    }

    .speedo-center {
      position: absolute;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      text-align: center;
      pointer-events: none;
    }

    .speedo-display-val {
      font-family: var(--font-mono);
      font-size: 92px;
      font-weight: 800;
      line-height: 0.9;
      color: #ffffff;
      letter-spacing: -2px;
      text-shadow: 0 0 40px rgba(56, 189, 248, 0.35);
      transition: color 0.25s ease;
    }
    .speedo-display-val.over-limit {
      color: #ff4d6d;
      text-shadow: 0 0 40px rgba(255, 77, 109, 0.6);
    }
    .speedo-unit {
      font-family: var(--font-mono);
      font-size: 13px;
      font-weight: 700;
      letter-spacing: 4px;
      color: var(--cobalt);
      margin-top: 6px;
      text-transform: uppercase;
    }

    .drive-mode-pill {
      margin-top: 14px;
      padding: 6px 18px;
      border-radius: 24px;
      font-family: var(--font-mono);
      font-size: 11px;
      font-weight: 700;
      letter-spacing: 2px;
      text-transform: uppercase;
      background: rgba(255, 255, 255, 0.05);
      border: 1px solid rgba(255, 255, 255, 0.12);
      color: var(--text-muted);
      transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    }
    .drive-mode-pill.mode-DRIVING {
      background: rgba(56, 189, 248, 0.12);
      border-color: var(--cobalt);
      color: var(--cobalt);
      box-shadow: 0 0 20px rgba(56, 189, 248, 0.25);
    }
    .drive-mode-pill.mode-FAULT {
      background: rgba(239, 68, 68, 0.15);
      border-color: var(--crimson);
      color: var(--crimson);
      box-shadow: 0 0 20px var(--crimson-glow);
      animation: alertPulse 1.2s infinite;
    }
    @keyframes alertPulse {
      0%, 100% { transform: scale(1); }
      50% { transform: scale(1.05); }
    }

    .speedo-bottom-telemetry {
      width: 100%;
      display: flex;
      justify-content: space-around;
      align-items: center;
      background: rgba(0, 0, 0, 0.35);
      border: 1px solid rgba(255, 255, 255, 0.05);
      border-radius: 12px;
      padding: 10px 16px;
      margin-top: 16px;
    }
    .tele-cell {
      display: flex;
      flex-direction: column;
      align-items: center;
    }
    .tele-cell .t-lbl {
      font-size: 9px;
      font-family: var(--font-mono);
      letter-spacing: 1px;
      color: var(--text-dim);
      text-transform: uppercase;
    }
    .tele-cell .t-val {
      font-size: 13px;
      font-weight: 700;
      font-family: var(--font-mono);
      color: #fff;
    }

    /* RIGHT WING: CHASSIS & TPMS */
    .tpms-chassis-stage {
      display: flex;
      align-items: center;
      justify-content: center;
      position: relative;
      margin: 10px 0;
      min-height: 220px;
    }
    .car-silhouette {
      width: 130px;
      opacity: 0.85;
      filter: drop-shadow(0 0 15px rgba(56, 189, 248, 0.15));
    }

    .tpms-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
    }
    .wheel-card {
      background: rgba(0, 0, 0, 0.35);
      border: 1px solid rgba(255, 255, 255, 0.06);
      border-radius: 12px;
      padding: 12px;
      display: flex;
      flex-direction: column;
      align-items: center;
      text-align: center;
      transition: all 0.3s ease;
      position: relative;
    }
    .wheel-card.punctured {
      background: rgba(239, 68, 68, 0.15);
      border-color: var(--crimson);
      box-shadow: 0 0 20px var(--crimson-glow);
    }
    .wheel-pos {
      font-size: 10px;
      font-weight: 700;
      font-family: var(--font-mono);
      color: var(--text-dim);
      letter-spacing: 1px;
      text-transform: uppercase;
    }
    .wheel-psi {
      font-family: var(--font-mono);
      font-size: 20px;
      font-weight: 800;
      color: #fff;
      margin: 2px 0;
    }
    .wheel-card.punctured .wheel-psi {
      color: var(--crimson);
    }
    .wheel-status {
      font-size: 9px;
      font-weight: 700;
      font-family: var(--font-mono);
      letter-spacing: 1px;
      color: var(--emerald);
      text-transform: uppercase;
    }
    .wheel-card.punctured .wheel-status {
      color: var(--crimson);
    }

    .chassis-health-bar {
      margin-top: auto;
      padding: 12px;
      background: rgba(0, 0, 0, 0.25);
      border: 1px solid rgba(255, 255, 255, 0.04);
      border-radius: 12px;
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .chassis-row {
      display: flex;
      justify-content: space-between;
      font-size: 11px;
      font-family: var(--font-mono);
      color: var(--text-muted);
    }
    .chassis-badge {
      font-weight: 700;
      color: var(--emerald);
    }

    /* BOTTOM DECK: SWITCHBOARD & BLACK-BOX RECORDER */
    .deck-grid {
      display: grid;
      grid-template-columns: 1.25fr 1fr;
      gap: 18px;
    }
    @media (max-width: 950px) {
      .deck-grid { grid-template-columns: 1fr; }
    }

    .switchboard-section-title {
      font-size: 10px;
      font-weight: 700;
      letter-spacing: 1.8px;
      text-transform: uppercase;
      color: var(--text-dim);
      font-family: var(--font-mono);
      margin-bottom: 8px;
      margin-top: 4px;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }

    .btn-row {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
      margin-bottom: 16px;
    }
    @media (max-width: 600px) {
      .btn-row { grid-template-columns: 1fr; }
    }

    .cockpit-btn {
      padding: 12px 14px;
      border-radius: 10px;
      border: 1px solid transparent;
      font-family: var(--font-sans);
      font-size: 12px;
      font-weight: 700;
      letter-spacing: 0.8px;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
      text-transform: uppercase;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
      position: relative;
      overflow: hidden;
    }
    .cockpit-btn:hover {
      transform: translateY(-2px);
      box-shadow: 0 6px 18px rgba(0, 0, 0, 0.5);
    }
    .cockpit-btn:active {
      transform: translateY(1px);
    }
    .cockpit-btn .k-badge {
      font-family: var(--font-mono);
      font-size: 9px;
      padding: 1px 5px;
      border-radius: 4px;
      background: rgba(255, 255, 255, 0.15);
      color: #fff;
    }

    .btn-drive-start {
      background: linear-gradient(135deg, #059669, #10b981);
      color: #ffffff;
      border-color: rgba(16, 185, 129, 0.4);
      box-shadow: 0 4px 16px var(--emerald-glow);
    }
    .btn-drive-start:hover {
      background: linear-gradient(135deg, #047857, #059669);
    }

    .btn-drive-stop {
      background: rgba(255, 255, 255, 0.06);
      color: #ffffff;
      border: 1px solid rgba(255, 255, 255, 0.15);
    }
    .btn-drive-stop:hover {
      background: rgba(255, 255, 255, 0.12);
      border-color: rgba(255, 255, 255, 0.3);
    }

    .btn-drive-reset {
      background: linear-gradient(135deg, var(--cobalt-deep), #0369a1);
      color: #ffffff;
      border-color: rgba(56, 189, 248, 0.4);
      box-shadow: 0 4px 16px var(--cobalt-glow);
    }
    .btn-drive-reset:hover {
      background: linear-gradient(135deg, #0369a1, #075985);
    }

    /* Fault Injection Switches */
    .fault-matrix {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: 10px;
    }
    @media (max-width: 700px) {
      .fault-matrix { grid-template-columns: repeat(2, 1fr); }
    }

    .btn-fault-switch {
      background: rgba(239, 68, 68, 0.08);
      border: 1px solid rgba(239, 68, 68, 0.25);
      color: #fca5a5;
      padding: 10px 10px;
      border-radius: 10px;
      font-size: 11px;
      font-weight: 700;
      letter-spacing: 0.5px;
      cursor: pointer;
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 4px;
      transition: all 0.2s;
    }
    .btn-fault-switch:hover {
      background: var(--crimson);
      color: #fff;
      box-shadow: 0 4px 18px var(--crimson-glow);
      transform: translateY(-2px);
    }
    .btn-fault-switch:active {
      transform: translateY(1px);
    }
    .btn-fault-switch .sub-text {
      font-size: 9px;
      font-family: var(--font-mono);
      opacity: 0.8;
    }

    /* Real-Time Sparkline Canvas */
    .sparkline-box {
      margin-top: 16px;
      background: rgba(0, 0, 0, 0.35);
      border: 1px solid rgba(255, 255, 255, 0.05);
      border-radius: 12px;
      padding: 12px 14px;
    }
    .sparkline-header {
      display: flex;
      justify-content: space-between;
      font-size: 10px;
      font-family: var(--font-mono);
      color: var(--text-dim);
      margin-bottom: 6px;
      text-transform: uppercase;
    }
    .sparkline-legend {
      display: flex;
      gap: 12px;
    }
    .spark-leg-item {
      display: flex;
      align-items: center;
      gap: 4px;
    }
    .spark-leg-dot {
      width: 6px; height: 6px; border-radius: 50%;
    }
    canvas#telemetryChart {
      width: 100%;
      height: 75px;
      display: block;
    }

    /* Black-Box Telemetry Event Recorder */
    .recorder-panel {
      display: flex;
      flex-direction: column;
    }
    .recorder-actions {
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .btn-recorder-tool {
      background: rgba(255, 255, 255, 0.05);
      border: 1px solid var(--card-border);
      color: var(--text-muted);
      padding: 3px 8px;
      border-radius: 6px;
      font-size: 10px;
      font-family: var(--font-mono);
      cursor: pointer;
      transition: all 0.2s;
    }
    .btn-recorder-tool:hover {
      background: rgba(255, 255, 255, 0.12);
      color: #fff;
    }

    .log-stream {
      flex: 1;
      background: rgba(0, 0, 0, 0.5);
      border: 1px solid rgba(255, 255, 255, 0.06);
      border-radius: 12px;
      padding: 12px;
      font-family: var(--font-mono);
      font-size: 11px;
      height: 250px;
      overflow-y: auto;
      display: flex;
      flex-direction: column-reverse;
      gap: 6px;
    }
    .log-row {
      display: flex;
      align-items: baseline;
      gap: 8px;
      line-height: 1.4;
      padding-bottom: 4px;
      border-bottom: 1px solid rgba(255, 255, 255, 0.03);
    }
    .log-row-time {
      color: var(--text-dim);
      font-size: 10px;
      white-space: nowrap;
    }
    .log-badge {
      font-size: 9px;
      font-weight: 700;
      padding: 1px 6px;
      border-radius: 4px;
      text-transform: uppercase;
      white-space: nowrap;
    }
    .log-badge.CRITICAL {
      background: var(--crimson-subtle);
      color: #f87171;
      border: 1px solid rgba(239, 68, 68, 0.3);
    }
    .log-badge.WARNING {
      background: var(--amber-subtle);
      color: #fbbf24;
      border: 1px solid rgba(245, 158, 11, 0.3);
    }
    .log-badge.INFO {
      background: var(--cobalt-subtle);
      color: var(--cobalt);
      border: 1px solid rgba(56, 189, 248, 0.3);
    }
    .log-row-msg {
      color: var(--text-main);
      word-break: break-word;
    }

    ::-webkit-scrollbar { width: 6px; height: 6px; }
    ::-webkit-scrollbar-track { background: rgba(0, 0, 0, 0.2); }
    ::-webkit-scrollbar-thumb { background: rgba(255, 255, 255, 0.15); border-radius: 4px; }
    ::-webkit-scrollbar-thumb:hover { background: rgba(255, 255, 255, 0.25); }
  </style>
</head>
<body>

<div class="container">

  <!-- TOP COCKPIT HUD & NAV -->
  <header class="cockpit-header">
    <div class="brand-section">
      <div class="brand-emblem">
        <svg viewBox="0 0 24 24"><path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5"/></svg>
      </div>
      <div class="brand-meta">
        <h1>DRIVESENSE</h1>
        <div class="badge-arch">
          <span>AUTOMOTIVE HMI</span>
          <span>&bull;</span>
          <span class="kernel-tag">/dev/drivesense</span>
          <span>&bull;</span>
          <span id="headerSeq">#0000</span>
        </div>
      </div>
    </div>

    <!-- PRND Transmission Gear Selector -->
    <div class="gear-cluster">
      <button class="gear-btn active" id="gearP">P</button>
      <button class="gear-btn" id="gearR">R</button>
      <button class="gear-btn" id="gearN">N</button>
      <button class="gear-btn" id="gearD">D</button>
    </div>

    <!-- Telemetry Vitals & Audio Controls -->
    <div class="vitals-bar">
      <div class="vital-item">
        <span class="vital-label">DRIVER LINK</span>
        <span class="vital-val">
          <span class="pulse-dot" id="driverPulse"></span>
          <span id="driverStatusTxt">ONLINE</span>
        </span>
      </div>
      <div class="vital-item">
        <span class="vital-label">BUS TICKER</span>
        <span class="vital-val" id="busTickerVal">100 HZ</span>
      </div>
      <button class="audio-toggle-btn" id="audioToggle" onclick="toggleAudio()" title="Toggle Cockpit Audio Chimes">
        <span id="audioIcon">&#128263;</span>
        <span id="audioText">MUTED</span>
      </button>
    </div>
  </header>

  <!-- CRITICAL ALERT STRIP -->
  <div class="alert-strip" id="alertStrip">
    <div class="alert-left">
      <div class="alert-icon">!</div>
      <div>
        <div style="font-weight: 700; letter-spacing: 0.5px;" id="alertTitle">CRITICAL KERNEL INTERRUPT</div>
        <div style="font-size: 11px; opacity: 0.85; font-family: var(--font-mono);" id="alertDesc">Telemetry out of safe operational thresholds.</div>
      </div>
    </div>
    <div class="alert-actions">
      <button class="btn-quick-reset" onclick="sendCmd('reset')">RESET ALL FAULTS</button>
    </div>
  </div>

  <!-- THREE-WING PANORAMIC CLUSTER -->
  <div class="cluster-grid">

    <!-- LEFT WING: POWERTRAIN & THERMAL -->
    <div class="panel">
      <div class="panel-header">
        <div class="panel-title">
          <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-6h2v6zm0-8h-2V7h2v2z"/></svg>
          Powertrain Core
        </div>
        <div class="panel-tag">DIAGNOSTICS</div>
      </div>

      <!-- Fuel Level Meter -->
      <div class="gauge-block">
        <div class="gauge-block-top">
          <span class="gauge-name">Fuel Capacity</span>
          <span class="gauge-val-big" id="fuelNum">--<small>%</small></span>
        </div>
        <div class="linear-bar-wrap">
          <div class="linear-bar-fill fill-fuel-nom" id="fuelBar"></div>
        </div>
        <div class="gauge-footer-meta">
          <span>RESERVE: <span id="fuelReserveTag" style="color: var(--emerald);">NOMINAL</span></span>
          <span>EST. RANGE: <span id="fuelRangeTag">-- KM</span></span>
        </div>
      </div>

      <!-- Coolant Temperature Meter -->
      <div class="gauge-block">
        <div class="gauge-block-top">
          <span class="gauge-name">Coolant Temperature</span>
          <span class="gauge-val-big" id="tempNum">--<small>&deg;C</small></span>
        </div>
        <div class="linear-bar-wrap">
          <div class="linear-bar-fill fill-temp-nom" id="tempBar"></div>
        </div>
        <div class="gauge-footer-meta">
          <span>OPTIMAL: 85&ndash;95 &deg;C</span>
          <span id="tempStateTag" style="color: var(--emerald);">NORMAL</span>
        </div>
      </div>

      <!-- Kernel Driver Statistics Table -->
      <div class="kernel-stats-card">
        <div class="kernel-stats-title">
          <span>Kernel Counters</span>
          <span>/proc/drivesense</span>
        </div>
        <div class="kernel-stats-grid">
          <div class="stat-box">
            <span class="k-label">SYSCALL READS</span>
            <span class="k-num" id="statReads">0</span>
          </div>
          <div class="stat-box">
            <span class="k-label">IOCTL CMDS</span>
            <span class="k-num" id="statIoctls">0</span>
          </div>
          <div class="stat-box">
            <span class="k-label">TIMER UPDATES</span>
            <span class="k-num" id="statUpdates">0</span>
          </div>
          <div class="stat-box">
            <span class="k-label">ACTIVE FAULTS</span>
            <span class="k-num" id="statFaults" style="color: #fff;">0</span>
          </div>
        </div>
      </div>

    </div>

    <!-- CENTER WING: PANORAMIC SPEEDOMETER -->
    <div class="panel speedo-panel">
      <div class="panel-header" style="width: 100%;">
        <div class="panel-title">
          <svg viewBox="0 0 24 24"><path d="M12 4a8 8 0 00-8 8c0 2.21.89 4.21 2.34 5.66l1.41-1.41A6 6 0 016 12a6 6 0 0111.46-2.46l1.86-.75A8 8 0 0012 4z"/></svg>
          Digital Cluster Instrument
        </div>
        <div class="panel-tag" id="faultBadge" style="color: var(--text-dim); font-weight: 700;">FAULT: NONE</div>
      </div>

      <div class="speedo-stage">
        <svg class="speedo-svg" viewBox="0 0 320 320">
          <defs>
            <linearGradient id="speedoGrad" x1="0%" y1="100%" x2="100%" y2="0%">
              <stop offset="0%" stop-color="#38bdf8" />
              <stop offset="65%" stop-color="#0284c7" />
              <stop offset="100%" stop-color="#ef4444" />
            </linearGradient>
            <filter id="glowFilter" x="-20%" y="-20%" width="140%" height="140%">
              <feGaussianBlur stdDeviation="6" result="blur" />
              <feComposite in="SourceGraphic" in2="blur" operator="over" />
            </filter>
          </defs>

          <!-- Outer tick track background -->
          <circle cx="160" cy="160" r="132" fill="none" stroke="rgba(255, 255, 255, 0.04)" stroke-width="1" />
          <circle cx="160" cy="160" r="102" fill="none" stroke="rgba(255, 255, 255, 0.04)" stroke-width="1" />

          <!-- Background Gauge Arc (240 degrees: from 150 deg to 390 deg) -->
          <circle cx="160" cy="160" r="118" fill="none"
                  stroke="rgba(255, 255, 255, 0.06)"
                  stroke-width="14"
                  stroke-linecap="round"
                  stroke-dasharray="494.3 741.4"
                  stroke-dashoffset="0"
                  transform="rotate(150 160 160)" />

          <!-- Active Velocity Arc Sweep -->
          <circle id="speedArc" cx="160" cy="160" r="118" fill="none"
                  stroke="url(#speedoGrad)"
                  stroke-width="14"
                  stroke-linecap="round"
                  stroke-dasharray="494.3 741.4"
                  stroke-dashoffset="494.3"
                  filter="url(#glowFilter)"
                  transform="rotate(150 160 160)"
                  style="transition: stroke-dashoffset 0.3s cubic-bezier(0.2, 0, 0, 1);" />

          <g id="tickGroup"></g>

          <!-- Rotating Velocity Needle Pointer -->
          <g id="needleGroup" transform="rotate(-120 160 160)" style="transition: transform 0.3s cubic-bezier(0.2, 0, 0, 1);">
            <line x1="160" y1="160" x2="160" y2="46" stroke="#38bdf8" stroke-width="2.5" stroke-linecap="round" filter="drop-shadow(0 0 6px #38bdf8)" />
            <circle cx="160" cy="46" r="3.5" fill="#ffffff" />
            <circle cx="160" cy="160" r="8" fill="#1e293b" stroke="#38bdf8" stroke-width="2" />
          </g>
        </svg>

        <div class="speedo-center">
          <div class="speedo-display-val" id="speedDigital">0</div>
          <div class="speedo-unit">KM / H</div>
          <div class="drive-mode-pill" id="driveModePill">IDLE</div>
        </div>
      </div>

      <div class="speedo-bottom-telemetry">
        <div class="tele-cell">
          <span class="t-lbl">ODOMETER</span>
          <span class="t-val" id="tripDist">0.0 KM</span>
        </div>
        <div class="tele-cell">
          <span class="t-lbl">SPEED LIMIT</span>
          <span class="t-val" style="color: var(--amber);">120 KM/H</span>
        </div>
        <div class="tele-cell">
          <span class="t-lbl">AVG SPEED</span>
          <span class="t-val" id="avgSpeed">0 KM/H</span>
        </div>
      </div>
    </div>

    <!-- RIGHT WING: CHASSIS & TPMS -->
    <div class="panel">
      <div class="panel-header">
        <div class="panel-title">
          <svg viewBox="0 0 24 24"><path d="M18.92 6.01C18.72 5.42 18.16 5 17.5 5h-11c-.66 0-1.21.42-1.42 1.01L3 12v8c0 .55.45 1 1 1h1c.55 0 1-.45 1-1v-1h12v1c0 .55.45 1 1 1h1c.55 0 1-.45 1-1v-8l-2.08-5.99zM6.5 16c-.83 0-1.5-.67-1.5-1.5S5.67 13 6.5 13s1.5.67 1.5 1.5S7.33 16 6.5 16zm11 0c-.83 0-1.5-.67-1.5-1.5s.67-1.5 1.5-1.5 1.5.67 1.5 1.5-.67 1.5-1.5 1.5zM5 11l1.5-4.5h11L19 11H5z"/></svg>
          Chassis Dynamics
        </div>
        <div class="panel-tag" id="tpmsGlobalTag" style="color: var(--emerald); font-weight: 700;">TPMS: NOMINAL</div>
      </div>

      <!-- Top-Down Car Silhouette & Sensor Points -->
      <div class="tpms-chassis-stage">
        <svg class="car-silhouette" viewBox="0 0 100 190">
          <defs>
            <linearGradient id="carGrad" x1="0%" y1="0%" x2="100%" y2="100%">
              <stop offset="0%" stop-color="#1e293b" />
              <stop offset="100%" stop-color="#0f172a" />
            </linearGradient>
          </defs>
          <path d="M28 10 C38 4, 62 4, 72 10 C82 17, 85 45, 85 70 C88 95, 88 135, 84 165 C82 180, 68 186, 50 186 C32 186, 18 180, 16 165 C12 135, 12 95, 15 70 C15 45, 18 17, 28 10 Z"
                fill="url(#carGrad)" stroke="rgba(255, 255, 255, 0.2)" stroke-width="1.8" />
          <path d="M26 50 C38 45, 62 45, 74 50 L70 78 C58 75, 42 75, 30 78 Z"
                fill="rgba(56, 189, 248, 0.15)" stroke="rgba(56, 189, 248, 0.4)" stroke-width="1" />
          <path d="M30 125 C42 128, 58 128, 70 125 L73 145 C60 148, 40 148, 27 145 Z"
                fill="rgba(56, 189, 248, 0.1)" stroke="rgba(56, 189, 248, 0.3)" stroke-width="1" />
          <path d="M30 78 L70 78 L70 125 L30 125 Z" fill="none" stroke="rgba(255, 255, 255, 0.1)" stroke-width="1" />
          <rect id="wMeshFL" x="6" y="32" width="10" height="26" rx="4" fill="#38bdf8" opacity="0.8" />
          <rect id="wMeshFR" x="84" y="32" width="10" height="26" rx="4" fill="#38bdf8" opacity="0.8" />
          <rect id="wMeshRL" x="6" y="132" width="10" height="26" rx="4" fill="#38bdf8" opacity="0.8" />
          <rect id="wMeshRR" x="84" y="132" width="10" height="26" rx="4" fill="#38bdf8" opacity="0.8" />
        </svg>
      </div>

      <div class="tpms-grid">
        <div class="wheel-card" id="cardFL">
          <span class="wheel-pos">Front Left</span>
          <span class="wheel-psi" id="psiFL">--<small style="font-size: 11px;"> PSI</small></span>
          <span class="wheel-status" id="statFL">NOMINAL</span>
        </div>
        <div class="wheel-card" id="cardFR">
          <span class="wheel-pos">Front Right</span>
          <span class="wheel-psi" id="psiFR">--<small style="font-size: 11px;"> PSI</small></span>
          <span class="wheel-status" id="statFR">NOMINAL</span>
        </div>
        <div class="wheel-card" id="cardRL">
          <span class="wheel-pos">Rear Left</span>
          <span class="wheel-psi" id="psiRL">--<small style="font-size: 11px;"> PSI</small></span>
          <span class="wheel-status" id="statRL">NOMINAL</span>
        </div>
        <div class="wheel-card" id="cardRR">
          <span class="wheel-pos">Rear Right</span>
          <span class="wheel-psi" id="psiRR">--<small style="font-size: 11px;"> PSI</small></span>
          <span class="wheel-status" id="statRR">NOMINAL</span>
        </div>
      </div>

      <div class="chassis-health-bar">
        <div class="chassis-row">
          <span>ABS / STABILITY</span>
          <span class="chassis-badge">ENGAGED</span>
        </div>
        <div class="chassis-row">
          <span>BRAKE THERMALS</span>
          <span class="chassis-badge">NORMAL</span>
        </div>
        <div class="chassis-row">
          <span>PRESSURE THRESHOLD</span>
          <span style="color: var(--amber);">&lt; 26.0 PSI</span>
        </div>
      </div>

    </div>

  </div>

  <!-- BOTTOM DECK: SWITCHBOARD & BLACK-BOX RECORDER -->
  <div class="deck-grid">

    <!-- COMMAND & FAULT SWITCHBOARD -->
    <div class="panel">
      <div class="panel-header">
        <div class="panel-title">
          <svg viewBox="0 0 24 24"><path d="M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm-5 14H7v-2h7v2zm3-4H7v-2h10v2zm0-4H7V7h10v2z"/></svg>
          Command Console &amp; Test Matrix
        </div>
        <div class="panel-tag">IOCTL INTERFACE</div>
      </div>

      <!-- State Transitions -->
      <div class="switchboard-section-title">
        <span>POWERTRAIN STATE TRANSITIONS</span>
        <span style="font-size: 9px; color: var(--text-dim);">KEY SHORTCUTS [S] [P] [R]</span>
      </div>
      <div class="btn-row">
        <button class="cockpit-btn btn-drive-start" onclick="sendCmd('start')">
          <span>&#9654; START ENGINE</span>
          <span class="k-badge">S</span>
        </button>
        <button class="cockpit-btn btn-drive-stop" onclick="sendCmd('stop')">
          <span>&#9632; STOP / IDLE</span>
          <span class="k-badge">P</span>
        </button>
        <button class="cockpit-btn btn-drive-reset" onclick="sendCmd('reset')">
          <span>&#8635; RESET ALL</span>
          <span class="k-badge">R</span>
        </button>
      </div>

      <!-- Fault Injection Switches -->
      <div class="switchboard-section-title">
        <span>SAFETY FAULT INJECTION MATRIX (DS_IOC_INJECT_FAULT)</span>
        <span style="font-size: 9px; color: var(--text-dim);">KEYS [1] [2] [3] [4]</span>
      </div>
      <div class="fault-matrix">
        <button class="btn-fault-switch" onclick="sendCmd('fault', 'overheat')">
          <span>&#128293; OVERHEAT</span>
          <span class="sub-text">118 &deg;C [1]</span>
        </button>
        <button class="btn-fault-switch" onclick="sendCmd('fault', 'lowfuel')">
          <span>&#9981; LOW FUEL</span>
          <span class="sub-text">8% [2]</span>
        </button>
        <button class="btn-fault-switch" onclick="sendCmd('fault', 'flattyre')">
          <span>&#128065; FLAT TYRE</span>
          <span class="sub-text">18 PSI [3]</span>
        </button>
        <button class="btn-fault-switch" onclick="sendCmd('fault', 'overspeed')">
          <span>&#9889; OVERSPEED</span>
          <span class="sub-text">140 KM/H [4]</span>
        </button>
      </div>

      <!-- Real-Time Telemetry Sparkline -->
      <div class="sparkline-box">
        <div class="sparkline-header">
          <span>Live Oscilloscope (Last 40s)</span>
          <div class="sparkline-legend">
            <div class="spark-leg-item">
              <span class="spark-leg-dot" style="background: var(--cobalt);"></span>
              <span>Speed</span>
            </div>
            <div class="spark-leg-item">
              <span class="spark-leg-dot" style="background: var(--amber);"></span>
              <span>Temp</span>
            </div>
          </div>
        </div>
        <canvas id="telemetryChart" width="500" height="75"></canvas>
      </div>
    </div>

    <!-- BLACK-BOX EVENT & ALERT STREAM -->
    <div class="panel recorder-panel">
      <div class="panel-header">
        <div class="panel-title">
          <svg viewBox="0 0 24 24"><path d="M14 2H6c-1.1 0-1.99.9-1.99 2L4 20c0 1.1.89 2 1.99 2H18c1.1 0 2-.9 2-2V8l-6-6zm2 16H8v-2h8v2zm0-4H8v-2h8v2zm-3-5V3.5L18.5 9H13z"/></svg>
          Black-Box Telemetry Log
        </div>
        <div class="recorder-actions">
          <span class="panel-tag" id="logCountTag">0 ENTRIES</span>
          <button class="btn-recorder-tool" onclick="clearLogs()">CLEAR</button>
        </div>
      </div>

      <div class="log-stream" id="logStream">
      </div>
    </div>

  </div>

</div>

<!-- SCRIPT LOGIC -->
<script>
  let audioCtx = null;
  let audioEnabled = false;

  function initAudio() {
    if (!audioCtx) {
      audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    }
    if (audioCtx.state === 'suspended') {
      audioCtx.resume();
    }
  }

  function toggleAudio() {
    initAudio();
    audioEnabled = !audioEnabled;
    const btn = document.getElementById('audioToggle');
    const icon = document.getElementById('audioIcon');
    const text = document.getElementById('audioText');
    if (audioEnabled) {
      btn.classList.add('on');
      icon.innerHTML = '&#128266;';
      text.innerText = 'AUDIO ON';
      playTone(580, 0.08, 'sine');
    } else {
      btn.classList.remove('on');
      icon.innerHTML = '&#128263;';
      text.innerText = 'MUTED';
    }
  }

  function playTone(freq, dur, type = 'sine') {
    if (!audioEnabled || !audioCtx) return;
    try {
      const osc = audioCtx.createOscillator();
      const gain = audioCtx.createGain();
      osc.type = type;
      osc.frequency.setValueAtTime(freq, audioCtx.currentTime);
      gain.gain.setValueAtTime(0.08, audioCtx.currentTime);
      gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + dur);
      osc.connect(gain);
      gain.connect(audioCtx.destination);
      osc.start();
      osc.stop(audioCtx.currentTime + dur);
    } catch(e) {}
  }

  function playAlertChime() {
    if (!audioEnabled || !audioCtx) return;
    playTone(880, 0.12, 'triangle');
    setTimeout(() => playTone(660, 0.2, 'triangle'), 140);
  }

  const MAX_SPEED = 200;
  const ARC_LENGTH = 494.3;
  const START_ANGLE = 150;
  const SWEEP_ANGLE = 240;

  function initSpeedoTicks() {
    const tickGroup = document.getElementById('tickGroup');
    tickGroup.innerHTML = '';
    
    for (let speed = 0; speed <= MAX_SPEED; speed += 10) {
      const fraction = speed / MAX_SPEED;
      const angleDeg = START_ANGLE + fraction * SWEEP_ANGLE;
      const angleRad = (angleDeg * Math.PI) / 180;
      
      const isMajor = (speed % 20 === 0);
      const isRedline = (speed > 120);

      const rOuter = 132;
      const rInner = isMajor ? 120 : 125;

      const x1 = 160 + rOuter * Math.cos(angleRad);
      const y1 = 160 + rOuter * Math.sin(angleRad);
      const x2 = 160 + rInner * Math.cos(angleRad);
      const y2 = 160 + rInner * Math.sin(angleRad);

      const line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      line.setAttribute('x1', x1);
      line.setAttribute('y1', y1);
      line.setAttribute('x2', x2);
      line.setAttribute('y2', y2);
      line.setAttribute('stroke', isRedline ? '#ef4444' : (isMajor ? '#94a3b8' : 'rgba(255,255,255,0.2)'));
      line.setAttribute('stroke-width', isMajor ? (isRedline ? '2.5' : '2') : '1');
      tickGroup.appendChild(line);

      if (isMajor) {
        const rText = 96;
        const tx = 160 + rText * Math.cos(angleRad);
        const ty = 160 + rText * Math.sin(angleRad);
        const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        text.setAttribute('x', tx);
        text.setAttribute('y', ty + 4);
        text.setAttribute('fill', isRedline ? '#f87171' : '#64748b');
        text.setAttribute('font-size', '10');
        text.setAttribute('font-weight', '600');
        text.setAttribute('font-family', 'JetBrains Mono, monospace');
        text.setAttribute('text-anchor', 'middle');
        text.textContent = speed;
        tickGroup.appendChild(text);
      }
    }
  }
  initSpeedoTicks();

  function renderSpeedo(speed) {
    const clamped = Math.min(Math.max(speed, 0), MAX_SPEED);
    const fraction = clamped / MAX_SPEED;

    const digi = document.getElementById('speedDigital');
    digi.innerText = Math.round(clamped);
    if (clamped > 120) {
      digi.classList.add('over-limit');
    } else {
      digi.classList.remove('over-limit');
    }

    const offset = ARC_LENGTH - (fraction * ARC_LENGTH);
    document.getElementById('speedArc').style.strokeDashoffset = offset;

    const needleAngle = -120 + (fraction * 240);
    document.getElementById('needleGroup').setAttribute('transform', `rotate(${needleAngle} 160 160)`);
  }

  const canvas = document.getElementById('telemetryChart');
  const ctx = canvas.getContext('2d');
  const speedHistory = new Array(40).fill(0);
  const tempHistory = new Array(40).fill(90);

  function drawSparkline() {
    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);

    ctx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
    ctx.lineWidth = 1;
    [0.25, 0.5, 0.75].forEach(ratio => {
      ctx.beginPath();
      ctx.moveTo(0, h * ratio);
      ctx.lineTo(w, h * ratio);
      ctx.stroke();
    });

    const step = w / (speedHistory.length - 1);

    ctx.beginPath();
    ctx.strokeStyle = '#f59e0b';
    ctx.lineWidth = 1.8;
    for (let i = 0; i < tempHistory.length; i++) {
      const val = tempHistory[i];
      const norm = Math.min(Math.max((val - 40) / 90, 0), 1);
      const y = h - (norm * (h - 8)) - 4;
      const x = i * step;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();

    ctx.beginPath();
    ctx.strokeStyle = '#38bdf8';
    ctx.lineWidth = 2.2;
    for (let i = 0; i < speedHistory.length; i++) {
      const val = speedHistory[i];
      const norm = Math.min(Math.max(val / MAX_SPEED, 0), 1);
      const y = h - (norm * (h - 8)) - 4;
      const x = i * step;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
  }

  let logTotalCount = 0;
  function addLog(level, msg) {
    const stream = document.getElementById('logStream');
    const time = new Date().toTimeString().split(' ')[0];
    const row = document.createElement('div');
    row.className = 'log-row';
    row.innerHTML = `
      <span class="log-row-time">[${time}]</span>
      <span class="log-badge ${level}">${level}</span>
      <span class="log-row-msg">${msg}</span>
    `;
    stream.prepend(row);
    logTotalCount++;
    document.getElementById('logCountTag').innerText = logTotalCount + ' ENTRIES';

    while (stream.children.length > 50) {
      stream.removeChild(stream.lastChild);
    }
  }

  function clearLogs() {
    document.getElementById('logStream').innerHTML = '';
    logTotalCount = 0;
    document.getElementById('logCountTag').innerText = '0 ENTRIES';
  }

  function updateGear(state) {
    ['P', 'R', 'N', 'D'].forEach(g => {
      const el = document.getElementById('gear' + g);
      el.classList.remove('active', 'active-fault');
    });

    if (state === 'DRIVING') {
      document.getElementById('gearD').classList.add('active');
    } else if (state === 'FAULT') {
      document.getElementById('gearP').classList.add('active-fault');
    } else {
      document.getElementById('gearP').classList.add('active');
    }
  }

  let lastFault = "NONE";
  let lastState = "";
  let odometerKm = 0.0;
  let totalSpeedSamples = 0;
  let speedSum = 0;
  let lastTimeMs = Date.now();

  async function fetchTelemetry() {
    try {
      const res = await fetch('/api/telemetry');
      if (!res.ok) throw new Error('HTTP ' + res.status);
      const data = await res.json();

      document.getElementById('driverPulse').className = 'pulse-dot';
      document.getElementById('driverStatusTxt').innerText = 'ONLINE';

      renderSpeedo(data.speed_kmh);

      const now = Date.now();
      const dtSec = (now - lastTimeMs) / 1000;
      lastTimeMs = now;
      if (data.speed_kmh > 0) {
        odometerKm += (data.speed_kmh * (dtSec / 3600));
        document.getElementById('tripDist').innerText = odometerKm.toFixed(1) + ' KM';
        totalSpeedSamples++;
        speedSum += data.speed_kmh;
        document.getElementById('avgSpeed').innerText = Math.round(speedSum / totalSpeedSamples) + ' KM/H';
      }

      const pill = document.getElementById('driveModePill');
      pill.innerText = data.state;
      pill.className = 'drive-mode-pill mode-' + data.state;

      const fBadge = document.getElementById('faultBadge');
      fBadge.innerText = 'FAULT: ' + data.active_fault;
      if (data.active_fault !== 'NONE') {
        fBadge.style.color = 'var(--crimson)';
      } else {
        fBadge.style.color = 'var(--text-dim)';
      }

      updateGear(data.state);

      const fuelPct = Math.round(data.fuel_pct);
      document.getElementById('fuelNum').innerHTML = fuelPct + '<small>%</small>';
      const fuelBar = document.getElementById('fuelBar');
      fuelBar.style.width = Math.min(Math.max(fuelPct, 0), 100) + '%';
      if (fuelPct < 15) {
        fuelBar.className = 'linear-bar-fill fill-fuel-low';
        document.getElementById('fuelReserveTag').innerText = 'CRITICAL';
        document.getElementById('fuelReserveTag').style.color = 'var(--crimson)';
      } else {
        fuelBar.className = 'linear-bar-fill fill-fuel-nom';
        document.getElementById('fuelReserveTag').innerText = 'NOMINAL';
        document.getElementById('fuelReserveTag').style.color = 'var(--emerald)';
      }
      document.getElementById('fuelRangeTag').innerText = Math.round(fuelPct * 6.5) + ' KM';

      const tempC = Math.round(data.engine_temp_c);
      document.getElementById('tempNum').innerHTML = tempC + '<small>&deg;C</small>';
      const tempBar = document.getElementById('tempBar');
      const tempFillPct = Math.min(Math.max((tempC - 20) / 100 * 100, 0), 100);
      tempBar.style.width = tempFillPct + '%';
      const tempTag = document.getElementById('tempStateTag');
      if (tempC > 105) {
        tempBar.className = 'linear-bar-fill fill-temp-hot';
        tempTag.innerText = 'OVERHEATING';
        tempTag.style.color = 'var(--crimson)';
      } else if (tempC > 95) {
        tempBar.className = 'linear-bar-fill fill-temp-hot';
        tempTag.innerText = 'ELEVATED';
        tempTag.style.color = 'var(--amber)';
      } else {
        tempBar.className = 'linear-bar-fill fill-temp-nom';
        tempTag.innerText = 'STABLE';
        tempTag.style.color = 'var(--emerald)';
      }

      const psi = data.tyre_psi;
      const isFlat = (psi < 26.0);
      ['FL', 'FR', 'RL', 'RR'].forEach(id => {
        document.getElementById('psi' + id).innerHTML = psi.toFixed(1) + '<small style="font-size: 11px;"> PSI</small>';
        const card = document.getElementById('card' + id);
        const stat = document.getElementById('stat' + id);
        const mesh = document.getElementById('wMesh' + id);
        if (isFlat) {
          card.classList.add('punctured');
          stat.innerText = 'DEPRESSURIZED';
          if (mesh) mesh.setAttribute('fill', '#ef4444');
        } else {
          card.classList.remove('punctured');
          stat.innerText = 'NOMINAL';
          if (mesh) mesh.setAttribute('fill', '#38bdf8');
        }
      });
      const tpmsGlobal = document.getElementById('tpmsGlobalTag');
      if (isFlat) {
        tpmsGlobal.innerText = 'TPMS: LOW PRESSURE';
        tpmsGlobal.style.color = 'var(--crimson)';
      } else {
        tpmsGlobal.innerText = 'TPMS: NOMINAL';
        tpmsGlobal.style.color = 'var(--emerald)';
      }

      document.getElementById('headerSeq').innerText = '#' + String(data.sequence).padStart(4, '0');
      if (data.stats) {
        document.getElementById('statReads').innerText = data.stats.reads.toLocaleString();
        document.getElementById('statIoctls').innerText = data.stats.ioctls.toLocaleString();
        document.getElementById('statUpdates').innerText = data.stats.updates.toLocaleString();
        document.getElementById('statFaults').innerText = data.stats.faults_injected;
      }

      const alertStrip = document.getElementById('alertStrip');
      const alertTitle = document.getElementById('alertTitle');
      const alertDesc = document.getElementById('alertDesc');

      if (data.active_fault !== 'NONE') {
        alertStrip.className = 'alert-strip show';
        alertTitle.innerText = 'CRITICAL KERNEL FAULT: ' + data.active_fault;
        alertDesc.innerText = 'Fault injected in kernel-space driver /dev/drivesense. Engine derated.';
      } else if (data.speed_kmh > 120) {
        alertStrip.className = 'alert-strip show warn';
        alertTitle.innerText = 'SPEED LIMIT EXCEEDED';
        alertDesc.innerText = 'Vehicle speed (' + Math.round(data.speed_kmh) + ' km/h) breached safety velocity ceiling.';
      } else if (tempC > 105) {
        alertStrip.className = 'alert-strip show';
        alertTitle.innerText = 'ENGINE COOLANT OVERHEATING';
        alertDesc.innerText = 'Thermal core measured ' + tempC + ' °C. Risk of catastrophic block warping.';
      } else if (fuelPct < 10) {
        alertStrip.className = 'alert-strip show warn';
        alertTitle.innerText = 'LOW FUEL RESERVE';
        alertDesc.innerText = 'Fuel reserve critically low (' + fuelPct + '%). Refuel immediately.';
      } else if (isFlat) {
        alertStrip.className = 'alert-strip show';
        alertTitle.innerText = 'TIRE PRESSURE CRITICAL';
        alertDesc.innerText = 'Pressure measured at ' + psi.toFixed(1) + ' PSI. Loss of traction imminent.';
      } else {
        alertStrip.className = 'alert-strip';
      }

      if (data.state !== lastState && lastState !== "") {
        addLog('INFO', `Vehicle state transitioned: ${lastState} -> ${data.state}`);
      }
      lastState = data.state;

      if (data.active_fault !== lastFault && lastFault !== "") {
        if (data.active_fault !== "NONE") {
          addLog('CRITICAL', `Kernel Fault Injected: ${data.active_fault}`);
          playAlertChime();
        } else {
          addLog('INFO', 'System restored to baseline. All safety faults cleared.');
        }
      }
      lastFault = data.active_fault;

      if (data.new_alerts && data.new_alerts.length > 0) {
        data.new_alerts.forEach(a => {
          addLog(a.level, `[${a.sensor}] ${a.message}`);
          if (a.level === 'CRITICAL') playAlertChime();
        });
      }

      speedHistory.shift();
      speedHistory.push(data.speed_kmh);
      tempHistory.shift();
      tempHistory.push(data.engine_temp_c);
      drawSparkline();

    } catch (err) {
      document.getElementById('driverPulse').className = 'pulse-dot danger';
      document.getElementById('driverStatusTxt').innerText = 'OFFLINE';
    }
  }

  async function sendCmd(action, type = '') {
    initAudio();
    playTone(750, 0.05, 'square');
    try {
      let url = '/api/control?action=' + encodeURIComponent(action);
      if (type) url += '&type=' + encodeURIComponent(type);
      const res = await fetch(url, { method: 'POST' });
      const json = await res.json();
      addLog('INFO', `IOCTL [${action}${type ? ':' + type : ''}]: ${json.message || 'OK'}`);
      setTimeout(fetchTelemetry, 60);
    } catch (err) {
      addLog('CRITICAL', 'IOCTL dispatch failed: ' + err);
    }
  }

  window.addEventListener('keydown', (e) => {
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
    const key = e.key.toUpperCase();
    if (key === 'S') sendCmd('start');
    else if (key === 'P') sendCmd('stop');
    else if (key === 'R') sendCmd('reset');
    else if (key === '1') sendCmd('fault', 'overheat');
    else if (key === '2') sendCmd('fault', 'lowfuel');
    else if (key === '3') sendCmd('fault', 'flattyre');
    else if (key === '4') sendCmd('fault', 'overspeed');
  });

  addLog('INFO', 'DriveSense HMI Instrument Cluster initialized. Connected to /dev/drivesense.');
  setInterval(fetchTelemetry, 150);
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
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
                    reply_msg = "Vehicle Engine Engaged (DS_IOC_START)";
                } else if (action == "stop") {
                    device.stop();
                    reply_msg = "Vehicle Engine Disengaged (DS_IOC_STOP)";
                } else if (action == "reset") {
                    device.reset();
                    alert_mgr.clear();
                    reply_msg = "All Faults Cleared & Telemetry Reset (DS_IOC_RESET)";
                } else if (action == "fault") {
                    if (type == "overheat") {
                        device.injectFault(DS_FAULT_OVERHEAT);
                        reply_msg = "Fault Injected: Engine Thermal Overheat (118 °C)";
                    } else if (type == "lowfuel") {
                        device.injectFault(DS_FAULT_LOW_FUEL);
                        reply_msg = "Fault Injected: Fuel Reserve Critical (8 %)";
                    } else if (type == "flattyre") {
                        device.injectFault(DS_FAULT_FLAT_TYRE);
                        reply_msg = "Fault Injected: TPMS Tire Depressurization (18 PSI)";
                    } else if (type == "overspeed") {
                        device.injectFault(DS_FAULT_OVERSPEED);
                        reply_msg = "Fault Injected: Speed Limit Breach (140 km/h)";
                    } else {
                        success = false;
                        reply_msg = "Unknown fault injection parameter";
                    }
                } else {
                    success = false;
                    reply_msg = "Unknown IOCTL action";
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
              << "   DriveSense HMI Automotive Telemetry Web Server       \n"
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
