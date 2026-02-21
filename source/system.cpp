//
// Created by lorian on 15/02/2026.
//

#include "system.hpp"

#ifdef _WIN32
#include <ShlObj.h>
#include <Wbemidl.h>
#include <comdef.h>
#include <windows.h>
#else
#endif

namespace sys
{
std::wstring toUtf16(const std::string& str)
{
    std::wstring wide(str.size(), '\0');
#ifdef _WIN32
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wide.data(), static_cast<int>(wide.size()));
#else
    mbstowcs(wide.data(), str.c_str(), str.size());
#endif
    return wide;
}
}