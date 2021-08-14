#include <bitset>
#include <iomanip>
#include <sstream>

#include <cstdio>

#include "Util.h"

int int_from_string(const std::string& s, bool isHex)
{
    int x;
    std::stringstream ss;
    if (isHex) ss << std::hex << s; else ss << s;
    ss >> x;
    return x;
}

std::string int_to_bin8(uint8_t i, bool prefixed)
{
    std::stringstream stream;
    stream << (prefixed ? "0b" : "") << std::bitset<8>(i);
    return stream.str();
}

std::string int_to_hex(int i, int w, bool prefixed)
{
    std::stringstream stream;
    stream << (prefixed ? "0x" : "")
           << std::setfill('0') << std::setw(w)
           << std::uppercase << std::hex << i;
    return stream.str();
}

int CDECL PopMessageBox(TCHAR *szTitle, UINT uType, const char* format, ...)
{
	char buffer[100];
	va_list vl;

	va_start(vl, format);
    vsprintf(buffer, format, vl);
	va_end(vl);

	return MessageBox(nullptr, buffer, szTitle, uType);
}