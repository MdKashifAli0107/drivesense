# DriveSense ? Stage 6: Final Project Engineering Report

## 1. Executive Summary

DriveSense is an autonomous virtual vehicle sensor system engineered specifically for Linux. By decoupling virtual sensor simulation inside a Linux character device driver (`/dev/drivesense`) from user space consumption, DriveSense delivers a reliable, safe, and reproducible environment for developing and verifying automotive digital cockpit software without requiring costly physical vehicle prototypes or hazardous real-world test rigs.

The system was conceived, designed, developed, and verified across six distinct lifecycle stages, achieving 100% test passing rates across unit tests, multi-threaded kernel integration tests, 20x driver stress cycles, zero compiler warnings, zero static analysis findings via `cppcheck`, and zero memory leaks under `valgrind`.

---

## 2. Six-Stage Development Lifecycle Summary

### Stage 0: Environment Provisioning & Repository Setup
- Audited the Ubuntu 24.04 LTS host environment: Linux kernel `7.0.0-38-generic`, GCC/G++ 13.3.0, CMake 3.28.3, Git 2.43.0.
- Provisioned necessary kernel headers and developer tools (`libncurses-dev`, `cppcheck`, `valgrind`, `mokutil`, `dwarves`).
- Established the modular repository layout and initialized Git branching strategy (`main`, `develop`, and feature branches).

### Stage 1: Project Motivation & Scope Definition (`docs/01_introduction.md`)
- Identified critical bottlenecks in automotive digital cockpit development: high cost of hardware benches, destructive hazards of edge-case physical sensor testing, and flaky test automation.
- Outlined precise system scope: 4 core virtual sensors (speed, fuel, engine coolant temperature, tyre pressure), kernel softirq timer simulation, ioctl control, terminal UI dashboard, disk logging, and `/proc` diagnostics.

### Stage 2: Product Requirements & Specifications (`docs/02_PRD.md`)
- Detailed 9 functional requirements (FR1?FR9) covering device node creation, dynamic physics simulation, live dashboard rendering, edge-triggered alerting, fault injection, system reset, and procfs diagnostics.
- Detailed 6 non-functional requirements (NFR1?NFR6) covering language compliance (pure C and C++17), refresh rates (? 1 Hz), 20x stress load/unload resilience, thread-safe spinlock concurrency, zero memory leaks, and clean code maintainability.

### Stage 3: System Architecture, Data Protocol & UML Design (`docs/03_design.md`)
- Engineered the 3-tier decoupled architecture: C++ User Application Tier ? Linux VFS Device Interface Tier ? Kernel Module Driver Tier.
- Defined shared binary data structures (`struct ds_sensor_data`, `struct ds_stats`) and `ioctl` command definitions in `include/drivesense_ioctl.h`.
- Formulated comprehensive UML diagrams (Class, Sequence, State, and Architecture) in both Mermaid and PlantUML formats.

### Stage 4: Kernel Driver & C++17 Dashboard Implementation
- Implemented `driver/drivesense.c` as a Linux character device using `alloc_chrdev_region`, `cdev_init`, `class_create`, `device_create`, and `proc_create`.
- Programmed a 500 ms softirq simulation timer protected by `spinlock_t`, employing `spin_lock` in interrupt context and `spin_lock_bh` in process-context syscalls.
- Built the object-oriented C++17 dashboard application (`SensorDevice`, `SensorData`, `AlertManager`, `Logger`, `Dashboard`, `App`, `main.cpp`) featuring RAII resource management, non-blocking ncurses gauge rendering, edge-triggered alert detection, and persistent file logging.

### Stage 5: Verification, Quality Assurance & Test Harness (`docs/05_testing.md`)
- Authored automated unit test suites (`tests/unit_tests.cpp`) and kernel integration test suites (`tests/integration_test.cpp`).
- Created automated lifecycle scripts (`scripts/stress_load_unload.sh`, `scripts/build_all.sh`, `scripts/run_all_tests.sh`).
- Verified 100% test pass rate, 20x load/unload stress cycles, zero cppcheck findings, and 0 Valgrind memory leaks.

### Stage 6: Final Documentation & Packaging
- Produced comprehensive `README.md`, presentation deck with speaker notes and 12 technical interview Q&As (`docs/presentation.md`), and final engineering report.

---

## 3. Final Architecture & Technical Implementation Highlights

