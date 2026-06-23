#pragma once

#include <iostream>
#include <cerrno>

void log(const char* msg);
void log_error(const char* msg);
void log_db_error(const char* msg);