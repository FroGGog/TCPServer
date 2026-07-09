#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cstdio>
#include <atomic>
#include <csignal>
#include <mutex>
#include <thread>
#include <chrono>
#include <fstream>
#include <queue>

#include "db.h"
#include "utils.h"
#include "rate_limiter.h"

#include "nlohmann/json.hpp"

using json = nlohmann::json;

std::atomic<bool> running{true};
std::atomic<int> active_clients{0};
std::atomic<uint64_t> requests_handled{0};
std::atomic<int> test_api_counter{0};
static int g_server_socket = -1;

std::chrono::time_point server_start_time = std::chrono::steady_clock::now();


Config CONFIG;

class ConnectionPool
{
public:
    ConnectionPool(const Config& cfg, size_t pool_size)
    {
        m_pool.reserve(pool_size);

        for(size_t i = 0; i < pool_size; ++i)
        {
            PostgreConnection p_connection {cfg.host, std::to_string(cfg.db_port), 
            cfg.db_name, cfg.db_username, cfg.db_password};

            if(!p_connection.isConnected())
            {
                spdlog::error("Failed to create connection #{}", i);
                throw std::runtime_error("Database connection failed");
            }

            m_pool.push_back(std::move(p_connection));
            m_free_indices.push(i);
        }
        spdlog::info("Connection pool created with {} connections", pool_size);
    }

    size_t acquire()
    {
        std::unique_lock<std::mutex> lock(m_mtx);

        m_cv.wait(lock, [this]{return !m_free_indices.empty();});

        size_t index = m_free_indices.front();
        m_free_indices.pop();

        auto& conn = m_pool[index];
        if (PQstatus(conn.getConnection()) != CONNECTION_OK) {
            spdlog::warn("Connection #{} is dead, attempting reset...", index);
            PQreset(conn.getConnection());
        }
        return index;
    }

    void release(size_t con_index)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_free_indices.push(con_index);
        m_cv.notify_one();
    }

    PostgreConnection& getConnection(size_t index)
    {
        return m_pool[index];
    }


private:
    std::vector<PostgreConnection> m_pool;
    std::queue<size_t> m_free_indices;
    std::mutex m_mtx;
    std::condition_variable m_cv;

};

class ConnectionGuard
{
public:
    ConnectionGuard(ConnectionPool& pool, size_t idx)
        : m_pool(pool), m_idx(idx) {}    
        
    ~ConnectionGuard()
    {
        if(!m_released)
        {
            m_pool.release(m_idx);
        }
    }

    ConnectionGuard(const ConnectionGuard&) = delete;
    ConnectionGuard& operator=(const ConnectionGuard&) = delete;

    ConnectionGuard(ConnectionGuard&& other) noexcept
        : m_pool(other.m_pool), m_idx(other.m_idx), m_released(other.m_released)
    {
        other.m_released = true;
    }

    PostgreConnection& get()
    {
        return m_pool.getConnection(m_idx);
    }

private:
    ConnectionPool& m_pool;
    size_t m_idx = 0;
    bool m_released = false;
};

struct ClientCounterGuard
{
    std::atomic<int>& counter;

    explicit ClientCounterGuard(std::atomic<int>& c)
        : counter(c)
    {
        ++counter;
    }
    ~ClientCounterGuard()
    {
        --counter;
    }
};

void signal_handler(int)
{
    running.store(false);
    if(g_server_socket >= 0)
    {
        close(g_server_socket);
    }
}

