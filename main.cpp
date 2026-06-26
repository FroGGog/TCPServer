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

#include "utils.h"
#include "db.h"

#include "nlohmann/json.hpp"

using json = nlohmann::json;

std::mutex mutex;
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

void handleUser(int accept_socket, PostgreConnection& connection, std::mutex& mutex)
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
                        std::lock_guard<std::mutex> lock{mutex};
                        log_error("Failed to send saved message id");
                        return;
                    }
                    std::lock_guard<std::mutex> lock{mutex};
                    log(std::string{"Saved message with id: " + std::to_string(id.value())}.c_str());
                }
                else
                {
                    json response = {{"error", "db_insert_failed"}};
                    std::string response_str = response.dump();
                    if(send(accept_socket, response_str.c_str(), response_str.size(), 0) < 0)
                    {
                        std::lock_guard<std::mutex> lock{mutex};
                        log_error("Failed to send error message");
                        return;
                    }
                    std::lock_guard<std::mutex> lock{mutex};
                    log_db_error("Failed to save message");
                }
            }
            else
            {
                json response = {{"error", "unknown_action"}};
                std::string response_str = response.dump();
                std::lock_guard<std::mutex> lock{mutex};
                log_error("Unknown action");

                if(send(accept_socket, response_str.c_str(), response_str.length(), 0) < 0)
                {
                    std::lock_guard<std::mutex> lock{mutex};
                    log_error("Failed to send error response");
                    return;
                }
                return;
            }
        }
        catch (const json::exception& e)
        {
            log_error("JSON PARSE");
            json response = {{"error", "invalid_json"}};

            std::string response_str = response.dump();
            if(send(accept_socket, response_str.c_str(), response_str.length(), 0) < 0)
            {
                std::lock_guard<std::mutex> lock{mutex};
                log_error("Failed to send error response");
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
        log_error("Invalid IP format");
        return 1;
    }
    if(res < 0)
    {
        log_error("Inter_pton invalid argument");
        return 1;
    }

    g_server_socket = socket(server_param.sin_family, SOCK_STREAM, 0);
    if(g_server_socket < 0)
    {
        log_error("Socket creation");
        return 1;
    }

    int opt = 1;
    if (setsockopt(g_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) 
    {
        log_error("setsockopt");
        close(g_server_socket);
        return 1;
    }


    if(bind(g_server_socket, (sockaddr*)&server_param, sizeof(server_param)) < 0)
    {
        log_error("socket binding");
        return 1;
    }

    if(listen(g_server_socket, 1) < 0)
    {
        log_error("socket listen");
        return 1;
    }

    log(std::string{"Started server on port " + std::to_string(PORT)}.c_str());

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
            log_error("Accept socket");
            continue;
        }

        log(std::string{ "Connected " + std::to_string(accept_socket)}.c_str());
        std::thread j{handleUser, accept_socket, std::ref(connection), std::ref(mutex)};

        j.detach();
    }
    
    close(g_server_socket);
    g_server_socket = -1;
    return 0;
}


int main()
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    PostgreConnection p_connection;

    if(!p_connection.isConnected())
    {
        log_error("DB connection failed.");
        return 1;
    }

    startServer(p_connection);


    log_error("Server closed.");

    return 0;
}