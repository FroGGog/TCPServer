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

#include "db.h"

#include "nlohmann/json.hpp"

using json = nlohmann::json;

std::atomic<bool> running{true};
static int g_server_socket = -1;

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
            spdlog::error("Json parse failure");

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
    const char* host = "127.0.0.1";
    int PORT = 4124;

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
        spdlog::error("Setsockopt erro");        
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

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    PostgreConnection p_connection;

    if(!p_connection.isConnected())
    {
        spdlog::error("DB connection failure");
        return 1;
    }

    startServer(p_connection);

    spdlog::info("Server closed");

    return 0;
}