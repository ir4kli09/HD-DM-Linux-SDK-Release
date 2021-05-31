#pragma once

#include <string>
#include <vector>

#define RESTART_THRESHOLD_SEC 5

namespace eYs3D
{    

struct server_status
{
    bool enabled_restart_client = true;
    int restart_threshold_sec = RESTART_THRESHOLD_SEC;
};

struct client_parameters
{
    std::string _client_process_name = "";
    int _client_pid = -1; 
};

static const std::vector<int> get_port_list()
{
    static const std::vector<int> port_list = {16581, 12315, 9925};
    return port_list;
}

enum class watchdog_event
{
    handshake,
    handshake_acknowledge,
    dump_message,
    request_process_name,
    request_pid,
    heartbeat,
    exit,
};

static std::string get_event_function(watchdog_event event)
{
    switch (event)
    {
        case watchdog_event::handshake:
            return "handshake";
        case watchdog_event::handshake_acknowledge:
            return "handshake_acknowledge";
        case watchdog_event::dump_message:
            return "dump_message";
        case watchdog_event::request_process_name:
            return "request_process_name";
        case watchdog_event::request_pid:
            return "request_pid";
        case watchdog_event::heartbeat : 
            return "heartbeat";
        case watchdog_event::exit:
            return "exit";
    }

    return "temp";
}

}


