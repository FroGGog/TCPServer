#include "utils.h"

void log_error(const char* msg)
{
    std::cerr << "Error: " << msg << " | " << std::strerror(errno)  << '\n';
}