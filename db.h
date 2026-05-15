#pragma once

#include <libpq-fe.h>


class PostgreConnection
{
public:
    PostgreConnection();
    ~PostgreConnection();

    bool isConnected() const;
    PGconn* getConnection();

private:
    bool m_is_connected = false;
    PGconn* m_conn = nullptr;

};