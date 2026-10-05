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
      /* Matte Obsidian & Luxury Graphite Surfaces */
      --bg-base: #090b0f;
      --bg-gradient: radial-gradient(circle at 50% 0%, #11141c 0%, #090b0f 85%);
      --card-bg: rgba(14, 18, 25, 0.72);
      --card-elevated: rgba(19, 24, 34, 0.85);
      --card-border: rgba(255, 255, 255, 0.05);
      --card-border-light: rgba(255, 255, 255, 0.09);
      --card-inner-glow: inset 0 1px 0 rgba(255, 255, 255, 0.04);

      /* Refined, Subtle Automotive Accents (No harsh neon) */
      --accent-ice: #93c5fd;
      --accent-slate: #64748b;
      --accent-steel: #475569;
      --accent-subtle: rgba(147, 197, 253, 0.05);

      /* Semantic Status - Controlled, Muted & Matte */
      --status-sage: #6ee7b7;
      --status-sage-bg: rgba(110, 231, 183, 0.07);
      --status-sage-border: rgba(110, 231, 183, 0.2);

      --status-ochre: #fbbf24;
      --status-ochre-bg: rgba(251, 191, 36, 0.07);
      --status-ochre-border: rgba(251, 191, 36, 0.2);

      --status-rose: #f87171;
      --status-rose-bg: rgba(248, 113, 113, 0.08);
      --status-rose-border: rgba(248, 113, 113, 0.25);

      /* Restrained Neutral Typography */
      --text-main: #f1f5f9;
      --text-secondary: #cbd5e1;
      --text-muted: #94a3b8;
      --text-dim: #64748b;
      --text-faint: #334155;

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

    /* Architectural hairline grid */
    body::before {
      content: "";
      position: fixed;
      top: 0; left: 0; width: 100%; height: 100%;
      background-image: 
        linear-gradient(rgba(255, 255, 255, 0.012) 1px, transparent 1px),
        linear-gradient(90deg, rgba(255, 255, 255, 0.012) 1px, transparent 1px);
      background-size: 48px 48px;
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
      gap: 16px;
    }

    /* =========================================================
       TOP COCKPIT HUD
       ========================================================= */
    .cockpit-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      background: var(--card-bg);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      border: 1px solid var(--card-border);
      box-shadow: var(--card-inner-glow), 0 8px 24px rgba(0, 0, 0, 0.35);
      border-radius: 14px;
      padding: 12px 22px;
    }

    .brand-section {
      display: flex;
      align-items: center;
      gap: 12px;
    }

    .brand-emblem {
      width: 36px;
      height: 36px;
      background: rgba(255, 255, 255, 0.04);
      border: 1px solid var(--card-border-light);
      border-radius: 8px;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .brand-emblem svg { width: 18px; height: 18px; fill: var(--text-secondary); }

    .brand-meta h1 {
      font-size: 18px;
      font-weight: 700;
      letter-spacing: 1.5px;
      color: #ffffff;
      line-height: 1.1;
    }
    .brand-meta .badge-arch {
      font-family: var(--font-mono);
      font-size: 10px;
      font-weight: 500;
      letter-spacing: 1px;
      color: var(--text-dim);
      text-transform: uppercase;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .badge-arch span.kernel-tag {
      color: var(--text-secondary);
      background: rgba(255, 255, 255, 0.04);
      padding: 1px 6px;
      border-radius: 4px;
      border: 1px solid rgba(255, 255, 255, 0.08);
    }

    /* Gear Selector PRND - Sleek Minimalist */
    .gear-cluster {
      display: flex;
      align-items: center;
      gap: 4px;
      background: rgba(0, 0, 0, 0.3);
      padding: 3px 6px;
      border-radius: 8px;
      border: 1px solid var(--card-border);
    }
    .gear-btn {
      font-family: var(--font-mono);
      font-weight: 600;
      font-size: 13px;
      padding: 5px 12px;
      border-radius: 5px;
      color: var(--text-dim);
      background: transparent;
      border: 1px solid transparent;
      transition: all 0.2s ease;
    }
    .gear-btn.active {
      color: #ffffff;
      background: rgba(255, 255, 255, 0.08);
      border-color: rgba(255, 255, 255, 0.18);
    }
    .gear-btn.active-fault {
      color: #fca5a5;
      background: var(--status-rose-bg);
      border-color: var(--status-rose-border);
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
      font-size: 9px;
      font-weight: 600;
      letter-spacing: 1px;
      text-transform: uppercase;
      color: var(--text-dim);
      font-family: var(--font-mono);
    }
    .vital-val {
      font-size: 12px;
      font-weight: 600;
      font-family: var(--font-mono);
      color: var(--text-secondary);
      display: flex;
      align-items: center;
      gap: 6px;
    }
    .pulse-dot {
      width: 7px;
      height: 7px;
      border-radius: 50%;
      background: var(--status-sage);
      opacity: 0.85;
    }
    .pulse-dot.danger {
      background: var(--status-rose);
    }

    .audio-toggle-btn {
      background: rgba(255, 255, 255, 0.03);
      border: 1px solid var(--card-border);
      color: var(--text-muted);
      border-radius: 6px;
      padding: 6px 9px;
      cursor: pointer;
      display: flex;
      align-items: center;
      gap: 6px;
      font-size: 11px;
      font-family: var(--font-mono);
      transition: all 0.2s ease;
    }
    .audio-toggle-btn:hover {
      background: rgba(255, 255, 255, 0.07);
      color: #fff;
    }
    .audio-toggle-btn.on {
      color: var(--text-main);
      border-color: rgba(255, 255, 255, 0.16);
      background: rgba(255, 255, 255, 0.06);
    }

    /* =========================================================
       SUBTLE ALERT BANNER (No blinding neon)
       ========================================================= */
    .alert-strip {
      display: none;
      align-items: center;
      justify-content: space-between;
      padding: 10px 18px;
      border-radius: 10px;
      font-size: 12px;
      font-weight: 500;
      background: rgba(28, 18, 20, 0.85);
      border: 1px solid var(--status-rose-border);
      backdrop-filter: blur(10px);
      animation: alertSlide 0.25s ease-out;
    }
    .alert-strip.show { display: flex; }
    .alert-strip.warn {
      background: rgba(28, 24, 18, 0.85);
      border-color: var(--status-ochre-border);
    }
    @keyframes alertSlide {
      from { transform: translateY(-6px); opacity: 0; }
      to { transform: translateY(0); opacity: 1; }
    }
    .alert-left {
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .alert-icon {
      width: 22px;
      height: 22px;
      border-radius: 4px;
      display: flex;
      align-items: center;
      justify-content: center;
      background: rgba(248, 113, 113, 0.15);
      color: var(--status-rose);
      font-size: 11px;
      font-weight: 700;
      border: 1px solid var(--status-rose-border);
    }
    .alert-strip.warn .alert-icon {
      background: rgba(251, 191, 36, 0.15);
      color: var(--status-ochre);
      border-color: var(--status-ochre-border);
    }
    .btn-quick-reset {
      background: rgba(255, 255, 255, 0.06);
      border: 1px solid rgba(255, 255, 255, 0.14);
      color: var(--text-main);
      padding: 4px 10px;
      border-radius: 5px;
      font-size: 10px;
      font-family: var(--font-mono);
      font-weight: 600;
      cursor: pointer;
      text-transform: uppercase;
      transition: all 0.2s;
    }
    .btn-quick-reset:hover {
      background: rgba(255, 255, 255, 0.14);
    }

    /* =========================================================
       INSTRUMENT CLUSTER GRID
       ========================================================= */
    .cluster-grid {
      display: grid;
      grid-template-columns: 320px 1fr 320px;
      gap: 16px;
      align-items: stretch;
    }
    @media (max-width: 1150px) {
      .cluster-grid { grid-template-columns: 1fr; }
    }

    .panel {
      background: var(--card-bg);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      border: 1px solid var(--card-border);
      box-shadow: var(--card-inner-glow), 0 10px 28px rgba(0, 0, 0, 0.3);
      border-radius: 16px;
      padding: 20px;
      display: flex;
      flex-direction: column;
      position: relative;
    }
    .panel-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 16px;
      padding-bottom: 10px;
      border-bottom: 1px solid rgba(255, 255, 255, 0.04);
    }
    .panel-title {
      font-size: 11px;
      font-weight: 600;
      letter-spacing: 1.5px;
      text-transform: uppercase;
      color: var(--text-muted);
      display: flex;
      align-items: center;
      gap: 7px;
    }
    .panel-title svg { width: 13px; height: 13px; fill: var(--text-dim); }
    .panel-tag {
      font-family: var(--font-mono);
      font-size: 10px;
      color: var(--text-dim);
    }

    /* LEFT WING: POWERTRAIN & THERMAL */
    .gauge-block {
      background: rgba(0, 0, 0, 0.25);
      border: 1px solid rgba(255, 255, 255, 0.04);
      border-radius: 12px;
      padding: 14px;
      margin-bottom: 12px;
    }
    .gauge-block-top {
      display: flex;
      justify-content: space-between;
      align-items: baseline;
      margin-bottom: 8px;
    }
    .gauge-name {
      font-size: 11px;
      font-weight: 500;
      color: var(--text-muted);
      letter-spacing: 0.5px;
    }
    .gauge-val-big {
      font-family: var(--font-mono);
      font-size: 22px;
      font-weight: 700;
      color: var(--text-main);
    }
    .gauge-val-big small {
      font-size: 11px;
      font-weight: 500;
      color: var(--text-dim);
      margin-left: 2px;
    }

    .linear-bar-wrap {
      width: 100%;
      height: 6px;
      background: rgba(255, 255, 255, 0.05);
      border-radius: 6px;
      overflow: hidden;
    }
    .linear-bar-fill {
      height: 100%;
      width: 0%;
      border-radius: 6px;
      transition: width 0.3s ease, background-color 0.3s ease;
    }
    .fill-fuel-nom {
      background: #6ee7b7;
      opacity: 0.85;
    }
    .fill-fuel-low {
      background: #f87171;
    }
    .fill-temp-nom {
      background: #94a3b8;
    }
    .fill-temp-hot {
      background: #f87171;
    }

    .gauge-footer-meta {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-top: 8px;
      font-family: var(--font-mono);
      font-size: 10px;
      color: var(--text-dim);
    }

    /* Subsystem Stats Table */
    .kernel-stats-card {
      background: rgba(0, 0, 0, 0.2);
      border: 1px solid rgba(255, 255, 255, 0.03);
      border-radius: 12px;
      padding: 12px;
      margin-top: auto;
    }
    .kernel-stats-title {
      font-size: 9px;
      font-weight: 600;
      letter-spacing: 1.2px;
      color: var(--text-dim);
      text-transform: uppercase;
      margin-bottom: 8px;
      display: flex;
      justify-content: space-between;
    }
    .kernel-stats-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 8px;
    }
    .stat-box {
      display: flex;
      flex-direction: column;
      background: rgba(255, 255, 255, 0.015);
      border: 1px solid rgba(255, 255, 255, 0.03);
      padding: 6px 8px;
      border-radius: 6px;
    }
    .stat-box .k-label {
      font-size: 8.5px;
      color: var(--text-dim);
      font-family: var(--font-mono);
      text-transform: uppercase;
    }
    .stat-box .k-num {
      font-size: 13px;
      font-weight: 600;
      font-family: var(--font-mono);
      color: var(--text-secondary);
    }

    /* =========================================================
       CENTER WING: SPEEDOMETER & DRIVE DYNAMICS
       ========================================================= */
    .speedo-panel {
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: space-between;
      position: relative;
      padding: 22px;
    }

    .speedo-stage {
      position: relative;
      width: 360px;
      height: 360px;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    @media (max-width: 440px) {
      .speedo-stage { width: 290px; height: 290px; }
    }

    .speedo-svg {
      width: 100%;
      height: 100%;
      transform: rotate(0deg);
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
      font-size: 88px;
      font-weight: 700;
      line-height: 0.9;
      color: #ffffff;
      letter-spacing: -2px;
      transition: color 0.25s ease;
    }
    .speedo-display-val.over-limit {
      color: var(--status-rose);
    }
    .speedo-unit {
      font-family: var(--font-mono);
      font-size: 12px;
      font-weight: 600;
      letter-spacing: 3px;
      color: var(--text-muted);
      margin-top: 6px;
      text-transform: uppercase;
    }

    .drive-mode-pill {
      margin-top: 12px;
      padding: 5px 16px;
      border-radius: 20px;
      font-family: var(--font-mono);
      font-size: 10px;
      font-weight: 600;
      letter-spacing: 1.5px;
      text-transform: uppercase;
      background: rgba(255, 255, 255, 0.04);
      border: 1px solid rgba(255, 255, 255, 0.08);
      color: var(--text-dim);
      transition: all 0.25s ease;
    }
    .drive-mode-pill.mode-DRIVING {
      background: rgba(255, 255, 255, 0.08);
      border-color: rgba(255, 255, 255, 0.2);
      color: #ffffff;
    }
    .drive-mode-pill.mode-FAULT {
      background: var(--status-rose-bg);
      border-color: var(--status-rose-border);
      color: #fca5a5;
    }

    .speedo-bottom-telemetry {
      width: 100%;
      display: flex;
      justify-content: space-around;
      align-items: center;
      background: rgba(0, 0, 0, 0.25);
      border: 1px solid rgba(255, 255, 255, 0.04);
      border-radius: 10px;
      padding: 9px 14px;
      margin-top: 14px;
    }
    .tele-cell {
      display: flex;
      flex-direction: column;
      align-items: center;
    }
    .tele-cell .t-lbl {
      font-size: 8.5px;
      font-family: var(--font-mono);
      letter-spacing: 1px;
      color: var(--text-dim);
      text-transform: uppercase;
    }
    .tele-cell .t-val {
      font-size: 12px;
      font-weight: 600;
      font-family: var(--font-mono);
      color: var(--text-secondary);
    }

    /* =========================================================
       RIGHT WING: CHASSIS & TPMS
       ========================================================= */
    .tpms-chassis-stage {
      display: flex;
      align-items: center;
      justify-content: center;
      position: relative;
      margin: 8px 0;
      min-height: 200px;
    }
    .car-silhouette {
      width: 120px;
      opacity: 0.75;
    }

    .tpms-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
    }
    .wheel-card {
      background: rgba(0, 0, 0, 0.25);
      border: 1px solid rgba(255, 255, 255, 0.04);
      border-radius: 10px;
      padding: 10px;
      display: flex;
      flex-direction: column;
      align-items: center;
      text-align: center;
      transition: all 0.25s ease;
    }
    .wheel-card.punctured {
      background: var(--status-rose-bg);
      border-color: var(--status-rose-border);
    }
    .wheel-pos {
      font-size: 9px;
      font-weight: 600;
      font-family: var(--font-mono);
      color: var(--text-dim);
      letter-spacing: 0.8px;
      text-transform: uppercase;
    }
    .wheel-psi {
      font-family: var(--font-mono);
      font-size: 18px;
      font-weight: 700;
      color: var(--text-main);
      margin: 2px 0;
    }
    .wheel-card.punctured .wheel-psi {
      color: var(--status-rose);
    }
    .wheel-status {
      font-size: 8.5px;
      font-weight: 600;
      font-family: var(--font-mono);
      letter-spacing: 0.8px;
      color: var(--status-sage);
      text-transform: uppercase;
    }
    .wheel-card.punctured .wheel-status {
      color: var(--status-rose);
    }

    .chassis-health-bar {
      margin-top: auto;
      padding: 10px;
      background: rgba(0, 0, 0, 0.2);
      border: 1px solid rgba(255, 255, 255, 0.03);
      border-radius: 10px;
      display: flex;
      flex-direction: column;
      gap: 6px;
    }
    .chassis-row {
      display: flex;
      justify-content: space-between;
      font-size: 10px;
      font-family: var(--font-mono);
      color: var(--text-dim);
    }
    .chassis-badge {
      font-weight: 600;
      color: var(--text-secondary);
    }

    /* =========================================================
       BOTTOM DECK: SWITCHBOARD & BLACK-BOX RECORDER
       ========================================================= */
    .deck-grid {
      display: grid;
      grid-template-columns: 1.25fr 1fr;
      gap: 16px;
    }
    @media (max-width: 950px) {
      .deck-grid { grid-template-columns: 1fr; }
    }

    .switchboard-section-title {
      font-size: 9px;
      font-weight: 600;
      letter-spacing: 1.5px;
      text-transform: uppercase;
      color: var(--text-dim);
      font-family: var(--font-mono);
      margin-bottom: 8px;
      margin-top: 2px;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }

    .btn-row {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
      margin-bottom: 14px;
    }
    @media (max-width: 600px) {
      .btn-row { grid-template-columns: 1fr; }
    }

    .cockpit-btn {
      padding: 10px 12px;
      border-radius: 8px;
      border: 1px solid rgba(255, 255, 255, 0.08);
      font-family: var(--font-sans);
      font-size: 11px;
      font-weight: 600;
      letter-spacing: 0.6px;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 6px;
      transition: all 0.2s ease;
      text-transform: uppercase;
      background: rgba(255, 255, 255, 0.04);
      color: var(--text-main);
    }
    .cockpit-btn:hover {
      background: rgba(255, 255, 255, 0.08);
      border-color: rgba(255, 255, 255, 0.16);
      transform: translateY(-1px);
    }
    .cockpit-btn:active {
      transform: translateY(1px);
    }
    .cockpit-btn .k-badge {
      font-family: var(--font-mono);
      font-size: 8.5px;
      padding: 1px 4px;
      border-radius: 3px;
      background: rgba(255, 255, 255, 0.08);
      color: var(--text-muted);
    }

    .btn-drive-start {
      border-left: 3px solid var(--status-sage);
    }
    .btn-drive-stop {
      border-left: 3px solid var(--text-dim);
    }
    .btn-drive-reset {
      border-left: 3px solid var(--accent-ice);
    }

    /* Fault Injection Switches - Subdued Matte */
    .fault-matrix {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: 8px;
    }
    @media (max-width: 700px) {
      .fault-matrix { grid-template-columns: repeat(2, 1fr); }
    }

    .btn-fault-switch {
      background: rgba(255, 255, 255, 0.025);
      border: 1px solid rgba(255, 255, 255, 0.06);
      color: var(--text-secondary);
      padding: 8px 8px;
      border-radius: 8px;
      font-size: 10px;
      font-weight: 600;
      letter-spacing: 0.5px;
      cursor: pointer;
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 3px;
      transition: all 0.2s;
    }
    .btn-fault-switch:hover {
      background: rgba(248, 113, 113, 0.1);
      border-color: var(--status-rose-border);
      color: #fca5a5;
      transform: translateY(-1px);
    }
    .btn-fault-switch:active {
      transform: translateY(1px);
    }
    .btn-fault-switch .sub-text {
      font-size: 8.5px;
      font-family: var(--font-mono);
      color: var(--text-dim);
    }

    /* Telemetry Sparkline Canvas */
    .sparkline-box {
      margin-top: 14px;
      background: rgba(0, 0, 0, 0.25);
      border: 1px solid rgba(255, 255, 255, 0.04);
      border-radius: 10px;
      padding: 10px 12px;
    }
    .sparkline-header {
      display: flex;
      justify-content: space-between;
      font-size: 9px;
      font-family: var(--font-mono);
      color: var(--text-dim);
      margin-bottom: 6px;
      text-transform: uppercase;
    }
    .sparkline-legend {
      display: flex;
      gap: 10px;
    }
    .spark-leg-item {
      display: flex;
      align-items: center;
      gap: 4px;
    }
    .spark-leg-dot {
      width: 5px; height: 5px; border-radius: 50%;
    }
    canvas#telemetryChart {
      width: 100%;
      height: 65px;
      display: block;
    }

    /* Event Recorder Stream */
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
      background: rgba(255, 255, 255, 0.04);
      border: 1px solid var(--card-border);
      color: var(--text-dim);
      padding: 2px 7px;
      border-radius: 4px;
      font-size: 9.5px;
      font-family: var(--font-mono);
      cursor: pointer;
      transition: all 0.2s;
    }
    .btn-recorder-tool:hover {
      background: rgba(255, 255, 255, 0.08);
      color: var(--text-main);
    }

    .log-stream {
      flex: 1;
      background: rgba(0, 0, 0, 0.35);
      border: 1px solid rgba(255, 255, 255, 0.04);
      border-radius: 10px;
      padding: 10px;
      font-family: var(--font-mono);
      font-size: 10.5px;
      height: 240px;
      overflow-y: auto;
      display: flex;
      flex-direction: column-reverse;
      gap: 5px;
    }
    .log-row {
      display: flex;
      align-items: baseline;
      gap: 8px;
      line-height: 1.35;
      padding-bottom: 3px;
      border-bottom: 1px solid rgba(255, 255, 255, 0.02);
    }
    .log-row-time {
      color: var(--text-dim);
      font-size: 9.5px;
      white-space: nowrap;
    }
    .log-badge {
      font-size: 8.5px;
      font-weight: 600;
      padding: 1px 5px;
      border-radius: 3px;
      text-transform: uppercase;
      white-space: nowrap;
    }
    .log-badge.CRITICAL {
      background: var(--status-rose-bg);
      color: #fca5a5;
      border: 1px solid var(--status-rose-border);
    }
    .log-badge.WARNING {
      background: var(--status-ochre-bg);
      color: #fde68a;
      border: 1px solid var(--status-ochre-border);
    }
    .log-badge.INFO {
      background: rgba(255, 255, 255, 0.04);
      color: var(--text-muted);
      border: 1px solid rgba(255, 255, 255, 0.08);
    }
    .log-row-msg {
      color: var(--text-secondary);
      word-break: break-word;
    }

    ::-webkit-scrollbar { width: 5px; height: 5px; }
    ::-webkit-scrollbar-track { background: rgba(0, 0, 0, 0.2); }
    ::-webkit-scrollbar-thumb { background: rgba(255, 255, 255, 0.1); border-radius: 3px; }
    ::-webkit-scrollbar-thumb:hover { background: rgba(255, 255, 255, 0.2); }
  </style>
