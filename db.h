#pragma once

#include <libpq-fe.h>
#include <optional>
#include <string>
#include <mutex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <iostream>

class PostgreConnection
{
public:
    PostgreConnection(std::string host, std::string port, std::string dbname, 
        std::string username, std::string password);
    ~PostgreConnection();

    PostgreConnection(const PostgreConnection& other) = delete;

    PostgreConnection(PostgreConnection&& other);

    bool isConnected() const;
    PGconn* getConnection();
    
    std::optional<int64_t> saveMessage(const char* buffer);


private:
    bool m_is_connected = false;
    PGconn* m_conn = nullptr;

    std::shared_ptr<spdlog::logger> logger;
};