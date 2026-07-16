#pragma once
#include <string>
#include <random>
#include <chrono>

struct Config
{
    std::string server_host;
    std::string db_host; 
    std::string db_name;
    std::string db_username;
    std::string db_password;
    int db_port;

    int server_port;
};

std::string generateRandomString(int lenght);