</head>
<body>

<div class="container">

  <!-- TOP COCKPIT HUD -->
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
      <button class="audio-toggle-btn" id="audioToggle" onclick="toggleAudio()" title="Toggle Audio Chimes">
        <span id="audioIcon">&#128263;</span>
        <span id="audioText">MUTED</span>
      </button>
    </div>
  </header>

  <!-- SUBTLE ALERT STRIP -->
  <div class="alert-strip" id="alertStrip">
    <div class="alert-left">
      <div class="alert-icon">!</div>
      <div>
        <div style="font-weight: 600; letter-spacing: 0.3px;" id="alertTitle">CRITICAL KERNEL INTERRUPT</div>
        <div style="font-size: 11px; opacity: 0.8; font-family: var(--font-mono);" id="alertDesc">Telemetry out of safe operational thresholds.</div>
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
          <span>RESERVE: <span id="fuelReserveTag" style="color: var(--text-secondary);">NOMINAL</span></span>
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
          <span id="tempStateTag" style="color: var(--text-secondary);">NORMAL</span>
        </div>
      </div>

      <!-- Kernel Statistics Table -->
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
            <span class="k-num" id="statFaults">0</span>
          </div>
        </div>
      </div>

    </div>

    <!-- CENTER WING: SPEEDOMETER -->
    <div class="panel speedo-panel">
      <div class="panel-header" style="width: 100%;">
        <div class="panel-title">
          <svg viewBox="0 0 24 24"><path d="M12 4a8 8 0 00-8 8c0 2.21.89 4.21 2.34 5.66l1.41-1.41A6 6 0 016 12a6 6 0 0111.46-2.46l1.86-.75A8 8 0 0012 4z"/></svg>
          Digital Cluster Instrument
        </div>
        <div class="panel-tag" id="faultBadge" style="color: var(--text-dim); font-weight: 600;">FAULT: NONE</div>
      </div>

      <div class="speedo-stage">
        <svg class="speedo-svg" viewBox="0 0 320 320">
          <defs>
            <linearGradient id="speedoGrad" x1="0%" y1="100%" x2="100%" y2="0%">
              <stop offset="0%" stop-color="#94a3b8" />
              <stop offset="65%" stop-color="#cbd5e1" />
              <stop offset="100%" stop-color="#f87171" />
            </linearGradient>
          </defs>

          <!-- Outer tick track background -->
          <circle cx="160" cy="160" r="132" fill="none" stroke="rgba(255, 255, 255, 0.03)" stroke-width="1" />
          <circle cx="160" cy="160" r="102" fill="none" stroke="rgba(255, 255, 255, 0.03)" stroke-width="1" />

          <!-- Background Gauge Arc -->
          <circle cx="160" cy="160" r="118" fill="none"
                  stroke="rgba(255, 255, 255, 0.04)"
                  stroke-width="10"
                  stroke-linecap="round"
                  stroke-dasharray="494.3 741.4"
                  stroke-dashoffset="0"
                  transform="rotate(150 160 160)" />

          <!-- Active Velocity Arc Sweep (Subtle Matte) -->
          <circle id="speedArc" cx="160" cy="160" r="118" fill="none"
                  stroke="url(#speedoGrad)"
                  stroke-width="10"
                  stroke-linecap="round"
                  stroke-dasharray="494.3 741.4"
                  stroke-dashoffset="494.3"
                  transform="rotate(150 160 160)"
                  style="transition: stroke-dashoffset 0.3s cubic-bezier(0.2, 0, 0, 1);" />

          <g id="tickGroup"></g>

          <!-- Precision Needle Pointer -->
          <g id="needleGroup" transform="rotate(-120 160 160)" style="transition: transform 0.3s cubic-bezier(0.2, 0, 0, 1);">
            <line x1="160" y1="160" x2="160" y2="48" stroke="#e2e8f0" stroke-width="2" stroke-linecap="round" />
            <circle cx="160" cy="48" r="2.5" fill="#ffffff" />
            <circle cx="160" cy="160" r="6" fill="#1e2430" stroke="#94a3b8" stroke-width="1.5" />
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
          <span class="t-val" style="color: var(--text-secondary);">120 KM/H</span>
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
        <div class="panel-tag" id="tpmsGlobalTag" style="color: var(--text-secondary); font-weight: 600;">TPMS: NOMINAL</div>
      </div>

      <!-- Top-Down Car Silhouette -->
      <div class="tpms-chassis-stage">
        <svg class="car-silhouette" viewBox="0 0 100 190">
          <defs>
            <linearGradient id="carGrad" x1="0%" y1="0%" x2="100%" y2="100%">
              <stop offset="0%" stop-color="#181d26" />
              <stop offset="100%" stop-color="#0f1219" />
            </linearGradient>
          </defs>
          <path d="M28 10 C38 4, 62 4, 72 10 C82 17, 85 45, 85 70 C88 95, 88 135, 84 165 C82 180, 68 186, 50 186 C32 186, 18 180, 16 165 C12 135, 12 95, 15 70 C15 45, 18 17, 28 10 Z"
                fill="url(#carGrad)" stroke="rgba(255, 255, 255, 0.12)" stroke-width="1.5" />
          <path d="M26 50 C38 45, 62 45, 74 50 L70 78 C58 75, 42 75, 30 78 Z"
                fill="rgba(255, 255, 255, 0.04)" stroke="rgba(255, 255, 255, 0.1)" stroke-width="0.8" />
          <path d="M30 125 C42 128, 58 128, 70 125 L73 145 C60 148, 40 148, 27 145 Z"
                fill="rgba(255, 255, 255, 0.03)" stroke="rgba(255, 255, 255, 0.08)" stroke-width="0.8" />
          <path d="M30 78 L70 78 L70 125 L30 125 Z" fill="none" stroke="rgba(255, 255, 255, 0.05)" stroke-width="0.8" />
          <rect id="wMeshFL" x="6" y="32" width="10" height="26" rx="4" fill="#64748b" opacity="0.8" />
          <rect id="wMeshFR" x="84" y="32" width="10" height="26" rx="4" fill="#64748b" opacity="0.8" />
          <rect id="wMeshRL" x="6" y="132" width="10" height="26" rx="4" fill="#64748b" opacity="0.8" />
          <rect id="wMeshRR" x="84" y="132" width="10" height="26" rx="4" fill="#64748b" opacity="0.8" />
        </svg>
      </div>

      <div class="tpms-grid">
        <div class="wheel-card" id="cardFL">
          <span class="wheel-pos">Front Left</span>
          <span class="wheel-psi" id="psiFL">--<small style="font-size: 10px;"> PSI</small></span>
          <span class="wheel-status" id="statFL">NOMINAL</span>
        </div>
        <div class="wheel-card" id="cardFR">
          <span class="wheel-pos">Front Right</span>
          <span class="wheel-psi" id="psiFR">--<small style="font-size: 10px;"> PSI</small></span>
          <span class="wheel-status" id="statFR">NOMINAL</span>
        </div>
        <div class="wheel-card" id="cardRL">
          <span class="wheel-pos">Rear Left</span>
          <span class="wheel-psi" id="psiRL">--<small style="font-size: 10px;"> PSI</small></span>
          <span class="wheel-status" id="statRL">NOMINAL</span>
        </div>
        <div class="wheel-card" id="cardRR">
          <span class="wheel-pos">Rear Right</span>
          <span class="wheel-psi" id="psiRR">--<small style="font-size: 10px;"> PSI</small></span>
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
          <span>&lt; 26.0 PSI</span>
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
        <span style="font-size: 8.5px; color: var(--text-dim);">KEYS [S] [P] [R]</span>
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
        <span>SAFETY FAULT INJECTION (DS_IOC_INJECT_FAULT)</span>
        <span style="font-size: 8.5px; color: var(--text-dim);">KEYS [1] [2] [3] [4]</span>
      </div>
      <div class="fault-matrix">
        <button class="btn-fault-switch" onclick="sendCmd('fault', 'overheat')">
          <span>OVERHEAT</span>
          <span class="sub-text">118 &deg;C [1]</span>
        </button>
        <button class="btn-fault-switch" onclick="sendCmd('fault', 'lowfuel')">
          <span>LOW FUEL</span>
          <span class="sub-text">8% [2]</span>
        </button>
        <button class="btn-fault-switch" onclick="sendCmd('fault', 'flattyre')">
          <span>FLAT TYRE</span>
          <span class="sub-text">18 PSI [3]</span>
        </button>
        <button class="btn-fault-switch" onclick="sendCmd('fault', 'overspeed')">
          <span>OVERSPEED</span>
          <span class="sub-text">140 KM/H [4]</span>
        </button>
      </div>

      <!-- Telemetry Sparkline -->
      <div class="sparkline-box">
        <div class="sparkline-header">
          <span>Telemetry Stream (Last 40s)</span>
          <div class="sparkline-legend">
            <div class="spark-leg-item">
              <span class="spark-leg-dot" style="background: #cbd5e1;"></span>
              <span>Speed</span>
            </div>
            <div class="spark-leg-item">
              <span class="spark-leg-dot" style="background: #94a3b8;"></span>
              <span>Temp</span>
            </div>
          </div>
        </div>
        <canvas id="telemetryChart" width="500" height="65"></canvas>
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
      playTone(520, 0.06, 'sine');
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
      gain.gain.setValueAtTime(0.05, audioCtx.currentTime);
      gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + dur);
      osc.connect(gain);
      gain.connect(audioCtx.destination);
      osc.start();
      osc.stop(audioCtx.currentTime + dur);
    } catch(e) {}
  }

  function playAlertChime() {
    if (!audioEnabled || !audioCtx) return;
    playTone(740, 0.1, 'sine');
    setTimeout(() => playTone(580, 0.16, 'sine'), 130);
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
      const rInner = isMajor ? 122 : 126;

      const x1 = 160 + rOuter * Math.cos(angleRad);
      const y1 = 160 + rOuter * Math.sin(angleRad);
      const x2 = 160 + rInner * Math.cos(angleRad);
      const y2 = 160 + rInner * Math.sin(angleRad);

      const line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      line.setAttribute('x1', x1);
      line.setAttribute('y1', y1);
      line.setAttribute('x2', x2);
      line.setAttribute('y2', y2);
      line.setAttribute('stroke', isRedline ? 'rgba(248, 113, 113, 0.6)' : (isMajor ? 'rgba(255,255,255,0.3)' : 'rgba(255,255,255,0.1)'));
      line.setAttribute('stroke-width', isMajor ? (isRedline ? '1.8' : '1.5') : '1');
      tickGroup.appendChild(line);

      if (isMajor) {
        const rText = 98;
        const tx = 160 + rText * Math.cos(angleRad);
        const ty = 160 + rText * Math.sin(angleRad);
        const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        text.setAttribute('x', tx);
        text.setAttribute('y', ty + 3.5);
        text.setAttribute('fill', isRedline ? 'rgba(248, 113, 113, 0.7)' : 'rgba(255, 255, 255, 0.35)');
        text.setAttribute('font-size', '9.5');
        text.setAttribute('font-weight', '500');
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

    ctx.strokeStyle = 'rgba(255, 255, 255, 0.03)';
    ctx.lineWidth = 1;
    [0.33, 0.66].forEach(ratio => {
      ctx.beginPath();
      ctx.moveTo(0, h * ratio);
      ctx.lineTo(w, h * ratio);
      ctx.stroke();
    });

    const step = w / (speedHistory.length - 1);

    // Temp line in subtle muted slate
    ctx.beginPath();
    ctx.strokeStyle = 'rgba(148, 163, 184, 0.6)';
    ctx.lineWidth = 1.2;
    for (let i = 0; i < tempHistory.length; i++) {
      const val = tempHistory[i];
      const norm = Math.min(Math.max((val - 40) / 90, 0), 1);
      const y = h - (norm * (h - 8)) - 4;
      const x = i * step;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();

    // Speed line in clean silver
    ctx.beginPath();
    ctx.strokeStyle = 'rgba(241, 245, 249, 0.85)';
    ctx.lineWidth = 1.5;
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
        fBadge.style.color = 'var(--status-rose)';
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
        document.getElementById('fuelReserveTag').style.color = 'var(--status-rose)';
      } else {
        fuelBar.className = 'linear-bar-fill fill-fuel-nom';
        document.getElementById('fuelReserveTag').innerText = 'NOMINAL';
        document.getElementById('fuelReserveTag').style.color = 'var(--text-secondary)';
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
        tempTag.style.color = 'var(--status-rose)';
      } else if (tempC > 95) {
        tempBar.className = 'linear-bar-fill fill-temp-hot';
        tempTag.innerText = 'ELEVATED';
        tempTag.style.color = 'var(--status-ochre)';
      } else {
        tempBar.className = 'linear-bar-fill fill-temp-nom';
        tempTag.innerText = 'STABLE';
        tempTag.style.color = 'var(--text-secondary)';
      }

      const psi = data.tyre_psi;
      const isFlat = (psi < 26.0);
      ['FL', 'FR', 'RL', 'RR'].forEach(id => {
        document.getElementById('psi' + id).innerHTML = psi.toFixed(1) + '<small style="font-size: 10px;"> PSI</small>';
        const card = document.getElementById('card' + id);
        const stat = document.getElementById('stat' + id);
        const mesh = document.getElementById('wMesh' + id);
        if (isFlat) {
          card.classList.add('punctured');
          stat.innerText = 'DEPRESSURIZED';
          if (mesh) mesh.setAttribute('fill', '#f87171');
        } else {
          card.classList.remove('punctured');
          stat.innerText = 'NOMINAL';
          if (mesh) mesh.setAttribute('fill', '#64748b');
        }
      });
      const tpmsGlobal = document.getElementById('tpmsGlobalTag');
      if (isFlat) {
        tpmsGlobal.innerText = 'TPMS: LOW PRESSURE';
        tpmsGlobal.style.color = 'var(--status-rose)';
      } else {
        tpmsGlobal.innerText = 'TPMS: NOMINAL';
        tpmsGlobal.style.color = 'var(--text-secondary)';
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
        alertDesc.innerText = 'Thermal core measured ' + tempC + ' °C. Risk of block warping.';
      } else if (fuelPct < 10) {
        alertStrip.className = 'alert-strip show warn';
        alertTitle.innerText = 'LOW FUEL RESERVE';
        alertDesc.innerText = 'Fuel reserve critically low (' + fuelPct + '%). Refuel soon.';
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
    playTone(560, 0.04, 'sine');
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
