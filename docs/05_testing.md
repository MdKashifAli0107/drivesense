# DriveSense ? Stage 5: Quality Assurance, Verification & Test Results

## 1. Testing Strategy Overview

The DriveSense verification strategy implements a multi-tiered test framework to guarantee system correctness across kernel space, user space, and inter-process boundaries:

1. **Unit Testing (`tests/unit_tests.cpp`):** Verifies pure-logic components (`AlertManager`, `Logger`, and `SensorData`) without requiring the kernel module to be loaded. Tests threshold boundary values, edge-triggered alarm creation, repeat alert suppression, and thread-safe file logging.
2. **Integration Testing (`tests/integration_test.cpp`):** Validates the live character device driver (`/dev/drivesense`). Tests syscall operations (`read()`, `ioctl()`), state progression (IDLE -> DRIVING -> FAULT -> RESET), concurrency robustness across simultaneous reader threads, and statistical accounting.
3. **Stress Testing (`scripts/stress_load_unload.sh`):** Executes 20 consecutive module load (`insmod`) and unload (`rmmod`) cycles under high frequency, inspecting kernel logs (`dmesg`) for any memory leaks, lock imbalances, or kernel warnings.
4. **Static Code Analysis (`cppcheck`):** Scans all C++ application and test code using `-std=c++17 --enable=warning,style,performance` to catch uninitialized variables, style anomalies, and inefficiencies.
5. **Dynamic Memory Verification (`valgrind`):** Profiles heap allocations and frees to ensure strict zero-leak compliance under C++ RAII standards.

---

## 2. Test Execution Results (Automated Test Suite)

All 17 test cases executed against the live Linux kernel environment passed with a 100% success rate:

| Test ID | Tier | Test Name / Component | Verification Description | Expected Result | Actual Result | Status |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **UT-01** | Unit | `test_engine_temp_threshold` | Evaluate temp at 100 ?C (normal) and 106 ?C (critical) | 100 ?C yields 0 alerts; 106 ?C yields 1 CRITICAL alert | 100 ?C: 0 alerts; 106 ?C: 1 CRITICAL alert | **PASS** |
| **UT-02** | Unit | `test_fuel_threshold` | Evaluate fuel at 50% (normal) and 9% (critical) | 50% yields 0 alerts; 9% yields 1 CRITICAL alert | 50%: 0 alerts; 9%: 1 CRITICAL alert | **PASS** |
| **UT-03** | Unit | `test_tyre_pressure_threshold`| Evaluate tyre PSI at 32 (normal) and 25 (warning) | 32 PSI yields 0 alerts; 25 PSI yields 1 WARNING alert | 32 PSI: 0 alerts; 25 PSI: 1 WARNING alert | **PASS** |
| **UT-04** | Unit | `test_speed_threshold` | Evaluate speed at 100 km/h (normal) and 121 km/h (warning) | 100 km/h yields 0 alerts; 121 km/h yields 1 WARNING alert | 100 km/h: 0 alerts; 121 km/h: 1 WARNING alert | **PASS** |
| **UT-05** | Unit | `test_no_duplicate_alert` | Feed 125, 128, and 135 km/h sequentially | Exactly 1 alert produced on initial limit breach | 1 initial alert; subsequent 2 readings generated 0 alerts | **PASS** |
| **UT-06** | Unit | `test_alert_clearing` | Overspeed (125 km/h) followed by return to normal (90 km/h) | Generates INFO level clear alert and sets active=false | Generated 1 INFO clear alert; active state reset to false | **PASS** |
| **UT-07** | Unit | `test_logger_file_write` | Write alerts and subsystem messages to disk | File created with formatted timestamps, severities, tags | File written and formatted tokens verified on disk | **PASS** |
| **IT-01** | Integration | `test_open_and_read_range` | Open `/dev/drivesense` and read single telemetry packet | All channels within physical bounds (speed 0-200, fuel 0-100, temp 20-130, tyre 0-40) | Speed: 0, Fuel: 80%, Temp: 70 C, Tyre: 32 PSI (All within range) | **PASS** |
| **IT-02** | Integration | `test_start_and_sequence` | Issue `DS_IOC_START` and wait 1.2 seconds | Monotonic sequence increases (`seq2 > seq1`) and state is DRIVING | Sequence increased from 0 to 3; state DRIVING | **PASS** |
| **IT-03** | Integration | `test_inject_overheat` | Inject `DS_FAULT_OVERHEAT` and poll telemetry | Engine temp climbs past 105 ?C within a few seconds | Temp climbed to 109 ?C in 3.2 seconds; state FAULT | **PASS** |
| **IT-04** | Integration | `test_invalid_fault_id` | Pass invalid fault ID 99 via `DS_IOC_INJECT_FAULT` | Syscall fails returning -1 with `errno == EINVAL` | ioctl returned -1, errno set to 22 (EINVAL) | **PASS** |
| **IT-05** | Integration | `test_reset_baseline` | Issue `DS_IOC_RESET` from fault condition | Restores baseline values (Speed 0, Fuel 80, Temp 70, Tyre 32, IDLE, NONE) | All telemetry restored to default baseline | **PASS** |
| **IT-06** | Integration | `test_concurrent_readers` | 2 concurrent POSIX threads reading 40 times each | 80/80 reads succeed without data corruption or crashes | 80 reads succeeded; 0 errors observed under spinlock | **PASS** |
| **IT-07** | Integration | `test_stats_counts` | Perform operations and verify driver statistics counters | `reads`, `ioctls`, and `faults_injected` monotonically increase | All counters incremented accurately | **PASS** |
| **ER-01** | System | Unloaded Driver Detection | Run `./drivesense_app --once` without driver loaded | Returns non-zero exit code with actionable guidance | Exit code 1: `"Please load the driver first: sudo ./scripts/load.sh"` | **PASS** |
| **ST-01** | Stress | 20x Load / Unload Lifecycle | 20 consecutive `insmod` / `rmmod` cycles | `/dev/drivesense` appears and disappears; zero dmesg BUG/Oops | 20/20 cycles passed; dmesg clean with zero warnings | **PASS** |
| **VG-01** | Memory | Valgrind Leak-Check | Valgrind memcheck profiling on unit test binary | 0 heap leaks and 0 errors | All heap blocks freed: 71 allocs, 71 frees, 0 bytes in use | **PASS** |

