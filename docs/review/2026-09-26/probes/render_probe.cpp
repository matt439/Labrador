#include "engine/core/name_table.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/render/animation_strip.h"
#include "engine/render/camera.h"
#include "engine/render/dds_file.h"
#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/resource_factory.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/sprite_sheet.h"
#include "engine/render/texture_data.h"
#include "engine/render/visual.h"

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <memory>
#include <utility>
#include <vector>

static void poke(std::vector<unsigned char>& bytes, size_t offset, uint32_t value)
{
    for (size_t index = 0; index < 4; ++index)
        bytes[offset + index] = static_cast<unsigned char>((value >> (8 * index)) & 255u);
}

static void rectangle(const char* name, const mattmath::RectangleF& value)
{
    std::printf("%s=(%.3f, %.3f, %.3f, %.3f)\n", name,
        static_cast<double>(value.x), static_cast<double>(value.y),
        static_cast<double>(value.width), static_cast<double>(value.height));
}

int main()
{
    labrador::RenderResources resources;
    labrador::Renderer renderer;
    renderer.create_device(nullptr, 100, 100, 1);
    renderer.set_resources(&resources);
    labrador::TextureData texture;
    texture.width = 2;
    texture.height = 2;
    texture.levels.push_back(labrador::texture_level(texture.format, 2, 2, 0));
    texture.pixels.assign(16, 255);
    labrador::add_texture_asset(renderer, resources, "texture", texture);
    labrador::NameTable<labrador::SpriteFrame> frames("frame");
    frames.add("plain", labrador::SpriteFrame(mattmath::RectangleI(0, 0, 2, 2)));
    resources.add_sprite_sheet("sheet", std::make_unique<labrador::SpriteSheet>(
        resources.resolve_texture("texture"), std::move(frames),
        labrador::NameTable<labrador::AnimationStrip>("strip")));

    const mattmath::RectangleRotated turned(mattmath::Vector2F(50, 50),
        mattmath::Vector2F(0, 1), mattmath::Vector2F(-1, 0), mattmath::Vector2F(20, 10));
    const labrador::Visual visual("sheet", "plain", turned, &resources);
    rectangle("input_rotated_bounds", turned.bounding_box());
    rectangle("visual_bounds", visual.bounds());
    renderer.begin_frame();
    renderer.set_view_count(1);
    labrador::DrawList list = renderer.view(0);
    visual.draw(list);
    renderer.submit();
    for (const labrador::RecordedSprite& sprite : labrador::recorded_sprites(renderer))
        for (const labrador::SpriteVertex& corner : sprite.corners)
            std::printf("visual_corner=(%.3f, %.3f)\n",
                static_cast<double>(corner.position.x), static_cast<double>(corner.position.y));
    renderer.end_frame();

    const labrador::Visual fractional("sheet", "plain",
        mattmath::RectangleF(0.1f, 0.1f, 0.8f, 0.8f), &resources);
    const labrador::Camera camera(mattmath::Vector2F(0.2f, 0.2f), 10.0f);
    const mattmath::RectangleF visible = camera.visible_rectangle(labrador::Viewport(0, 0, 100, 100));
    rectangle("fractional_visual_bounds", fractional.bounds());
    rectangle("camera_visible_world", visible);
    std::printf("fractional_visual_passes_cull=%d\n", fractional.bounds().intersects(visible) ? 1 : 0);
    renderer.begin_frame();
    renderer.set_view_count(1);
    list = renderer.view(0);
    list.set_camera(camera);
    fractional.draw(list);
    renderer.submit();
    for (const labrador::RecordedSprite& sprite : labrador::recorded_sprites(renderer))
        for (const labrador::SpriteVertex& corner : sprite.corners)
            std::printf("fractional_actual_corner=(%.3f, %.3f)\n",
                static_cast<double>(corner.position.x), static_cast<double>(corner.position.y));
    renderer.end_frame();

    std::vector<unsigned char> bytes(132, 0);
    poke(bytes, 0, 0x20534444u);
    poke(bytes, 4, 124);
    poke(bytes, 8, 0x100Fu);
    poke(bytes, 12, 1);
    poke(bytes, 16, 0x40000001u);
    poke(bytes, 20, 4);
    poke(bytes, 76, 32);
    poke(bytes, 80, 0x41u);
    poke(bytes, 88, 32);
    poke(bytes, 92, 0x000000FFu);
    poke(bytes, 96, 0x0000FF00u);
    poke(bytes, 100, 0x00FF0000u);
    poke(bytes, 104, 0xFF000000u);
    poke(bytes, 108, 0x1000u);
    const char* path = "out/review-2026-09-26/overflow.dds";
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    try
    {
        const labrador::TextureData decoded = labrador::read_dds_file(path);
        std::printf("dds_accepted width=%d height=%d stride=%d size=%zu bytes=%zu\n",
            decoded.width, decoded.height, decoded.levels[0].stride,
            decoded.levels[0].size, decoded.pixels.size());
    }
    catch (const std::exception& error)
    {
        std::printf("dds_rejected=%s\n", error.what());
    }
}