void handleUser(int accept_socket, ConnectionPool& pool, ClientRegistry& c_registry)
{
    ClientCounterGuard guard{active_clients};
    constexpr size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE] = {};

    while(true)
    {
        int bytes_recived = recv(accept_socket, buffer, sizeof(buffer) - 1, 0);
        if(bytes_recived <= 0)
        {
            break;
        }

        buffer[bytes_recived] = '\0';

        try
        {
            json request = json::parse(buffer);

            if(request.contains("action"))
            {
                ++requests_handled;
                if(request["action"] == "save")
                {
                    std::string api_key = request.value("api_key", "");

                    if(api_key.empty() || !c_registry.is_valid_key(api_key))
                    {
                        json response = {{"error", "invalid api_key"}};
                        std::string response_str = response.dump() + '\n';
                        if(send(accept_socket, response_str.c_str(), response_str.size(), 0) < 0)
                        {
                            spdlog::error("Send failed: {}", accept_socket);                    
                        }
                        return;
                    }
                    
                    if(c_registry.is_rate_limited(api_key))
                    {
                        json response = {{"error", "429 Too Many Requests"}};
                        std::string response_str = response.dump() + '\n';
                        if(send(accept_socket, response_str.c_str(), response_str.size(), 0) < 0)
                        {
                            spdlog::error("Send failed: {}", accept_socket);                    
                        }
                        return;
                    }
                    c_registry.increment_request_count(api_key);

                    std::string value = request["value"].get<std::string>();
                    
                    auto conn_guard = ConnectionGuard{pool, pool.acquire()};
                    auto& connection = conn_guard.get();

                    
                    if(auto id = connection.saveMessage(value.c_str()))
                    {
                        json response = {{"id", *id}};
                        std::string response_str = response.dump() + '\n';
                        if(send(accept_socket, response_str.c_str(), response_str.size(), 0) < 0)
                        {                        
                            spdlog::error("Send failed: {}", accept_socket);
                            return;
                        }
                        spdlog::info("Saved message with id: {}", id.value());
                    }
                    else
                    {
                        json response = {{"error", "db_insert_failed"}};
                        std::string response_str = response.dump() + '\n';
                        if(send(accept_socket, response_str.c_str(), response_str.size(), 0) < 0)
                        {
                            spdlog::error("Send failed: {}", accept_socket);                    
                            return;
                        }

                        spdlog::error("Saving message failed");
                    }
                }
                else if(request["action"] == "health")
                {
                    auto end_time = std::chrono::steady_clock::now();

                    auto server_upkeep_time = std::chrono::duration_cast<std::chrono::seconds>(end_time - server_start_time);
                
                    json response = {{"status", "ok"}, {"uptime", server_upkeep_time.count()}};

                    std::string response_str = response.dump() + '\n';
                    if(send(accept_socket, response_str.c_str(), response_str.size(), 0) < 0)
                    {
                        spdlog::error("Send failed: {}", accept_socket);                    
                        return;
                    }
                    spdlog::info("Send server status info");
                }
                else if(request["action"] == "stats")
                {
                    json response = {{"active_clinets", active_clients.load()}, {"requests_handled", requests_handled.load()}};
                    std::string response_str = response.dump() + '\n';
                    if(send(accept_socket, response_str.c_str(), response_str.size(), 0) < 0)
                    {
                        spdlog::error("Send failed: {}", accept_socket);                    
                        return;
                    }
                    spdlog::info("Send server stats info");

                }
                else if(request["action"] == "register")
                {
                    auto conn_guard = ConnectionGuard{pool, pool.acquire()};
                    auto& connection = conn_guard.get();

                    std::string generated_api_key = generateRandomString(16);
                    json response = {{"api_key", generated_api_key}};
                    std::string response_str = response.dump() + '\n';
                    if(send(accept_socket, response_str.c_str(), response_str.size(), 0) < 0)
                    {
                        spdlog::error("Send failed: {}", accept_socket);                    
                        return;
                    }
                    connection.saveUser(generated_api_key);
                    c_registry.add_client(generated_api_key);

                    ++test_api_counter;
                    spdlog::info("Registrated client with api_key: {}", generated_api_key);
                }
            }
            else
            {
                json response = {{"error", "unknown_action"}};
                std::string response_str = response.dump() + '\n';                
                spdlog::error("Unknow action: {}", response_str);

                if(send(accept_socket, response_str.c_str(), response_str.length(), 0) < 0)
                {
                    spdlog::error("Send error response failed");
                    return;
                }
                return;
            }
        }
        catch (const json::exception& e)
        {
            spdlog::error("Json parse failure: {}", e.what());

            json response = {{"error", "invalid_json"}};
            std::string response_str = response.dump() + '\n';
            if(send(accept_socket, response_str.c_str(), response_str.length(), 0) < 0)
            {
                spdlog::error("Send error response failed");
                return;
            }
        }
    }
    close(accept_socket);
}


