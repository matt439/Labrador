#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <stdexcept>
#include <vector>

int main()
{
    HWND window = CreateWindowExW(0, L"STATIC", L"Labrador stream probe",
        WS_OVERLAPPEDWINDOW, 0, 0, 128, 128, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (window == nullptr) return 2;
    int result = 0;
    try
    {
        labrador::RenderResources resources;
        labrador::Renderer renderer;
        renderer.create_device(window, 64, 64, 2);
        renderer.set_resources(&resources);
        std::printf("device=%s backend=%s\n", renderer.device_info().device_name.c_str(),
            renderer.device_info().backend.c_str());
        labrador::TextureData texture;
        texture.width = 1;
        texture.height = 1;
        texture.levels.push_back(labrador::texture_level(texture.format, 1, 1, 0));
        texture.pixels.assign(4, 255);
        labrador::add_texture_asset(renderer, resources, "white", texture);
        const labrador::TextureHandle white = resources.resolve_texture("white");
        for (int frame = 0; frame < 240; ++frame)
        {
            // Distinct runs grow the descriptor pools; the frame has no readback wait.
            renderer.begin_frame();
            renderer.set_view_count(2);
            for (int view = 0; view < 2; ++view)
            {
                labrador::DrawList list = renderer.view(view);
                list.set_viewport(labrador::Viewport(static_cast<float>(view * 32), 0, 32, 64));
                for (int sprite = 0; sprite < 100; ++sprite)
                {
                    list.set_filter(sprite % 2 == 0 ? labrador::TextureFilter::point : labrador::TextureFilter::linear);
                    list.draw_sprite(white, mattmath::RectangleI(0, 0, 1, 1),
                        mattmath::RectangleF(static_cast<float>(sprite % 16),
                            static_cast<float>(frame % 32), 1, 1), labrador::Colour::white,
                        0, mattmath::Vector2F::ZERO, labrador::SpriteFlip::none, 0);
                }
            }
            renderer.submit();
            renderer.end_frame();
        }
        std::printf("completed_frames=240 readbacks=0 runs_per_frame=200 views=2\n");
    }
    catch (const std::exception& error)
    {
        std::printf("probe_error=%s\n", error.what());
        result = 1;
    }
    DestroyWindow(window);
    return result;
}
