# DriveSense — Stage 2: Product Requirements Document (PRD) & Development Plan

## 1. Executive Summary
DriveSense is an autonomous virtual vehicle sensor system engineered specifically for Linux. It couples a Linux kernel character device driver simulating dynamic vehicle telemetry with an interactive C++17 dashboard application. This document details the functional, non-functional, modular, and development lifecycle specifications required for full project realization.

---

## 2. Functional Requirements (FR1 – FR9)

| Requirement ID | Name | Description | Acceptance Criteria |
| :--- | :--- | :--- | :--- |
| **FR1** | Device Node Creation | The kernel driver automatically creates a character device node at `/dev/drivesense` upon module insertion (`insmod`). | `/dev/drivesense` is visible with major/minor numbers and permission `0666` (read/write by unprivileged user space). |
| **FR2** | Multi-Sensor Telemetry | Driver generates 4 distinct vehicle sensor channels: Vehicle Speed (km/h), Fuel Level (%), Engine Coolant Temperature (°C), and Tyre Pressure (PSI). | Telemetry is returned via `read()` and `DS_IOC_GET_DATA` in `struct ds_sensor_data` with strictly valid integer ranges. |
| **FR3** | Dynamic State Simulation | Sensor values evolve realistically over time based on vehicle operating state (IDLE, DRIVING, FAULT) using an internal kernel timer (500 ms period). | In IDLE, speed drifts to 0, temp to 70 °C, tyres 32 PSI. In DRIVING, speed wanders 40–110 km/h, fuel decreases, temp climbs to 85–98 °C. |
| **FR4** | Live Dashboard Display | The user space application provides a full-terminal live dashboard refreshing at least once every second (≥ 1 Hz). | Interactive `ncurses` UI displays color-coded bars, numerical readouts, units, active state, and alert notices without flickering. |
| **FR5** | Threshold Alert Engine | Application monitors telemetry and triggers visual and textual warnings when values breach safety thresholds. | Warning triggered if: Speed > 120 km/h, Fuel < 10 %, Engine Temp > 105 °C, Tyre Pressure < 26 PSI. Edge-triggered to avoid log spam. |
| **FR6** | Fault Injection Engine | System supports deliberate injection of four safety-critical vehicle failure conditions via ioctl and CLI arguments. | Supported faults: Overheat (> 105 °C), Low Fuel (≤ 8 %), Flat Tyre (≤ 18 PSI), Overspeed (> 130 km/h). State transitions to FAULT. |
| **FR7** | System Telemetry Reset | System allows immediate restoration of vehicle telemetry to clean baseline default values. | Speed = 0 km/h, Fuel = 80 %, Temp = 70 °C, Tyres = 32 PSI, State = IDLE, Fault = NONE. Sequence and stats preserved. |
| **FR8** | Persistent Alert Logging | Application logs all alert transition events with microsecond/millisecond timestamps to a persistent file. | Appends events to `drivesense_alerts.log` with human-readable timestamps and severity levels. Thread-safe execution. |
| **FR9** | Procfs Diagnostics | Driver exposes a virtual diagnostic file at `/proc/drivesense` displaying driver status and metrics. | `cat /proc/drivesense` outputs formatted telemetry, state name, active fault name, read count, ioctl count, update count, and open client count. |

---

## 3. Non-Functional Requirements (NFR1 – NFR6)

| Requirement ID | Category | Specification | Verification Method |
| :--- | :--- | :--- | :--- |
| **NFR1** | Implementation Stack | Kernel driver strictly written in C conforming to Linux kernel C standards; user application and tests strictly written in C++17. Shell scripts only for lifecycle automation. | Source code inspection and compiler flags (`-std=c++17`, `-Wall -Wextra -Wpedantic`). |
| **NFR2** | Real-Time Responsiveness | Dashboard refresh rate ≥ 1 Hz (reader loop target: 250–500 ms). Syscall read latency < 1 ms. | Benchmark loop time in user space; monitor sequence increments. |
| **NFR3** | Driver Lifecycle & Robustness | Module must cleanly load and unload at least 20 consecutive times without memory leaks, kernel warnings, or hung tasks. | Automated stress script `scripts/stress_load_unload.sh` checking `dmesg` for BUG/Oops/WARNING. |
| **NFR4** | Concurrency & Thread-Safety | Safe concurrent access by multiple simultaneous reader processes. Spinlock synchronization between timer softirq and user space syscall contexts. | Multi-threaded test program `tests/integration_test.cpp` reading concurrently; use of `spin_lock_bh` in process context and `spin_lock` in timer. |
| **NFR5** | Memory Safety | Zero dynamic memory leaks in both driver and user space application. Strict adherence to C++ RAII. | Driver exit unwinding; user space static analysis using `cppcheck` and memory verification. |
| **NFR6** | Code Clarity & Maintainability | Clean, self-documenting code with comprehensive commentary, modular separation, and zero compiler warnings. | GCC/Clang `-Wall -Wextra -Wpedantic` zero-warning build; thorough progress logs. |

