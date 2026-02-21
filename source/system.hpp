//
// Created by lorian on 15/02/2026.
//

#pragma once

#include <string>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

namespace sys
{
std::wstring toUtf16(const std::string& str);
static const fs::path ASSETS_DIR = fs::path(PROJECT_DIR) / "shaders";
static const fs::path CACHED_DIR = fs::path("cached");
}