### 1. In-Kernel Character Device Driver (`drivesense.ko`)
- **Version Compatibility:** Dynamic support for Linux 6.4+ (`class_create(name)`) and Linux 6.2+ (`timer_delete_sync`).
- **Concurrency & Spinlock Strategy:** Strict adherence to kernel locking guidelines. Because the simulation timer executes in softirq context, user syscalls (`read`, `ioctl`, `open`, `release`) use `spin_lock_bh` to disable bottom-halves on the local CPU, preventing self-deadlock.
- **Copy Outside Lock:** All memory copies across the user/kernel boundary (`copy_to_user`, `copy_from_user`) occur strictly outside the spinlock, ensuring no sleep-in-atomic violations.
- **Autonomous Simulation:** Integer physics evolving speed (0?200 km/h), fuel (0?100%), engine temperature (20?130 ?C), and tyre pressure (0?40 PSI) realistically over time.

### 2. Multi-Threaded C++17 User Application (`drivesense_app`)
- **Decoupled Concurrency:** Ingestion is performed by a dedicated background thread polling the driver every 250 ms, while the foreground main thread runs an event-driven `ncurses` UI loop at ~20 Hz. Shared state is synchronized via `std::mutex` and `std::atomic<bool>`.
- **Edge-Triggered Alert Engine:** `AlertManager` evaluates readings against thresholds (Speed > 120, Fuel < 10, Temp > 105, Tyres < 26) and only fires events on state transitions, eliminating repetitive log spam.
- **Graceful Fault Tolerance:** Detects driver absence at startup and throws descriptive user-facing recovery guidance.

---

## 4. Key Verification & Test Metrics

| Testing Dimension | Tool / Harness | Target Criteria | Result Achieved | Status |
| :--- | :--- | :--- | :--- | :--- |
| **Unit Testing** | `build/unit_tests` | 7 test cases covering alert thresholds & logging | 7/7 PASSED | **100% PASS** |
| **Integration Testing**| `build/integration_test` | 7 test cases covering syscalls, faults & threads | 7/7 PASSED | **100% PASS** |
| **Driver Stress** | `scripts/stress_load_unload.sh` | 20 consecutive `insmod`/`rmmod` cycles | 20/20 PASSED (0 dmesg warnings) | **100% PASS** |
| **Error Handling** | Unloaded Driver CLI check | Exit non-zero with actionable instructions | PASSED (Clean error output) | **100% PASS** |
| **Static Code Analysis**| `cppcheck 2.13.0` | 0 errors, 0 warnings across all C++ code | 0 errors, 0 warnings | **100% PASS** |
| **Memory Profiling** | `valgrind 3.22.0` | 0 memory leaks, 0 bytes in use at exit | 0 leaks, 71 allocs / 71 frees | **100% PASS** |

---

## 5. Limitations

While DriveSense meets all functional and non-functional capstone requirements, the following technical limitations are acknowledged:
1. **Mathematical Simulation vs. Physical Dynamics:** Virtual vehicle parameters are driven by algorithmic integer heuristics rather than a multi-degree-of-freedom vehicular dynamics physics engine.
2. **Terminal Visuals:** The UI relies on `ncurses` in a terminal window rather than hardware-accelerated automotive vector graphics (e.g. Qt Quick, Unreal Automotive, or Kanzi).
3. **Single Platform Host:** Developed and verified specifically on Ubuntu Linux 24.04 LTS (x86_64).

---

## 6. Future Work & Enhancements

1. **Hardware Bus Bridging:** Implement virtual CAN (`vcan`) or SocketCAN adapters to allow physical CAN bus analysers and real ECU microcontrollers (e.g. STM32 / ESP32) to communicate with the virtual driver.
2. **Physical I2C / SPI Sensors:** Provide an optional driver hardware abstraction layer (HAL) that can switch between internal softirq simulation and physical I2C temperature/pressure transducers on embedded Linux boards (e.g. Raspberry Pi).
3. **Cloud Telemetry Streaming:** Add an asynchronous network client module transmitting JSON/Protobuf telemetry via MQTT or WebSockets to cloud-based vehicle fleet analytics dashboards.
4. **Expanded Automotive Telemetry:** Introduce additional sensor channels such as engine RPM, brake pad wear percentage, battery pack voltage / state-of-charge (for electric vehicles), and GPS location drift.

---

## 7. Conclusion

DriveSense successfully achieves all objectives established in the project charter. By combining an in-kernel Linux character driver with a resilient C++17 dashboard, the project serves as a comprehensive demonstration of systems programming, kernel synchronization, user/kernel memory isolation, and automated verification.