---

## 4. System Architecture & Modules Overview

The DriveSense ecosystem comprises four discrete subsystems:

```
+------------------------------------------------------------------+
|                     User Space Application                       |
|  +---------------------+   +-----------------+   +------------+  |
|  | Dashboard (ncurses) |   | Alert & Logger  |   | CLI Engine |  |
|  +----------+----------+   +--------+--------+   +-----+------+  |
|             \                       |                  /         |
|              +----------------------+-----------------+          |
|                                     |                            |
|                            [ SensorDevice RAII ]                 |
+-------------------------------------|----------------------------+
                                      | Syscalls: read(), ioctl()
+-------------------------------------|----------------------------+
| Linux Kernel VFS:             /dev/drivesense   /proc/drivesense |
+-------------------------------------|------------------|---------+
| DriveSense Kernel Module:           |                  |         |
|  +--------------------+             v                  |         |
|  | Character Device   | ---> [ Sensor State & Stats ] <+         |
|  | File Operations    |             ^                            |
|  +--------------------+             |                            |
|  | Periodic Timer     | ------------+ (500 ms softirq, spinlock) |
|  | Simulation Engine  |                                          |
|  +--------------------+                                          |
+------------------------------------------------------------------+
```

### Module Breakdown
1. **Shared Telemetry Protocol (`include/drivesense_ioctl.h`):** Defines binary layouts (`struct ds_sensor_data`, `struct ds_stats`) and ioctl magic macros compatible with both C and C++17.
2. **Kernel Driver Module (`driver/`):** Character device management, atomic spinlock locking, softirq timer simulation, and procfs status interface.
3. **Dashboard Application (`app/`):**
   - `SensorDevice`: RAII wrapper managing the file descriptor and ioctl interactions.
   - `SensorData`: Data encapsulation and string conversion helpers.
   - `AlertManager`: Pure-logic edge-triggered alert threshold evaluator and historic queue.
   - `Logger`: Thread-safe disk-backed logging facility.
   - `Dashboard`: Terminal-adaptive `ncurses` UI rendering live gauge bars and telemetry.
   - `App`: Multi-threaded coordinator binding background ingestion with UI loop.
4. **Verification & Testing (`tests/` & `scripts/`):** Unit testing suite, multi-threaded kernel integration tests, and 20x stress load/unload verification.

---

## 5. System Deliverables
- `include/drivesense_ioctl.h`: Kernel/user common header.
- `driver/drivesense.ko` & `driver/Makefile`: Linux kernel driver module.
- `app/drivesense_app`: Live dashboard binary with interactive and headless CLI flags.
- `tests/unit_tests` & `tests/integration_test`: Automated test binaries.
- `scripts/`: Operational shell scripts (`load.sh`, `unload.sh`, `build_all.sh`, `run_all_tests.sh`, `stress_load_unload.sh`).
- `docs/`: Complete 6-stage engineering documentation portfolio with PlantUML and Mermaid diagrams.

---

## 6. Development Plan & Staged Timeline

| Phase | Focus Stage | Primary Deliverables | Verification Milestone |
| :--- | :--- | :--- | :--- |
| **Phase 0** | Environment & Repo Setup | Directory structure, `.gitignore`, build tools check, branch setup. | `cmake`, `gcc`, `linux-headers`, git branches verified. |
| **Phase 1** | Stage 1 & 2 Documentation | `01_introduction.md`, `02_PRD.md`. | PRD approved with complete FR/NFR traceability. |
| **Phase 2** | Stage 3 Architecture & Design | `03_design.md`, Mermaid & PlantUML diagrams, `04_progress_log.md`. | Complete sequence, class, state, and architecture diagrams. |
| **Phase 3** | Driver & Shared Protocol | `drivesense_ioctl.h`, `drivesense.c`, `Makefile`, `load.sh`, `unload.sh`. | Clean build, `/dev/drivesense` created, `/proc/drivesense` live values. |
| **Phase 4** | Userland Dashboard App | C++17 RAII classes, ncurses UI, thread coordinator, CLI modes. | `--once`, interactive UI, and live fault injection confirmed. |
| **Phase 5** | Testing & Quality Assurance | Unit tests, integration test, stress script, cppcheck, `05_testing.md`. | 100% test pass rate, 20x stress unload/load pass, zero warnings. |
| **Phase 6** | Final Delivery & Release | `README.md`, `06_final_report.md`, `presentation.md`, git tag `v1.0`. | Clean build from scratch, full project documentation complete. |
