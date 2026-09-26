#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/resource_factory.h"
#include "engine/render/sprite_sheet.h"
#include "engine/scene/scene.h"
#include "engine/ui/widget.h"
#include <algorithm>
#include <cstdio>
#include <memory>
#include <utility>

int main()
{
    using namespace labrador;
    using namespace mattmath;
    RenderResources resources;
    Renderer renderer;
    renderer.create_device(nullptr, 64, 64, 1);
    renderer.set_resources(&resources);
    load_texture_asset(renderer, resources,
        "samples/linesweeper/content/textures/", "white");
    NameTable<SpriteFrame> frames("sprite frame");
    frames.add("full", SpriteFrame(RectangleI(0, 0, 1, 1)));
    NameTable<AnimationStrip> strips("animation strip");
    resources.add_sprite_sheet("white", std::make_unique<SpriteSheet>(
        resources.resolve_texture("white"), std::move(frames), std::move(strips)));
    Scene scene(nullptr, nullptr);
    UiTexture* widget = scene.add(std::make_unique<UiTexture>(
        "rotated", "white", "full", RectangleF(10.0f, 10.0f, 20.0f, 10.0f),
        &resources, Colour::white, false, 1.57079632679f));
    scene.end_tick();
    renderer.begin_frame();
    renderer.set_view_count(1);
    DrawList direct = renderer.view(0);
    widget->draw(direct);
    renderer.submit();
    const RectangleF bounds = widget->bounds();
    float left = bounds.left();
    float top = bounds.top();
    float right = bounds.right();
    float bottom = bounds.bottom();
    for (const RecordedSprite& sprite : recorded_sprites(renderer))
    {
        for (const SpriteVertex& corner : sprite.corners)
        {
            left = std::min(left, corner.position.x);
            top = std::min(top, corner.position.y);
            right = std::max(right, corner.position.x);
            bottom = std::max(bottom, corner.position.y);
            std::printf("corner=(%.3f,%.3f)\n", corner.position.x, corner.position.y);
        }
    }
    std::printf("bounds=(%.3f,%.3f)-(%.3f,%.3f) union=(%.3f,%.3f)-(%.3f,%.3f)\n",
        bounds.left(), bounds.top(), bounds.right(), bounds.bottom(), left, top, right, bottom);
    const Viewport viewport(0.0f, 0.0f, 9.0f, 8.0f);
    const Camera camera(0.0f, 12.0f, 1.0f);
    scene.add_view(viewport, camera);
    renderer.begin_frame();
    scene.draw(renderer);
    renderer.submit();
    std::printf("view=(0,12)-(9,20) bounds_intersects=%d scene_sprites=%zu\n",
        bounds.intersects(camera.visible_rectangle(viewport)), recorded_sprites(renderer).size());
    return 0;
}
