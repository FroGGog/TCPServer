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

#include "db.h"
#include "utils.h"

#include "nlohmann/json.hpp"

using json = nlohmann::json;

std::atomic<bool> running{true};
std::atomic<int> active_clients{0};
static int g_server_socket = -1;

ServerData DATA;


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

void handleUser(int accept_socket, PostgreConnection& connection)
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

            if(request.contains("action") && request["action"] == "save")
            {
                std::string value = request["value"].get<std::string>();
                
                if(auto id = connection.saveMessage(value.c_str()))
                {
                    json response = {{"id", *id}};
                    std::string response_str = response.dump();
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
                    std::string response_str = response.dump();
                    if(send(accept_socket, response_str.c_str(), response_str.size(), 0) < 0)
                    {
                        spdlog::error("Send failed: {}", accept_socket);                    
                        return;
                    }

                    spdlog::error("Saving message failed");
                }
            }
            else
            {
                json response = {{"error", "unknown_action"}};
                std::string response_str = response.dump();                
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
            std::string response_str = response.dump();
            if(send(accept_socket, response_str.c_str(), response_str.length(), 0) < 0)
            {
                spdlog::error("Send error response failed");
                return;
            }
        }
    }
    close(accept_socket);
}


int startServer(PostgreConnection& connection)
{
    const char* host = DATA.host.c_str();
    int PORT = DATA.server_port;

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

    if(listen(g_server_socket, 1) < 0)
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
        std::thread j{handleUser, accept_socket, std::ref(connection)};

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

        DATA.host = config.value("host", "127.0.0.1");
        DATA.server_port = config.value("server_port", 4124);

        DATA.db_port = config.value("db_port", 5432);
        DATA.db_name = config.value("db_name", "TCPServer");
        DATA.db_username = config.value("user", "postgres");
        DATA.db_password = config.value("password", "123");
    }
    catch (json::exception& e)
    {
        spdlog::error("Json config parse failure: {}", e.what());
        return 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    PostgreConnection p_connection {DATA.host, std::to_string(DATA.db_port), 
            DATA.db_name, DATA.db_username, DATA.db_password};

    if(!p_connection.isConnected())
    {
        spdlog::error("DB connection failure");
        return 1;
    }

    startServer(p_connection);

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