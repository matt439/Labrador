#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>

#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

int main()
{
    HWND window = CreateWindowExW(0, L"STATIC", L"Labrador GL refusal probe",
        WS_OVERLAPPEDWINDOW, 0, 0, 128, 128, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (window == nullptr) return 2;
    int result = 0;
    try
    {
        labrador::RenderResources resources;
        labrador::Renderer renderer;
        renderer.create_device(window, 64, 64, 1);
        std::printf("device=%s\n", renderer.device_info().device_name.c_str());
        labrador::TextureData texture;
        texture.width = 1;
        texture.height = 1;
        texture.format = labrador::TextureFormat::b4g4r4a4_unorm;
        texture.levels.push_back(labrador::texture_level(texture.format, 1, 1, 0));
        texture.pixels.assign(2, 255);
        std::vector<GLuint> leaked;
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            glBindTexture(GL_TEXTURE_2D, 0);
            try
            {
                labrador::add_texture_asset(renderer, resources, "unsupported", texture);
                result = 3;
            }
            catch (const std::runtime_error& error)
            {
                std::printf("attempt=%d refusal=%s\n", attempt, error.what());
            }
            GLint bound = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
            const GLuint name = static_cast<GLuint>(bound);
            const bool alive = name != 0 && glIsTexture(name) == GL_TRUE;
            std::printf("attempt=%d bound_texture=%u alive=%d\n", attempt, name, alive ? 1 : 0);
            if (alive) leaked.push_back(name);
        }
        resources.release_device_resources();
        for (GLuint name : leaked)
            std::printf("after_release_texture=%u alive=%d\n", name,
                glIsTexture(name) == GL_TRUE ? 1 : 0);
        // The probe cleans up the leak it just observed while the context exists.
        for (GLuint name : leaked) glDeleteTextures(1, &name);
    }
    catch (const std::exception& error)
    {
        std::printf("probe_error=%s\n", error.what());
        result = 1;
    }
    DestroyWindow(window);
    return result;
}