int startServer(ConnectionPool& pool, ClientRegistry& c_registry)
{
    const char* host = CONFIG.host.c_str();
    int PORT = CONFIG.server_port;

    sockaddr_in server_param = {};
    server_param.sin_family = AF_INET;
    server_param.sin_port = htons(PORT);
    int res = inet_pton(AF_INET, host, &server_param.sin_addr);
    if(res == 0)
    {
        spdlog::error("Server invalid IP format {}", res);
        return 1;
    }
    if(res < 0)
    {
        spdlog::error("Invalid inet_pton invalid argument {}", res);
        return 1;
    }

    g_server_socket = socket(server_param.sin_family, SOCK_STREAM, 0);
    if(g_server_socket < 0)
    {
        spdlog::error("Socket creation failure: {}", g_server_socket);
        return 1;
    }

    int opt = 1;
    if (setsockopt(g_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) 
    {
        spdlog::error("Setsockopt error");
        close(g_server_socket);
        return 1;
    }


    if(bind(g_server_socket, (sockaddr*)&server_param, sizeof(server_param)) < 0)
    {
        spdlog::error("Server socket binding failure"); 
        return 1;
    }

    if(listen(g_server_socket, 128) < 0)
    {
        spdlog::error("Listening to server socket failure");
        return 1;
    }

    spdlog::info("Started server on port {}", PORT);

    while(true)
    {
        int accept_socket = accept(g_server_socket, NULL, NULL);
        if(accept_socket < 0)
        {
            if(errno == EINTR || errno == EBADF)
            {
                if(!running.load())
                {
                    break;
                }
            }
            spdlog::error("Accept socket failure: {}", accept_socket);
            continue;
        }
        
        spdlog::info("Connected: {}", accept_socket);
        std::thread j{handleUser, accept_socket, std::ref(pool), std::ref(c_registry)};

        j.detach();
    }
    
    close(g_server_socket);
    g_server_socket = -1;
    return 0;
}


int main()
{

    try {
        auto logger = spdlog::rotating_logger_mt("server", "logs/server.log", 1024 * 1024, 3);

        logger->flush_on(spdlog::level::info);

        spdlog::set_default_logger(logger);

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log init failed: " << ex.what() << "\n";
        return 1;
    }

    try
    {
        std::fstream fs;
        fs.open("bin/config.json", std::ios::in);
        if(!fs.is_open())
        {
            spdlog::error("Failed to open config.json");
            return 1;
        }

        json config = json::parse(fs);

        CONFIG.host = config.value("host", "127.0.0.1");
        CONFIG.server_port = config.value("server_port", 4124);

        CONFIG.db_port = config.value("db_port", 5432);
        CONFIG.db_name = config.value("db_name", "TCPServer");
        CONFIG.db_username = config.value("user", "postgres");
        CONFIG.db_password = config.value("password", "123");
    }
    catch (json::exception& e)
    {
        spdlog::error("Json config parse failure: {}", e.what());
        return 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    ConnectionPool p_connections{CONFIG, 5};
    ClientRegistry client_registry;
    
    {
        auto conn_guard = ConnectionGuard{p_connections, p_connections.acquire()};
        client_registry.load_users_data(&conn_guard.get());
    }

    startServer(p_connections, client_registry);

    spdlog::info("Shutting down... {} active clients", active_clients.load());
    int wait_cycles = 0;
    while (active_clients.load() > 0 && wait_cycles < 30)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ++wait_cycles;
    }

    spdlog::info("Server closed");

    return 0;
}