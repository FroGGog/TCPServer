#pragma once

#include <libpq-fe.h>
#include <optional>
#include <string>

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

};