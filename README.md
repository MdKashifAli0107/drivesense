# ?? DriveSense ? Virtual Car Sensor Driver and Live Dashboard for Linux

[![Linux Platform](https://img.shields.io/badge/Platform-Ubuntu%20Linux-orange.svg)](https://ubuntu.com)
[![Kernel Subsystem](https://img.shields.io/badge/Kernel-Character%20Driver-blue.svg)](https://kernel.org)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org)
[![Static Analysis](https://img.shields.io/badge/Cppcheck-Passed%20(0%20warnings)-brightgreen.svg)](http://cppcheck.net)
[![Memory Safety](https://img.shields.io/badge/Valgrind-0%20Leaks-brightgreen.svg)](https://valgrind.org)
[![License](https://img.shields.io/badge/License-GPL%20v2%20%2F%20MIT-green.svg)](LICENSE)

> **A software-defined in-kernel virtual vehicle telemetry device and interactive C++17 digital cockpit dashboard for Linux systems programming and automotive software verification.**

---

## ?? Executive Overview

In automotive software development, cockpit instrument clusters, electronic control units (ECUs), and telematics systems must be built and verified months before physical test vehicles or physical sensors (CAN/OBD-II/I2C/SPI transducers) are physically available. Testing critical safety edge cases?such as engine overheating, tyre blowouts, and fuel depletion?is expensive, dangerous, and difficult to reproduce on physical hardware test rigs.

**DriveSense** solves this by providing a virtual automotive sensor driver in Linux. Simulated vehicular dynamics evolve autonomously inside an in-kernel character device driver (`/dev/drivesense`), synchronized via atomic spinlocks and driven by a 500 ms kernel timer. A multi-threaded C++17 dashboard application consumes the telemetry, renders real-time colored gauge bars in the terminal, triggers edge-detected safety alerts, logs events to disk, and supports programmatic fault injection.

---

## ? Key Features

- **Linux Character Device Driver (`/dev/drivesense`):** Custom kernel module with dynamic device node allocation, standard VFS file operations (`open`, `release`, `read`, `unlocked_ioctl`), and strict user/kernel memory isolation.
- **Autonomous Physics Simulation:** Softirq kernel timer (500 ms period) dynamically models speed acceleration/deceleration, fuel consumption, engine thermodynamics, and tyre pressure fluctuations under a dedicated `spinlock_t`.
- **Live Terminal Dashboard (`ncurses`):** Dual-threaded C++17 dashboard with visual text gauge bars, color-coded status badges (green `[ OK ]`, red `[ WARN ]`), terminal resize adaptation, and non-blocking key inputs.
- **Edge-Triggered Safety Alarms:** Monitors threshold boundaries (Speed > 120 km/h, Fuel < 10 %, Engine Temp > 105 ?C, Tyre Pressure < 26 PSI) and suppresses duplicate alert spam while alerting on transitions.
- **Fault Injection & Recovery Engine:** Injects safety hazards on demand (`overheat`, `lowfuel`, `flattyre`, `overspeed`) via `ioctl` commands and keyboard shortcuts, with instant restoration to default baseline.
- **Diagnostic Procfs Interface (`/proc/drivesense`):** Formatted text telemetry and operational statistics (`reads`, `ioctls`, `updates`, `faults_injected`, `open_count`).
- **Comprehensive CLI Suite:** Fully scriptable headless execution (`--once`, `--start`, `--stop`, `--reset`, `--inject`, `--stats`, `--help`).
- **Thread-Safe Persistent Logging:** Appends timestamped events with microsecond precision to `drivesense_alerts.log`.

---

## ??? System Architecture

```mermaid
flowchart TD
    subgraph UserSpace ["User Space Application Layer (C++17)"]
        UI["Dashboard (ncurses UI)"]
        APP["App Controller (Dual-Threaded)"]
        ALERT["AlertManager (Edge-Triggered Logic)"]
        LOG["Logger (drivesense_alerts.log)"]
        DEV["SensorDevice (RAII POSIX Wrapper)"]

        APP --> DEV
        APP --> ALERT
        ALERT --> LOG
        APP --> UI
    end

    subgraph VFS ["Linux Virtual File System (VFS)"]
        DEVNODE["/dev/drivesense\n(Character Device Node, 0666)"]
        PROCNODE["/proc/drivesense\n(seq_file Diagnostic Node)"]
    end

    subgraph KernelSpace ["DriveSense Kernel Driver Layer (C)"]
        CDEV["cdev & file_operations\n(read, unlocked_ioctl, open, release)"]
        STATE["Shared Telemetry & Statistics Buffer\n(struct ds_sensor_data, struct ds_stats)"]
        LOCK["Spinlock (ds_lock)\nspin_lock (Timer) / spin_lock_bh (Process)"]
        TIMER["Kernel Timer (500 ms)\nSoftirq Simulation Loop"]

        CDEV --> STATE
        TIMER --> LOCK
        LOCK --> STATE
    end

    DEV <-->|read() / ioctl()| DEVNODE
    DEVNODE <--> CDEV
    PROCNODE <-->|seq_printf()| STATE
```

---

## ?? Repository Structure

```
drivesense/
??? CMakeLists.txt              # Top-level unified CMake build configuration
??? README.md                   # Comprehensive project documentation
??? .gitignore                  # Git ignore rules for kernel & application builds
??? include/
?   ??? drivesense_ioctl.h      # Shared kernel/user protocol header & ioctls
??? driver/
?   ??? drivesense.c            # Linux kernel character driver with softirq timer
?   ??? Makefile                # Kbuild kernel module build script
??? app/
?   ??? CMakeLists.txt          # Dashboard application CMake build script
?   ??? include/
?   ?   ??? Alert.hpp           # Alert structure definition
?   ?   ??? AlertManager.hpp    # Edge-triggered threshold evaluation engine
?   ?   ??? App.hpp             # Multi-threaded coordinator & signal handler
?   ?   ??? Dashboard.hpp       # ncurses UI terminal gauge renderer
?   ?   ??? Logger.hpp          # Thread-safe disk-backed logging facility
?   ?   ??? SensorData.hpp      # Telemetry conversion and helper wrappers
?   ?   ??? SensorDevice.hpp    # RAII POSIX file descriptor wrapper
?   ??? src/
?       ??? AlertManager.cpp    # Alert threshold evaluation logic
?       ??? App.cpp             # Ingestion thread loop & CLI dispatcher
?       ??? Dashboard.cpp       # ncurses rendering and gauge calculations
?       ??? Logger.cpp          # Timestamped log formatting
?       ??? SensorData.cpp      # Telemetry formatting helpers
?       ??? SensorDevice.cpp    # System call wrappers (read, ioctl)
?       ??? main.cpp            # Application entry point
??? tests/
?   ??? CMakeLists.txt          # Test suite CMake configuration
?   ??? unit_tests.cpp          # Standalone unit tests (AlertManager, Logger)
?   ??? integration_test.cpp    # Driver integration tests (syscalls, concurrency)
??? scripts/
?   ??? build_all.sh            # One-step compilation of driver, app, and tests
?   ??? load.sh                 # Driver loader with automatic permissions setup
?   ??? unload.sh               # Driver unloader with dmesg verification
?   ??? run_all_tests.sh        # Master automated test runner (100% pass)
?   ??? stress_load_unload.sh   # 20x driver stress test verifying 0 leaks
??? docs/
    ??? 01_introduction.md      # Project introduction and problem definition
    ??? 02_PRD.md               # Product requirements document (FR1-9, NFR1-6)
    ??? 03_design.md            # System architecture, data design, and UML
    ??? 04_progress_log.md      # Engineering progress log and issue resolutions
    ??? 05_testing.md           # Verification report with real test results
    ??? 06_final_report.md      # Final capstone report
    ??? presentation.md         # 10-slide deck and 12 technical interview Q&As
    ??? diagrams/               # PlantUML diagrams (architecture, class, sequence, state)
```

---

## ?? System Requirements

- **Operating System:** Linux (tested on Ubuntu 24.04 LTS / Linux kernel `7.0.0-38-generic` / `6.8+`)
- **Compilers:** GCC & G++ 13+ (`-std=c++17`)
- **Build Tools:** CMake 3.16+, GNU Make, `linux-headers-$(uname -r)`
- **Libraries:** GNU Libncurses (`libncurses-dev`), POSIX Threads (`pthread`)
- **QA Tools:** `cppcheck`, `valgrind`

### Quick Dependency Installation:
```bash
sudo apt-get update
sudo apt-get install -y build-essential linux-headers-$(uname -r) cmake git libncurses-dev cppcheck valgrind
```

---

## ?? Build and Run Instructions

### 1. Build Everything
```bash
./scripts/build_all.sh
```
This compiles the kernel driver module (`driver/drivesense.ko`), the dashboard binary (`build/drivesense_app`), and the test suites (`build/unit_tests`, `build/integration_test`).

### 2. Load the Kernel Driver
```bash
sudo ./scripts/load.sh
```
Verifies module insertion and sets `/dev/drivesense` permissions to `0666` so the application runs without root privileges.

### 3. Launch the Interactive Live Dashboard
```bash
./build/drivesense_app
```

#### Keyboard Controls:
| Key | Action | Description |
| :--- | :--- | :--- |
| `S` | **Start** | Transitions car to `DRIVING` mode (speed and fuel evolve) |
| `P` | **Stop** | Transitions car to `IDLE` mode (speed decelerates to 0) |
| `1` | **Fault: Overheat** | Forces engine temperature to climb toward ~118 ?C |
| `2` | **Fault: Low Fuel** | Forces fuel level to drop to critical reserve (? 8 %) |
| `3` | **Fault: Flat Tyre** | Deflates tyre pressure toward ~18 PSI |
| `4` | **Fault: Overspeed** | Accelerates vehicle past speed limit toward ~140 km/h |
| `R` | **Reset** | Restores all telemetry to clean baseline defaults |
| `Q` | **Quit** | Shuts down dashboard and reader thread cleanly |

### 4. Non-Interactive Command-Line Modes (CLI)
DriveSense can be controlled programmatically without opening the visual dashboard:

```bash
# Print single telemetry snapshot
./build/drivesense_app --once

# Start vehicle driving simulation
./build/drivesense_app --start

# Inject a specific vehicle fault
./build/drivesense_app --inject overheat
./build/drivesense_app --inject lowfuel
./build/drivesense_app --inject flattyre
./build/drivesense_app --inject overspeed

# Reset telemetry back to baseline defaults
./build/drivesense_app --reset

# Display driver operational statistics
./build/drivesense_app --stats

# Stop simulation (return to IDLE)
./build/drivesense_app --stop

# Unload the driver when finished
sudo ./scripts/unload.sh
```

---

## ?? Live Sample Outputs (Captured from this Machine)

### Sample Output: Headless Snapshot (`./build/drivesense_app --once`)
```
=== DriveSense Telemetry Snapshot ===
State:            IDLE
Active Fault:     NONE
Speed:            0 km/h
Fuel Level:       80 %
Engine Temp:      70 C
Tyre Pressure:    32 PSI
Sequence:         0
Timestamp:        3581957805517 ns
=== Operational Statistics ===
Reads:            1
IOCTLs:           1
Updates:          0
Faults Injected:  0
Open Clients:     1
```

### Sample Output: Diagnostic Procfs (`cat /proc/drivesense`)
```
=== DriveSense Virtual Vehicle Telemetry ===
State:            IDLE
Active Fault:     NONE
Speed:            0 km/h
Fuel:             80 %
Engine Temp:      70 deg C
Tyre Pressure:    32 PSI
Sequence:         0
Timestamp:        3581957805517 ns
=== Operational Statistics ===
Reads:            1
IOCTLs:           1
Updates:          0
Faults Injected:  0
Open Clients:     0
```

### Sample Output: Friendly Error when Driver is Unloaded
```
$ ./build/drivesense_app --once
DriveSense Application Error: Device /dev/drivesense not found or inaccessible. Please load the driver first: sudo ./scripts/load.sh
```

---

## ?? Testing and Quality Assurance

DriveSense includes a master automated test harness running unit tests, integration tests, stress cycles, static analysis, and memory leak profiling:

```bash
./scripts/run_all_tests.sh
```

### Test Coverage Summary:
- **Unit Tests (`build/unit_tests`):** 7/7 PASSED (Threshold boundaries, edge-triggered alerting, repeat alert suppression, clear transitions, file logging).
- **Graceful Error Check:** PASSED (Helpful error message and non-zero exit code when `/dev/drivesense` is absent).
- **Integration Tests (`build/integration_test`):** 7/7 PASSED (Syscall reading, dynamic sequence updates, overheat fault escalation, invalid fault `EINVAL` rejection, baseline reset, concurrent reader threads under spinlock, driver statistics).
- **Driver Stress Lifecycle (`scripts/stress_load_unload.sh`):** 20/20 cycles PASSED with zero kernel warnings, Oops, or bugs in `dmesg`.
- **Static Code Analysis (`cppcheck`):** 0 errors, 0 warnings across all C++ files.
- **Dynamic Memory Verification (`valgrind`):** 0 leaks, 0 heap blocks remaining at exit.

---

## ?? Systems Engineering Concepts Demonstrated

1. **Linux Kernel Module Architecture:** Module initialization (`module_init`), teardown (`module_exit`), dynamic major/minor number registration (`alloc_chrdev_region`), device class registration (`class_create`), and device node generation (`device_create`).
2. **Virtual File System (VFS) Abstraction:** Implementing `struct file_operations` (`open`, `release`, `read`, `unlocked_ioctl`) and linking file descriptors to in-kernel drivers.
3. **Interrupt & Softirq Synchronization:** Employing `spinlock_t` with `spin_lock` inside the softirq timer callback and `spin_lock_bh` (bottom-half disable) in user syscall context to prevent deadlock.
4. **User-Kernel Memory Isolation:** Strict usage of `copy_to_user()` and `copy_from_user()` executed strictly **outside** spinlock boundaries.
5. **Kernel Timers:** Non-blocking softirq timer scheduling (`timer_setup`, `mod_timer`) for autonomous background simulation.
6. **Virtual File Systems (`procfs`):** Using `seq_file` (`single_open`, `seq_printf`) to expose live human-readable kernel telemetry.
7. **Modern C++17 & RAII:** Encapsulating POSIX file descriptors, `ncurses` screen sessions, and background threads in RAII resource holders to guarantee exception safety and leak prevention.
8. **Multi-Threaded Concurrency:** Dedicated background ingestion thread decoupled from foreground `ncurses` event rendering, synchronized using `std::mutex` and `std::atomic<bool>`.
9. **POSIX Signal Handling:** Trapping `SIGINT` and `SIGTERM` to restore terminal mode, flush disk logs, and shut down threads without corruption.
10. **Automotive Safety Patterns:** Edge-triggered state evaluation to avoid alert storming, fault injection harnesses, and dead-reckoning recovery.

---

## ?? Limitations & Future Work

### Limitations:
- **Telemetry Simulation:** Sensor physics are modeled numerically via integer mathematics rather than rigid multi-body vehicle aerodynamic physics engines.
- **Terminal Display:** The dashboard utilizes terminal-based `ncurses` rather than a hardware-accelerated automotive GPU display (e.g., Qt Digital Cockpit or OpenGL).
- **Single Host IPC:** Operates locally on the Linux workstation without physical vehicle bus integration.

### Future Work:
- **Physical Bus Integration:** Expose virtual socketCAN or I2C endpoints to bridge DriveSense with physical Raspberry Pi ECUs or OBD-II hardware scanners.
- **Telemetry Streaming:** Stream telemetry over MQTT or WebSockets to cloud fleet management dashboards.
- **Extended Sensor Channels:** Add tyre temperature, oil life percentage, battery pack state of charge (for electric vehicles), and GPS coordinate navigation drift.

---

## ?? Author & Acknowledgments

- **Project Lead:** Shail
- **Institution:** Capstone Engineering Project
- **License:** Open source under GPL v2 (Driver) and MIT (Application).
