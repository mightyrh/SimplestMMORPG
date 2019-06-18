#pragma once

// defines
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _CRT_SECURE_NO_WARNINGS



#include <iostream>
#include <vector>
#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <cassert>
#include <thread>
#include <mutex>
#include <random>
#include <unordered_set>
#include <cstdlib>

using namespace std::chrono_literals;

// Network
#include <WS2tcpip.h>

// spdlog
#include "spdlog/spdlog.h"

// TBB
#include "tbb/include/tbb/concurrent_hash_map.h"
#include "tbb/include/tbb/concurrent_priority_queue.h"
#include "tbb/include/tbb/concurrent_queue.h"
#include "tbb/include/tbb/concurrent_priority_queue.h"

