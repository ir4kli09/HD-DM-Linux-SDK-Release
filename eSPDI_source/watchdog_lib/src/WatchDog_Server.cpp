#include "WatchDog_Server.h"
#include "private_def.h"
#include "rpc/server.h"
#include "rpc/client.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include "utility.h"
#include "common_def.h"
#include "rpc/rpc_error.h"
#include "logger.h"
namespace eYs3D
{

    extern int port_list[];

    WatchDog_Server::WatchDog_Server()
    {
        for (int port : get_port_list())
        {
            try
            {
                _server_instance = std::make_shared<rpc::server>(port);
            }
            catch (const std::system_error &e)
            {
                _server_instance.reset();
                continue;
            }

            init();
            _server_instance->async_run();
            _port = port;

            char log_file_path[512];
            if (!getcwd(log_file_path, 512))
            {
                throw std::runtime_error("Get curret path failed.\n");
            }

            std::tm *time = get_current_time();

            sprintf(log_file_path, "%s/eYs3D_dump_%04d%02d%02d_%02d%02d%02d.txt",
                    log_file_path,
                    time->tm_year, time->tm_mon, time->tm_mday,
                    time->tm_hour, time->tm_min, time->tm_sec);

            _file = fopen(log_file_path, "wt");
            if (!_file)
            {
                throw std::runtime_error("Create log file failed.\n");
            }

            EYS3D_INFO("[Server] Wathdog server established!\n");

            return;
        }

        throw std::runtime_error("Establish watchdog server failed.\n");
    }

    WatchDog_Server::~WatchDog_Server()
    {
        _server_instance->stop();

        if (_file)
        {
            fclose(_file);
        }

        EYS3D_INFO("[Server] Wathdog server destroyed!\n");
    }

    void WatchDog_Server::init()
    {
        _server_status = std::make_shared<server_status>();
        _client_parameters = std::make_shared<client_parameters>();

        _server_instance->bind(get_event_function(watchdog_event::handshake), [this]() {
            
            EYS3D_INFO("[Server] Wathdog received signal(handshake)\n");

            if (_client_instance)
            {
                EYS3D_INFO("[Server] Wathdog server already has client connected\n");
                return (int)watchdog_value::error;
            }            

            return _port;
        });

        _server_instance->bind(get_event_function(watchdog_event::handshake_acknowledge), [this](int port) {
            
            _client_instance = watchdog_client_facotry(port);
            if (!_client_instance)
                (int)watchdog_value::disconnectd;

            try{
                _client_parameters->_client_process_name = _client_instance->call(get_event_function(watchdog_event::request_process_name)).as<std::string>();

                _client_parameters->_client_pid = _client_instance->call(get_event_function(watchdog_event::request_pid)).as<int>();
            }
            catch(const rpc::timeout &t)
            {
                LOG_DEBUG("[Server] Wathdog get client attribute timeout\n");
            }
            catch (const std::exception &e)
            {
                LOG_DEBUG("[Server] Wathdog get client attribute failed [%s]\n", e.what());
            }

            EYS3D_INFO("[Server] Wathdog received signal(handshake_acknowledge) [%s, %d]\n",
                        _client_parameters->_client_process_name.c_str(),
                        _client_parameters->_client_pid);

            _workers.emplace_back([this]() {
                while (true)
                {
                    {
                        std::lock_guard<std::mutex> lock(_client_mutex);
                        if (!has_client())
                        {
                            break;
                        }
                    }

                    if (_server_status->restart_threshold_sec < 0)
                    {
                        restart_client();
                    }

                    --_server_status->restart_threshold_sec;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            });

            return (int) watchdog_value::ok;
        });

        _server_instance->bind(get_event_function(watchdog_event::heartbeat), [this]() {
            ack_heartbeat();
            EYS3D_DEBUG("[Server] Wathdog received signal(heartbeat)\n");
            return (int)watchdog_value::ok;
        });

        _server_instance->bind(get_event_function(watchdog_event::exit), [this]() {
            EYS3D_INFO("[Server] Wathdog received signal(exit)\n");
            ack_heartbeat();
            client_exit();
            return (int)watchdog_value::ok;
        });

        _server_instance->bind(get_event_function(watchdog_event::dump_message), [this](std::string &message) {
            EYS3D_INFO("[Server] Wathdog received signal(dump_message)\n");
            dump_message(message);
            return (int)watchdog_value::ok;
        });
    }

    bool WatchDog_Server::has_client()
    {
        return _client_instance &&
               !_client_parameters->_client_process_name.empty() &&
               _client_parameters->_client_pid != -1;
    }

    void WatchDog_Server::ack_heartbeat()
    {
        _server_status->restart_threshold_sec = RESTART_THRESHOLD_SEC;
    }

    void WatchDog_Server::restart_client()
    {
        if (!has_client()) return;

        dump_message("restart_client+\n");

        char kill_process_command[1024];
        sprintf(kill_process_command, "kill %d -9", _client_parameters->_client_pid);

        char restart_command[1024];
        sprintf(restart_command, "%s &", _client_parameters->_client_process_name.c_str());

        client_exit();
        system(kill_process_command);
        system(restart_command);

        dump_message("restart_client-\n");
    }

    void WatchDog_Server::client_exit()
    {
        dump_message("client_exit+\n");

        std::lock_guard<std::mutex> lock(_client_mutex);
        _client_instance.reset();
        _client_parameters->_client_process_name = "";
        _client_parameters->_client_pid = -1;
        ack_heartbeat();

        dump_message("client_exit-\n");
    }

    void WatchDog_Server::spin()
    {
        std::condition_variable cv;
        std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, []() { return false; });
    }

    void WatchDog_Server::dump_message(std::string message)
    {
        EYS3D_INFO("[Server] dump_message : %s\n", message.c_str());
        std::string time = get_dump_time_format(*get_current_time());
        fprintf(_file, "%s : %s\n", time.c_str(), message.c_str());
        fflush(_file);
    }

}

