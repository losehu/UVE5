#include <chrono>
#include <thread>
#include "Arduino.hpp"

extern "C" void SystickHandler(void);

void setup(void);
void loop(void);

int main(int argc, char **argv)
{
    OPENCV_SetRestartArgs(argc, argv);

    std::thread([] {
        using namespace std::chrono;
        auto next = steady_clock::now();
        while (true) {
            next += 10ms;
            std::this_thread::sleep_until(next);
            SystickHandler();
            // 若严重落后，可选择 next = steady_clock::now(); 防止疯狂补课
        }
    }).detach();

    for (;;) {
        try {
            setup();
            loop();
            return 0;
        } catch (const EspRestartException &) {
        }
    }
}
