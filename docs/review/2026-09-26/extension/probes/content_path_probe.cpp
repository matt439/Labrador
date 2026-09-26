#include "engine/app/content_root.h"
#include "engine/assets/json.h"

#include <Windows.h>
#include <cstdio>
#include <exception>
#include <string>

int main()
{
    std::printf("Windows ANSI code page=%u\n", GetACP());
    try
    {
        const std::string root = labrador::executable_directory();
        std::printf("executable_directory question-mark=%d\n",
            root.find('?') != std::string::npos);
        const labrador::JsonDocument manifest = labrador::read_json_file(
            labrador::resolved_under(root, "manifest.json").c_str());
        std::printf("manifest loaded=%s\n",
            manifest.root().string("probe").c_str());
    }
    catch (const std::exception& error)
    {
        std::printf("content discovery threw: %s\n", error.what());
        return 1;
    }
}
