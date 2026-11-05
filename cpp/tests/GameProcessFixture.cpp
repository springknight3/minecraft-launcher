#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

int main(int argc, char** argv)
{
    const std::string_view mode = argc > 1 ? argv[1] : "";
    std::cout << "Sound engine started\n" << std::flush;
    if (mode == "crash")
    {
        return 9;
    }
    if (mode == "wait")
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 0;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return 0;
}
