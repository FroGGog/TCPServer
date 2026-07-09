#pragma once
#include <string>
#include <chrono>
#include <unordered_map>
#include <mutex>

struct ClientInfo
{
    static constexpr int RATE_LIMIT_WINDOW_SECONDS = 60;
    static constexpr int RATE_LIMIT_MAX_REQUESTS = 100;

    ClientInfo(const std::string& api_key)
        : m_api_key(api_key), m_request_count(0), 
        m_window_start(std::chrono::steady_clock::now()) {}

    std::string m_api_key = "";
    int m_request_count = 0;
    std::chrono::steady_clock::time_point m_window_start;
    

    bool is_rate_limited()
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_window_start);

        if(elapsed.count() >= RATE_LIMIT_WINDOW_SECONDS)
        {
            m_request_count = 0;
            m_window_start = now;
        }

        return m_request_count >= RATE_LIMIT_MAX_REQUESTS;
    }
};

class ClientRegistry
{
public:
    void add_client(const std::string& api_key)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_clients.emplace(api_key, ClientInfo{api_key});
    }

    bool is_valid_key(const std::string& api_key)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_clients.count(api_key);
    }

    bool is_rate_limited(const std::string& api_key)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        auto it = m_clients.find(api_key);
        if(it == m_clients.end())
        {
            return false;
        }
        return it->second.is_rate_limited();
        
    }

    void increment_request_count(const std::string& api_key)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        auto it = m_clients.find(api_key);
        if(it != m_clients.end())
        {
            ++it->second.m_request_count;
        }
    }
    
    void load_users_data(PostgreConnection* connection)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        for(auto& key : connection->getAllAPIKeys())
        {
            m_clients.emplace(key, ClientInfo{key});
        }
    }
private:
    std::unordered_map<std::string, ClientInfo> m_clients;
    std::mutex m_mtx;
};