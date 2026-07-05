#pragma once
#include <string>

struct Config
{
    std::string host;

    std::string db_name;
    std::string db_username;
    std::string db_password;
    int db_port;

    int server_port;
};

