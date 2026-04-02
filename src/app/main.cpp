#include <app/application.h>
#include <iostream>
#include <string>

using namespace gnd;

int main(int argc, char** argv) {
    bool isHeadless = false;
    bool denoise = false;
    std::string scenePath = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--headless") {
            isHeadless = true;
        } else if (arg == "--denoise") {
            denoise = true;
        } else if (scenePath.empty()) {
            scenePath = arg;
        }
    }

    Application app(isHeadless);

    if (isHeadless) {
        if (scenePath.empty()) {
            std::cerr << "[Gianduia Headless] Error: headless mode requires path to the scene." << std::endl;
            return 1;
        }
        app.runHeadless(scenePath, denoise);
    } else {
        app.run();
    }

    return 0;
}