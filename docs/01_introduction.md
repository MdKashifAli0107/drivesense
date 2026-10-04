# DriveSense — Project Introduction

## 1. Problem

Modern automotive software must be developed and tested before the actual vehicle hardware and sensors are available. Testing directly with real sensors can be costly, risky, and difficult during early development.

DriveSense addresses this problem by creating a software-only virtual car sensor device inside Linux. This allows sensor behavior and dashboard software to be tested without requiring physical automotive hardware.

## 2. Objective

The objective of DriveSense is to build a virtual car sensor system in Linux that:

- Simulates vehicle sensor data.
- Provides speed, fuel level, engine temperature, and tyre pressure values.
- Exposes the virtual sensor through a Linux device interface.
- Allows a C++ dashboard application to read and display the sensor values.
- Generates warnings when sensor values cross defined safety limits.
- Allows faults such as overheating, low fuel, and flat tyres to be intentionally injected.
- Demonstrates Linux device-driver and system-programming concepts.

## 3. Project Scope

### Included

- A Linux kernel driver for the virtual sensor.
- Four simulated sensors:
  - Speed
  - Fuel level
  - Engine temperature
  - Tyre pressure
- A C++ terminal-based dashboard.
- Communication between the dashboard and Linux driver.
- Sensor value updates over time.
- Fault injection and reset functionality.
- Warning and alert handling.
- Logging of warnings.
- Testing of the driver and dashboard.

### Not Included

- Real physical sensors or automotive hardware.
- A graphical desktop dashboard.
- Direct connection to a real vehicle.

## 4. Expected Outcome and Use

The final system will provide a software-based environment for demonstrating and testing car dashboard software without requiring a real vehicle or physical sensors.

DriveSense will also serve as a practical learning project covering Linux device drivers, system programming, C/C++ programming, device-file communication, synchronization, testing, and basic computer architecture concepts.
