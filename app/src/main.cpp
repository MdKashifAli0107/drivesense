#include "App.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    try {
        if (argc == 1) {
            App app;
            return app.runInteractive();
        }

        std::string arg1 = argv[1];

        if (arg1 == "--help" || arg1 == "-h") {
            App::printHelp(argv[0]);
            return 0;
        }

        if (arg1 == "--once") {
            App app;
            return app.runOnce();
        }

        if (arg1 == "--start") {
            App app;
            return app.runStart();
        }

        if (arg1 == "--stop") {
            App app;
            return app.runStop();
        }

        if (arg1 == "--reset") {
            App app;
            return app.runReset();
        }

        if (arg1 == "--stats") {
            App app;
            return app.runStats();
        }

        if (arg1 == "--inject") {
            if (argc < 3) {
                std::cerr << "Error: --inject requires a fault name argument.\n"
                          << "Valid options: overheat, lowfuel, flattyre, overspeed\n";
                return 1;
            }
            App app;
            return app.runInjectFault(argv[2]);
        }

        std::cerr << "Unknown option: " << arg1 << "\n";
        App::printHelp(argv[0]);
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "DriveSense Application Error: " << e.what() << std::endl;
        return 1;
    }
}
