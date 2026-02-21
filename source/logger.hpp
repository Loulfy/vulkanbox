//
// Created by lorian on 10/02/2026.
//

#pragma once

#include <spdlog/spdlog.h>
namespace cominou::log
{
using namespace spdlog;

void initLogger();
void fatalError(const std::string& message);
}