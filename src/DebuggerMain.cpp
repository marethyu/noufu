#include <iostream>

#include "Emulator.h"
#include "Version.h"

#define FMT_HEADER_ONLY
#include "fmt/format.h"

static void PrintPrologue()
{
    std::cout << fmt::format("Noufu v{} (Debug mode)", CURRENT_VERSION) << std::endl;
    std::cout << fmt::format("Type \"help\" for more information.") << std::endl;
}

int main(int argc, char *argv[])
{
    PrintPrologue();
    return 0;
}