//
// Created by lorian on 10/02/2026.
//

#include "logger.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace cominou::log
{
void initLogger()
{
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    sinks.back()->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs.txt", true));

    auto logger = std::make_shared<spdlog::logger>("COMINOU", sinks.begin(), sinks.end());
    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);
    spdlog::default_logger()->set_level(spdlog::level::debug);
}

void fatalError(const std::string& message)
{
    spdlog::error(message);
    throw std::runtime_error(message);
}
}