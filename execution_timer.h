#ifndef EXECUTION_TIMER_H
#define EXECUTION_TIMER_H

#include <chrono>
#include <iostream>
#include <ostream>
namespace Color {
    enum Code {
        FG_RED = 31,
        FG_GREEN = 32,
        FG_BLUE = 34,
        FG_DEFAULT = 39,
        BG_RED = 41,
        BG_GREEN = 42,
        BG_BLUE = 44,
        BG_DEFAULT = 49
    };
    std::ostream& operator<<(std::ostream& os, Code code);
}

class ExecutionTimer {
private:
    std::string name;
    std::chrono::steady_clock::time_point start;

public:
ExecutionTimer(std::string name) : name(name), start(std::chrono::high_resolution_clock::now()) {}

  ~ExecutionTimer() {
    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << Color::FG_BLUE << name << Color::FG_DEFAULT << " " << duration.count() << std::endl;
  }
};

#endif // EXECUTION_TIMER_H
