#pragma once
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include "common_def.h"

namespace rpc
{
    class client;
    class server;
}

namespace eYs3D
{

class WatchDog_Client
{

public:
    WatchDog_Client();
    ~WatchDog_Client();

    watchdog_value dump_message(std::string message);

    template <typename... Args>
    watchdog_value dump_message(const char *format, Args... args)
    {
        if (!format) return watchdog_value::invalid_arguments;
        
        char message[1024];
        sprintf(message, format, args...);
        return dump_message(std::string(message));
    }

    bool is_connected();

private:
    void init();

    watchdog_value connect();
    watchdog_value disconnect();

private:
    int _port;
    std::shared_ptr<rpc::client> _client_instance;
    std::shared_ptr<rpc::server> _server_instance;

    std::vector<std::thread> _workers;

    std::atomic<bool> _running;
};

}