#include <chrono>
#include <thread>

extern "C" void SystickHandler(void);

void setup(void);
void loop(void);

int main()
{
    setup();

    std::thread([] {
        auto next = std::chrono::steady_clock::now();
        while (true) {
            next += std::chrono::milliseconds(10);
            SystickHandler();
            std::this_thread::sleep_until(next);
        }
    }).detach();

    while (true) {
        loop();
    }

    return 0;
}
