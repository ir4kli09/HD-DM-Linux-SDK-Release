#pragma once

#include <ctime>
#include <string>
#include "rpc/client.h"
#include <memory>
#include "common_def.h"

namespace eYs3D{

std::tm *get_current_time();
std::string get_dump_time_format(std::tm time);
std::shared_ptr<rpc::client> watchdog_client_facotry(int port, int timeout_ms = 500);
}

