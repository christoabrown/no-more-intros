#include "execution_timer.h"

std::ostream& Color::operator<<(std::ostream& os, Code code)
{
    return os << "\033[" << static_cast<int>(code) << "m";
}
