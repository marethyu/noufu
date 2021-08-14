#ifndef _UTIL_H_
#define _UTIL_H_

#include <string>

#include <Windows.h>

int int_from_string(const std::string& s, bool isHex);

std::string int_to_bin8(uint8_t i, bool prefixed=true);

std::string int_to_hex(int i, int w, bool prefixed=true);

int CDECL PopMessageBox(TCHAR *szTitle, UINT uType, const char* format, ...);

#endif