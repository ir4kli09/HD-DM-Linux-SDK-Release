#include "WatchDog_Client.h"
#include "private_def.h"
#include "rpc/client.h"
#include "rpc/server.h"
#include "rpc/rpc_error.h"
#include "utility.h"
#include <fstream>
#include <unistd.h>
#include <iostream>
#include "logger.h"

namespace eYs3D{

WatchDog_Client::WatchDog_Client()
{    
    if (ok != connect())
    {
        LOG_DEBUG("[Client] Watchdog can't connected server\n");
        disconnect();
    }
}

WatchDog_Client::~WatchDog_Client()
{
    disconnect();
}

watchdog_value WatchDog_Client::dump_message(std::string message)
{
    if (!is_connected()) return disconnectd;

    LOG_DEBUG("[Client] dump_message %s\n", message.c_str());

    try{
        auto result = _client_instance->call(get_event_function(watchdog_event::dump_message), message).as<int>();

        return (watchdog_value)result;
    }
    catch(rpc::timeout t)
    {
        return timeout;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what();
        return error;
    }
}

watchdog_value WatchDog_Client::connect()
{
    for (int port : get_port_list())
    {
        _client_instance = watchdog_client_facotry(port);
        if (!_client_instance)
            continue;

        try
        {
            port = _client_instance->call(get_event_function(watchdog_event::handshake)).as<int>();
        }
        catch (const rpc::rpc_error &e)
        {
            _client_instance.reset();
            continue;
        }

        for (int i = 0; i < 10; ++i)
        {
            try
            {
                _server_instance = std::make_shared<rpc::server>(port + i);
            }
            catch (const std::system_error &e)
            {
                continue;
            }

            _server_instance->async_run();

            init();

            try 
            {
                int result = _client_instance->call(get_event_function(watchdog_event::handshake_acknowledge), port + i).as<int>();

                if (ok != result)
                    continue;

                LOG_INFO("[Client] Connected server success. %s\n", message.c_str());
                return ok;
            }
            catch (const std::exception &e)
            {
                continue;
            }        
        }
    }

    _client_instance.reset();
    _server_instance.reset();

    return (watchdog_value)disconnectd;
}

watchdog_value WatchDog_Client::disconnect()
{
    _running = false;
    for (std::thread &worker : _workers)
    {
        worker.join();
    }

    if (is_connected())
    {
        try
        {
            auto result = _client_instance->call(get_event_function(watchdog_event::exit)).as<int>();
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what();
        }

        _server_instance->stop();
    }

    _client_instance.reset();
    _server_instance.reset();
}

void WatchDog_Client::init()
{
    _server_instance->bind(get_event_function(watchdog_event::request_process_name), [this]() {
        
        LOG_INFO("[Client] Wathdog received signal(request_process_name).\n");

        char result[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);

        return std::string(result, (count > 0) ? count : 0);
    });

    _server_instance->bind(get_event_function(watchdog_event::request_pid), [this]() {
        
        LOG_INFO("[Client] Wathdog received signal(request_pid).\n");
        return getpid();
    });

    _running = true;
    _workers.emplace_back([this]() {
        
        while (_running)
        {
            try
            {
                _client_instance->call(get_event_function(watchdog_event::heartbeat)).as<int>();
                
            }
            catch (const rpc::timeout &t)
            {

            }
            catch (const std::exception &e)
            {
                std::cerr << e.what();
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}

bool WatchDog_Client::is_connected()
{
    return _client_instance && _server_instance;
}

}