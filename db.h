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
    PostgreConnection();
    ~PostgreConnection();

    bool isConnected() const;
    PGconn* getConnection();
    
    std::optional<int64_t> saveMessage(const char* buffer);


private:
    bool m_is_connected = false;
    PGconn* m_conn = nullptr;

    std::shared_ptr<spdlog::logger> logger;

    std::mutex db_mutex;

};