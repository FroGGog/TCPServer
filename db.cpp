#include "db.h"

PostgreConnection::PostgreConnection(std::string host, std::string port, std::string dbname, 
        std::string username, std::string password)
{
    try {

        logger = spdlog::get("db_logger");
        if(!logger)
        {
            logger = spdlog::rotating_logger_mt("db_logger", "logs/db_server.log", 1024 * 1024, 3);
            logger->flush_on(spdlog::level::info);
        }
        
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log init failed: " << ex.what() << "\n";
    }   

    std::string str_conninfo =
        "host=" + host + ' ' +
        "port=" + port + ' ' +
        "dbname=" + dbname + ' ' +
        "user=" + username + ' ' +
        "password=" + password;

    const char* conninfo = str_conninfo.c_str();

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
