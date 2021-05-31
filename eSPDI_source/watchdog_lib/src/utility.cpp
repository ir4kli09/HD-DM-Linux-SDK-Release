#include "utility.h"

namespace eYs3D
{

std::tm *get_current_time()
{
    std::time_t raw_time = std::time(nullptr);
    std::tm *time = std::localtime(&raw_time);

    if (!time)
        return nullptr;

    time->tm_year += 1900;
    time->tm_mon += 1;
}

std::string get_dump_time_format(std::tm time)
{
    return std::to_string(time.tm_year) + "/" +
            std::to_string(time.tm_mon) + "/" +
            std::to_string(time.tm_mday) + " " +
            std::to_string(time.tm_hour) + ":" +
            std::to_string(time.tm_min) + ":" +
            std::to_string(time.tm_sec);
}

std::shared_ptr<rpc::client> watchdog_client_facotry(int port, int timeout_ms)
{
    std::shared_ptr<rpc::client> client = std::make_shared<rpc::client>("localhost", port);

    do
    {
        if (rpc::client::connection_state::connected == client->get_connection_state())
        {
            client->set_timeout(3000);
            return client;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        timeout_ms -= 100;

    } while (timeout_ms > 0);

    return nullptr;
}

}