---

## 3. Static Code Analysis Report (Cppcheck)

Static analysis was performed across the complete C++ codebase (`app` and `tests` directories) using the following parameters:
```bash
cppcheck --enable=warning,style,performance --error-exitcode=1 --std=c++17 app tests
```

### Analysis Output:
```
Checking app/src/AlertManager.cpp ... 9% done
Checking app/src/App.cpp ... 28% done
Checking app/src/Dashboard.cpp ... 43% done
Checking app/src/Logger.cpp ... 48% done
Checking app/src/SensorData.cpp ... 52% done
Checking app/src/SensorDevice.cpp ... 58% done
Checking app/src/main.cpp ... 62% done
Checking tests/integration_test.cpp ... 79% done
Checking tests/unit_tests.cpp ... 100% done
```
**Result:** 0 errors, 0 warnings, 0 style suggestions, 0 performance alerts.

---

## 4. Bugs Found & Resolved During Development

1. **Unit Test False Alert Triggering:**
   - *Symptom:* `test_engine_temp_threshold` failed with unexpected alerts on the initial baseline reading.
   - *Cause:* The `SensorData` default constructor initialized `fuel_pct` and `tyre_psi` to 0, which immediately triggered false Low Fuel (< 10%) and Low Tyre Pressure (< 26 PSI) warnings.
   - *Fix:* Created a helper function `make_nominal_data()` setting nominal automotive values (Speed: 60, Fuel: 75%, Temp: 85 ?C, Tyre: 32 PSI) for baseline test frames.
2. **Overheat Fault Thermal Acceleration:**
   - *Symptom:* `test_inject_overheat` timed out before engine temp exceeded 105 ?C.
   - *Cause:* Temperature was incrementing by only +2 ?C every 500 ms (4 ?C/s), requiring ~9 seconds to reach 106 ?C from a cold 70 ?C start.
   - *Fix:* Tuned the kernel thermal simulation curve during `DS_FAULT_OVERHEAT` to ramp by +6 ?C per tick below 95 ?C, reaching warning thresholds safely within ~3.2 seconds.
3. **Compiler Dangling Else Warning:**
   - *Symptom:* GCC emitted `warning: suggest explicit braces to avoid ambiguous ?else? [-Wdangling-else]` in `drivesense.c`.
   - *Fix:* Replaced nested unbraced `if/else` clauses with explicit brace blocks to guarantee strict zero-warning compilation.
4. **Driver Node Ownership and Build Artifacts:**
   - *Symptom:* Building without root privileges failed if previous commands ran as root.
   - *Fix:* Configured `scripts/load.sh` to explicitly set `chmod 666 /dev/drivesense` so the application runs completely unprivileged.
