# DriveSense — Stage 1: Project Introduction & Problem Definition

## 1. Problem Statement
In modern automotive software engineering, electronic control unit (ECU) software, telematics systems, and digital cockpit dashboards must be designed, developed, and thoroughly tested long before physical vehicle prototypes or automotive sensors (such as CAN bus transceivers, OBD-II interfaces, temperature transducers, and pressure sensors) are physically manufactured or available.

Relying on physical hardware for early-stage software verification carries several severe disadvantages:
- **High Hardware Cost & Scarcity:** Physical test benches, hardware-in-the-loop (HIL) simulators, and vehicle prototypes are expensive, rare, and difficult to provision across entire software teams.
- **Physical Safety Hazards:** Intentionally testing edge conditions — such as catastrophic engine overheating (>115 °C), rapid tyre blowout (<18 PSI), or emergency low fuel situations — is dangerous and destructive to perform on physical test rigs.
- **Flaky Repeatability:** Physical sensors are subject to environmental drift and wear, making automated continuous integration (CI) test suites inconsistent and difficult to reproduce.

**DriveSense** solves this problem by delivering a software-only, in-kernel virtual car sensor device for Linux. By simulating vehicle dynamics and safety-critical failure modes directly inside a Linux kernel character device, developers can validate dashboard UI logic, alert dispatchers, and diagnostics safely, cheaply, and reliably on standard Linux workstations.

---

## 2. Project Objectives
The objective of DriveSense is to design, implement, test, and document a robust, production-quality virtual car sensor driver and real-time dashboard application:
1. **Linux Kernel Driver:** Implement a loadable character device driver (`/dev/drivesense`) with periodic timer-driven simulation, spinlock synchronization, ioctl control commands, and a diagnostic `/proc` filesystem interface.
2. **Realistic Sensor Telemetry:** Simulate four vital vehicle telemetry channels: vehicle speed, fuel percentage, engine coolant temperature, and tyre pressure.
3. **Fault Injection & Recovery:** Provide controllable injection of real-world automotive hazards (Engine Overheat, Low Fuel, Flat Tyre, Overspeed) and instant system reset to baseline conditions.
4. **Live C++17 Dashboard:** Build an interactive terminal dashboard using modern C++17 and `ncurses`, featuring gauge bars, color-coded threshold warnings, thread-safe asynchronous telemetry ingestion, and non-interactive CLI automation modes.
5. **Persistent Safety Logging:** Automatically record timestamped warning and fault events to a disk-based log file for post-drive safety auditing.
6. **Educational & Engineering Excellence:** Demonstrate foundational systems programming concepts — user space vs. kernel space separation, device file descriptors, `copy_to_user`, spinlocks in softirq contexts, POSIX threads, RAII, and automated stress testing.

---

## 3. Project Scope

### In-Scope
- **Kernel Character Driver:** Implemented in C, conforming to standard Linux kernel subsystem guidelines (`alloc_chrdev_region`, `cdev_init`, `device_create`).
- **Telemetry Simulation:** Autonomous state-driven evolution (IDLE, DRIVING, FAULT) using an internal kernel timer running at 500 ms (2 Hz).
- **Driver IPC & Control:** `read()` syscall support for telemetry snapshots, custom `ioctl` commands for state control (START, STOP, INJECT_FAULT, RESET, GET_STATS, GET_DATA), and `/proc/drivesense` text diagnostics.
- **Userland C++17 Dashboard Application:** Dual-threaded architecture (dedicated reader thread and UI event loop), `ncurses` visual interface with color alerts, threshold validation, and alert edge-detection (preventing repetitive log spam).
- **Automation CLI:** Command-line switches (`--once`, `--start`, `--stop`, `--inject`, `--reset`, `--stats`, `--help`) enabling headless integration and scripting.
- **Automated Verification:** Standalone unit tests, multi-threaded kernel integration tests, and 20x driver stress load/unload cycles.

### Out-of-Scope
- Physical hardware connections (e.g., CAN bus, OBD-II adapters, I2C/SPI physical transducers).
- Graphical X11/Wayland/Qt desktop applications (DriveSense focuses on lightweight terminal and embedded environments).
- Multi-node network streaming over socket or MQTT (telemetry is local IPC).

---

## 4. Expected Outcomes and Practical Applications
Upon completion, DriveSense delivers a turn-key development and verification harness for automotive digital cockpit software:
- **Embedded Automotive Prototyping:** Allows infotainment and instrument cluster engineers to build and test UI screens without hardware dependencies.
- **Safety Critical Fault Validation:** Enables test engineers to verify that audio/visual alarms trigger accurately within strict latency bounds when simulated sensors cross safety thresholds.
- **Educational Systems Platform:** Serves as a reference implementation of Linux device driver architecture, kernel synchronization, user/kernel memory isolation, and modern C++ system software design.
