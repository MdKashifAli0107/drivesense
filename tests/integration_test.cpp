#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>
#include <cstring>
#include <sys/ioctl.h>
#include "SensorDevice.hpp"
#include "drivesense_ioctl.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_CASE(name) \
    std::cout << "[RUNNING] " << #name << " ... " << std::flush; \
    g_tests_run++;

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cout << "FAILED\n  Assertion failed: (" #cond ") - " << msg \
                      << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
            return false; \
        } \
    } while (0)

#define ASSERT_FALSE(cond, msg) ASSERT_TRUE(!(cond), msg)

#define ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            std::cout << "FAILED\n  Assertion failed: " << (a) << " == " << (b) \
                      << " - " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
            return false; \
        } \
    } while (0)

#define PASS_TEST() \
    do { \
        std::cout << "PASSED" << std::endl; \
        g_tests_passed++; \
        return true; \
    } while (0)

// Test 1: Device open and valid in-range data
bool test_open_and_read_range()
{
    TEST_CASE(test_open_and_read_range);
    SensorDevice dev("/dev/drivesense");
    SensorData d = dev.read();

    ASSERT_TRUE(d.speed_kmh >= 0 && d.speed_kmh <= 200, "Speed out of range [0, 200]");
    ASSERT_TRUE(d.fuel_pct >= 0 && d.fuel_pct <= 100, "Fuel out of range [0, 100]");
    ASSERT_TRUE(d.engine_temp_c >= 20 && d.engine_temp_c <= 130, "Temp out of range [20, 130]");
    ASSERT_TRUE(d.tyre_psi >= 0 && d.tyre_psi <= 40, "Tyre PSI out of range [0, 40]");

    PASS_TEST();
}

// Test 2: START command and sequence incrementing
bool test_start_and_sequence()
{
    TEST_CASE(test_start_and_sequence);
    SensorDevice dev("/dev/drivesense");
    dev.start();

    SensorData d1 = dev.read();
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    SensorData d2 = dev.read();

    ASSERT_TRUE(d2.sequence > d1.sequence, "Sequence must increase after start");
    ASSERT_EQ(d2.state, static_cast<uint32_t>(DS_STATE_DRIVING), "State must be DRIVING");

    PASS_TEST();
}

// Test 3: INJECT overheat raises temp above 105 within a few seconds
bool test_inject_overheat()
{
    TEST_CASE(test_inject_overheat);
    SensorDevice dev("/dev/drivesense");
    dev.injectFault(DS_FAULT_OVERHEAT);

    bool reached_threshold = false;
    for (int i = 0; i < 30; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        SensorData d = dev.read();
        if (d.engine_temp_c > 105) {
            reached_threshold = true;
            break;
        }
    }

    ASSERT_TRUE(reached_threshold, "Overheat fault must push engine temp above 105 C within a few seconds");
    SensorData current = dev.read();
    ASSERT_EQ(current.state, static_cast<uint32_t>(DS_STATE_FAULT), "State must be FAULT");
    ASSERT_EQ(current.active_fault, static_cast<uint32_t>(DS_FAULT_OVERHEAT), "Active fault must be OVERHEAT");

    PASS_TEST();
}

// Test 4: Invalid fault id returns EINVAL
bool test_invalid_fault_id()
{
    TEST_CASE(test_invalid_fault_id);
    SensorDevice dev("/dev/drivesense");

    uint32_t bad_fault = 99;
    int ret = ::ioctl(dev.getFd(), DS_IOC_INJECT_FAULT, &bad_fault);
    ASSERT_EQ(ret, -1, "Invalid fault ID must return -1");
    ASSERT_EQ(errno, EINVAL, "errno must be EINVAL for bad fault ID");

    PASS_TEST();
}

// Test 5: RESET restores baseline values
bool test_reset_baseline()
{
    TEST_CASE(test_reset_baseline);
    SensorDevice dev("/dev/drivesense");
    dev.reset();

    SensorData d = dev.read();
    ASSERT_EQ(d.speed_kmh, 0, "Speed must be 0 after reset");
    ASSERT_EQ(d.fuel_pct, 80, "Fuel must be 80% after reset");
    ASSERT_EQ(d.engine_temp_c, 70, "Temp must be 70 C after reset");
    ASSERT_EQ(d.tyre_psi, 32, "Tyres must be 32 PSI after reset");
    ASSERT_EQ(d.state, static_cast<uint32_t>(DS_STATE_IDLE), "State must be IDLE");
    ASSERT_EQ(d.active_fault, static_cast<uint32_t>(DS_FAULT_NONE), "Fault must be NONE");

    PASS_TEST();
}

// Test 6: Two threads reading at the same time both get valid data
bool test_concurrent_readers()
{
    TEST_CASE(test_concurrent_readers);
    std::atomic<bool> thread_error{false};
    std::atomic<int> success_count{0};

    auto reader_func = [&]() {
        try {
            SensorDevice dev("/dev/drivesense");
            for (int i = 0; i < 40; ++i) {
                SensorData d = dev.read();
                if (d.speed_kmh < 0 || d.speed_kmh > 200 ||
                    d.fuel_pct < 0 || d.fuel_pct > 100 ||
                    d.engine_temp_c < 20 || d.engine_temp_c > 130 ||
                    d.tyre_psi < 0 || d.tyre_psi > 40) {
                    thread_error = true;
                }
                success_count++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } catch (...) {
            thread_error = true;
        }
    };

    std::thread t1(reader_func);
    std::thread t2(reader_func);

    t1.join();
    t2.join();

    ASSERT_FALSE(thread_error.load(), "Concurrent readers must not encounter errors");
    ASSERT_EQ(success_count.load(), 80, "All 80 concurrent reads must succeed");

    PASS_TEST();
}

// Test 7: GET_STATS counts increase
bool test_stats_counts()
{
    TEST_CASE(test_stats_counts);
    SensorDevice dev("/dev/drivesense");
    struct ds_stats s1 = dev.getStats();

    // Perform operations
    dev.read();
    dev.read();
    dev.start();
    dev.injectFault(DS_FAULT_FLAT_TYRE);
    dev.reset();

    struct ds_stats s2 = dev.getStats();
    ASSERT_TRUE(s2.reads > s1.reads, "Reads counter must increase");
    ASSERT_TRUE(s2.ioctls > s1.ioctls, "IOCTLs counter must increase");
    ASSERT_TRUE(s2.faults_injected > s1.faults_injected, "Faults injected counter must increase");
    ASSERT_TRUE(s2.updates >= s1.updates, "Updates counter must not decrease");

    PASS_TEST();
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "   DRIVESENSE INTEGRATION TEST SUITE    " << std::endl;
    std::cout << "========================================" << std::endl;

    bool all_pass = true;
    all_pass &= test_open_and_read_range();
    all_pass &= test_start_and_sequence();
    all_pass &= test_inject_overheat();
    all_pass &= test_invalid_fault_id();
    all_pass &= test_reset_baseline();
    all_pass &= test_concurrent_readers();
    all_pass &= test_stats_counts();

    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_tests_passed << "/" << g_tests_run << " tests passed." << std::endl;
    std::cout << "========================================" << std::endl;

    return (all_pass && (g_tests_passed == g_tests_run)) ? 0 : 1;
}
