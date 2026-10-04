#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <cstdlib>
#include "AlertManager.hpp"
#include "Logger.hpp"
#include "SensorData.hpp"

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

// Helper for nominal baseline readings
static SensorData make_nominal_data()
{
    SensorData d;
    d.speed_kmh = 60;
    d.fuel_pct = 75;
    d.engine_temp_c = 85;
    d.tyre_psi = 32;
    d.state = DS_STATE_DRIVING;
    d.active_fault = DS_FAULT_NONE;
    d.sequence = 1;
    return d;
}

bool test_engine_temp_threshold()
{
    TEST_CASE(test_engine_temp_threshold);
    AlertManager mgr;
    SensorData normal = make_nominal_data();
    normal.engine_temp_c = 100;
    auto alerts1 = mgr.evaluate(normal);
    ASSERT_TRUE(alerts1.empty(), "100 C should not trigger alert");
    ASSERT_FALSE(mgr.isTempAlertActive(), "Temp alert should be inactive");

    SensorData warn = make_nominal_data();
    warn.engine_temp_c = 106;
    auto alerts2 = mgr.evaluate(warn);
    ASSERT_EQ(alerts2.size(), 1, "106 C must trigger exactly 1 alert");
    ASSERT_TRUE(mgr.isTempAlertActive(), "Temp alert must be active");
    ASSERT_EQ(alerts2[0].sensor_name, "ENGINE_TEMP", "Sensor name must be ENGINE_TEMP");
    ASSERT_EQ(alerts2[0].severity, "CRITICAL", "Severity must be CRITICAL");

    PASS_TEST();
}

bool test_fuel_threshold()
{
    TEST_CASE(test_fuel_threshold);
    AlertManager mgr;
    SensorData normal = make_nominal_data();
    normal.fuel_pct = 50;
    auto alerts1 = mgr.evaluate(normal);
    ASSERT_TRUE(alerts1.empty(), "50% fuel should not trigger alert");
    ASSERT_FALSE(mgr.isFuelAlertActive(), "Fuel alert should be inactive");

    SensorData warn = make_nominal_data();
    warn.fuel_pct = 9;
    auto alerts2 = mgr.evaluate(warn);
    ASSERT_EQ(alerts2.size(), 1, "9% fuel must trigger exactly 1 alert");
    ASSERT_TRUE(mgr.isFuelAlertActive(), "Fuel alert must be active");
    ASSERT_EQ(alerts2[0].sensor_name, "FUEL", "Sensor name must be FUEL");
    ASSERT_EQ(alerts2[0].severity, "CRITICAL", "Severity must be CRITICAL");

    PASS_TEST();
}

bool test_tyre_pressure_threshold()
{
    TEST_CASE(test_tyre_pressure_threshold);
    AlertManager mgr;
    SensorData normal = make_nominal_data();
    normal.tyre_psi = 32;
    auto alerts1 = mgr.evaluate(normal);
    ASSERT_TRUE(alerts1.empty(), "32 PSI should not trigger alert");
    ASSERT_FALSE(mgr.isTyreAlertActive(), "Tyre alert should be inactive");

    SensorData warn = make_nominal_data();
    warn.tyre_psi = 25;
    auto alerts2 = mgr.evaluate(warn);
    ASSERT_EQ(alerts2.size(), 1, "25 PSI must trigger exactly 1 alert");
    ASSERT_TRUE(mgr.isTyreAlertActive(), "Tyre alert must be active");
    ASSERT_EQ(alerts2[0].sensor_name, "TYRE_PRESSURE", "Sensor name must be TYRE_PRESSURE");
    ASSERT_EQ(alerts2[0].severity, "WARNING", "Severity must be WARNING");

    PASS_TEST();
}

bool test_speed_threshold()
{
    TEST_CASE(test_speed_threshold);
    AlertManager mgr;
    SensorData normal = make_nominal_data();
    normal.speed_kmh = 100;
    auto alerts1 = mgr.evaluate(normal);
    ASSERT_TRUE(alerts1.empty(), "100 km/h should not trigger alert");
    ASSERT_FALSE(mgr.isSpeedAlertActive(), "Speed alert should be inactive");

    SensorData warn = make_nominal_data();
    warn.speed_kmh = 121;
    auto alerts2 = mgr.evaluate(warn);
    ASSERT_EQ(alerts2.size(), 1, "121 km/h must trigger exactly 1 alert");
    ASSERT_TRUE(mgr.isSpeedAlertActive(), "Speed alert must be active");
    ASSERT_EQ(alerts2[0].sensor_name, "SPEED", "Sensor name must be SPEED");
    ASSERT_EQ(alerts2[0].severity, "WARNING", "Severity must be WARNING");

    PASS_TEST();
}

