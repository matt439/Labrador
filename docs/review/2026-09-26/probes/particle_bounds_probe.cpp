#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/resource_factory.h"
#include "engine/scene/scene.h"
#include "samples/linesweeper/presentation/particles.h"
#include <algorithm>
#include <cstdio>
#include <memory>

int main()
{
    using namespace labrador;
    using namespace linesweeper;
    using namespace mattmath;
    RenderResources resources;
    Renderer renderer;
    renderer.create_device(nullptr, 1280, 720, 1);
    renderer.set_resources(&resources);
    load_texture_asset(renderer, resources,
        "samples/linesweeper/content/textures/", "white");
    World world;
    TickResult tick_result;
    world.cells[static_cast<std::size_t>(cell_index(0, well_rows - 1))] = 1;
    Scene scene(nullptr, nullptr);
    ParticleField* field = scene.add(std::make_unique<ParticleField>(
        &world, &tick_result, resources.resolve_texture("white")));
    scene.end_tick();
    world.topped_out = 1;
    ++world.tick;
    field->update(0.0f);
    const RectangleF bounds = field->bounds();
    renderer.begin_frame();
    renderer.set_view_count(1);
    DrawList direct = renderer.view(0);
    field->draw(direct);
    renderer.submit();
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
        }
    }
    const std::size_t direct_count = recorded_sprites(renderer).size();
    std::printf("direct_sprites=%zu bounds=(%.3f,%.3f)-(%.3f,%.3f) drawn=(%.3f,%.3f)-(%.3f,%.3f)\n",
        direct_count, bounds.left(), bounds.top(), bounds.right(), bounds.bottom(),
        left, top, right, bottom);
    const Viewport viewport(0.0f, 0.0f, 1.0f, 720.0f);
    const Camera camera(left, 0.0f, 1.0f);
    scene.add_view(viewport, camera);
    renderer.begin_frame();
    scene.draw(renderer);
    renderer.submit();
    std::printf("edge_view=(%.3f,%.3f) bounds_intersects=%d scene_sprites=%zu\n",
        left, left + 1.0f, bounds.intersects(camera.visible_rectangle(viewport)),
        recorded_sprites(renderer).size());
    return 0;
}
