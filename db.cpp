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
            log_error(PQerrorMessage(m_conn));
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