bool test_no_duplicate_alert()
{
    TEST_CASE(test_no_duplicate_alert);
    AlertManager mgr;
    SensorData s1 = make_nominal_data();
    s1.speed_kmh = 125;
    auto a1 = mgr.evaluate(s1);
    ASSERT_EQ(a1.size(), 1, "First reading above limit must generate 1 alert");

    // Second reading also above limit
    SensorData s2 = make_nominal_data();
    s2.speed_kmh = 128;
    auto a2 = mgr.evaluate(s2);
    ASSERT_TRUE(a2.empty(), "Subsequent reading above limit must NOT generate duplicate alert");

    // Third reading still above limit
    SensorData s3 = make_nominal_data();
    s3.speed_kmh = 135;
    auto a3 = mgr.evaluate(s3);
    ASSERT_TRUE(a3.empty(), "Third reading above limit must NOT generate duplicate alert");

    ASSERT_EQ(mgr.getHistory().size(), 1, "History must contain only 1 alert event");
    PASS_TEST();
}

bool test_alert_clearing()
{
    TEST_CASE(test_alert_clearing);
    AlertManager mgr;
    SensorData s1 = make_nominal_data();
    s1.speed_kmh = 125;
    mgr.evaluate(s1);
    ASSERT_TRUE(mgr.isSpeedAlertActive(), "Speed alert should be active");

    // Speed drops back to normal
    SensorData s2 = make_nominal_data();
    s2.speed_kmh = 90;
    auto clear_alerts = mgr.evaluate(s2);
    ASSERT_EQ(clear_alerts.size(), 1, "Speed returning to normal must generate 1 clear event");
    ASSERT_FALSE(mgr.isSpeedAlertActive(), "Speed alert should now be inactive");
    ASSERT_EQ(clear_alerts[0].severity, "INFO", "Clear event must have severity INFO");
    ASSERT_FALSE(clear_alerts[0].active, "Alert active flag must be false");

    // Next reading normal should generate nothing
    SensorData s3 = make_nominal_data();
    s3.speed_kmh = 85;
    auto no_alerts = mgr.evaluate(s3);
    ASSERT_TRUE(no_alerts.empty(), "Stable normal speed should generate no events");

    PASS_TEST();
}

bool test_logger_file_write()
{
    TEST_CASE(test_logger_file_write);
    const std::string tmp_log = "/tmp/test_drivesense_logger.log";
    std::remove(tmp_log.c_str());

    {
        Logger logger(tmp_log);
        Alert a("ENGINE_TEMP", "CRITICAL", "Engine coolant overheating test message");
        logger.log(a);
        logger.logMessage("INFO", "SUBSYSTEM", "Direct subsystem log entry");
        logger.flush();
    }

    std::ifstream in(tmp_log);
    ASSERT_TRUE(in.is_open(), "Log file should exist and be openable");

    std::string line1, line2;
    std::getline(in, line1);
    std::getline(in, line2);

    ASSERT_TRUE(line1.find("CRITICAL") != std::string::npos, "Line 1 must contain CRITICAL");
    ASSERT_TRUE(line1.find("ENGINE_TEMP") != std::string::npos, "Line 1 must contain ENGINE_TEMP");
    ASSERT_TRUE(line1.find("overheating") != std::string::npos, "Line 1 must contain message text");

    ASSERT_TRUE(line2.find("INFO") != std::string::npos, "Line 2 must contain INFO");
    ASSERT_TRUE(line2.find("SUBSYSTEM") != std::string::npos, "Line 2 must contain SUBSYSTEM");

    std::remove(tmp_log.c_str());
    PASS_TEST();
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "      DRIVESENSE UNIT TEST SUITE        " << std::endl;
    std::cout << "========================================" << std::endl;

    bool all_pass = true;
    all_pass &= test_engine_temp_threshold();
    all_pass &= test_fuel_threshold();
    all_pass &= test_tyre_pressure_threshold();
    all_pass &= test_speed_threshold();
    all_pass &= test_no_duplicate_alert();
    all_pass &= test_alert_clearing();
    all_pass &= test_logger_file_write();

    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_tests_passed << "/" << g_tests_run << " tests passed." << std::endl;
    std::cout << "========================================" << std::endl;

    return (all_pass && (g_tests_passed == g_tests_run)) ? 0 : 1;
}
