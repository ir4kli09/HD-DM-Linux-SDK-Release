#pragma once
#include <memory>
#include <thread>
#include <vector>
#include <mutex>

namespace rpc
{
    class server;
    class client;
}

namespace eYs3D{

struct server_status;
struct client_parameters;
class WatchDog_Server
{

public:

    WatchDog_Server();
    ~WatchDog_Server();

    void spin();

private:
    void init();

    bool has_client();
    void ack_heartbeat();
    void restart_client();
    void client_exit();

    void dump_message(std::string message);

    void dump_message(const char *message)
    {
        dump_message(std::string(message));
    }

    template <typename... Args>
    void dump_message(const char *format, Args... args)
    {
        if (!format)
            return;

        char message[1024];
        sprintf(message, format, args...);

        dump_message(std::string(message));
    }

private:
    int _port;

    std::shared_ptr<rpc::server> _server_instance;
    std::shared_ptr<rpc::client> _client_instance;

    std::vector<std::thread> _workers;
    std::mutex _client_mutex;

    std::shared_ptr<server_status> _server_status;
    std::shared_ptr<client_parameters> _client_parameters;

    FILE *_file;
};
}