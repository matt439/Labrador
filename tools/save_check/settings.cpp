// The settings-and-saves guide's JSON writer. It owns no engine resources.
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#include <windows.h>
#include <shlobj.h>
#endif

std::filesystem::path settings_directory()
{
#if defined(__EMSCRIPTEN__)
    return "/save";
#else
    wchar_t* raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &raw)))
        throw std::runtime_error("Cannot find this user's LocalAppData folder");

    struct FolderDeleter
    {
        void operator()(wchar_t* value) const { CoTaskMemFree(value); }
    };
    const std::unique_ptr<wchar_t, FolderDeleter> folder(raw);
    return std::filesystem::path(folder.get()) / L"MyGame";
#endif
}

void write_settings(const std::filesystem::path& directory, int volume)
{
    if (volume < 0 || volume > 100)
        throw std::invalid_argument("Volume must be from 0 to 100");

    std::filesystem::create_directories(directory);
    std::ofstream output(directory / "settings.json", std::ios::binary | std::ios::trunc);
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output << "{\"version\":1,\"volume\":" << volume << "}\n";
    output.close();
}

std::string read_settings_json(const std::filesystem::path& directory)
{
    const std::filesystem::path path = directory / "settings.json";
    if (!std::filesystem::exists(path))
        return "{}";

    std::ifstream input(path, std::ios::binary);
    input.exceptions(std::ios::failbit | std::ios::badbit);
    const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();
    return json;
}

#if defined(__EMSCRIPTEN__)
EM_JS(void, report_save_error, (const char* message), {
    console.error(UTF8ToString(message));
});

EM_JS(void, report_loaded_settings, (const char* json), {
    Module['loadedSettings'] = JSON.parse(UTF8ToString(json));
    Module['settingsReady'] = true;
});

extern "C" EMSCRIPTEN_KEEPALIVE int save_volume(int volume)
{
    try
    {
        write_settings(settings_directory(), volume);
        return 0;
    }
    catch (const std::exception& error)
    {
        report_save_error(error.what());
        return 1;
    }
}

int main()
{
    const std::string json = read_settings_json(settings_directory());
    report_loaded_settings(json.c_str());
}
#endif
