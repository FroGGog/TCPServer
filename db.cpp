#include "db.h"
#include "utils.h"


PostgreConnection::PostgreConnection()
    {
        const char* conninfo = 
            "host=127.0.0.1 "
            "port=5432 "
            "dbname=TCPServer "
            "user=postgres "
            "password=123";
    
        m_conn = PQconnectdb(conninfo);
        if(PQstatus(m_conn) != CONNECTION_OK)
        {
            log_db_error(PQerrorMessage(m_conn));
            PQfinish(m_conn);
            m_is_connected = false;
        }
        else
        {
            m_is_connected = true;
        }
    }

PostgreConnection::~PostgreConnection()
{
    PQfinish(m_conn);
    std::cout << "DB connection closed\n";
}

bool PostgreConnection::isConnected() const
{ 
    return m_is_connected;
}

PGconn* PostgreConnection::getConnection()
{
    return m_conn;
}

std::optional<int64_t> PostgreConnection::saveMessage(const char* buffer)
{
    const char* param_values[1] = { buffer };

    PGresult* res = PQexecParams(
        m_conn,
        "INSERT INTO messages (content, recived_at) VALUES ($1, NOW()) RETURNING id",
        1,
        NULL,
        param_values,
        NULL,
        NULL,
        0
    );


    if(PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
    {
        log_db_error(PQerrorMessage(m_conn));
        PQclear(res);
        return std::nullopt;
    }

    int64_t id = std::atoll(PQgetvalue(res, 0, 0));
    PQclear(res);
    return id;
}
