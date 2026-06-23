#include "utils.h"

void log(const char* msg)
{
    std::cerr << "[LOG] " << msg << '\n';
}

void log_error(const char* msg)
{
    std::cerr << "[Error] " << msg << " | " << std::strerror(errno)  << '\n';
}

void log_db_error(const char* msg)
{
    std::cerr << "[DB Error] " << msg << '\n';
}