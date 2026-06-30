#include "db.h"

PostgreConnection::PostgreConnection()
{
    try {
        logger = spdlog::rotating_logger_mt("db_logger", "logs/db_server.log", 1024 * 1024, 3);

        logger->flush_on(spdlog::level::info);
        
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log init failed: " << ex.what() << "\n";
    }   

    const char* conninfo = 
        "host=127.0.0.1 "
        "port=5432 "
        "dbname=TCPServer "
        "user=postgres "
        "password=123";

    m_conn = PQconnectdb(conninfo);
    if(PQstatus(m_conn) != CONNECTION_OK)
    {
        logger->error("DB connection failed: {}", PQerrorMessage(m_conn));
        PQfinish(m_conn);
        m_is_connected = false;
    }
    else
    {
        logger->info("DB connection established");
        m_is_connected = true;
    }
}

PostgreConnection::~PostgreConnection()
{
    PQfinish(m_conn);
    logger->info("DB connection closed");
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
    std::lock_guard<std::mutex> lock(db_mutex);
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
        logger->error("DB save message failure: {}", PQerrorMessage(m_conn));
        PQclear(res);
        return std::nullopt;
    }

    int64_t id = std::atoll(PQgetvalue(res, 0, 0));
    PQclear(res);
    return id;
}
