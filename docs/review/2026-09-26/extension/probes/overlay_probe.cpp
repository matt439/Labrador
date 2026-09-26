#include "engine/core/state.h"
#include "engine/core/state_context.h"
#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/scene/scene.h"

#include <cstdio>
#include <memory>
#include <stdexcept>

namespace
{
    class SceneState final : public labrador::State
    {
    public:
        SceneState(labrador::TextureHandle texture, int views, bool covering)
            : texture_(texture), covering_(covering), scene_(nullptr, nullptr)
        {
            for (int index = 0; index < views; ++index)
                this->scene_.add_view(labrador::Viewport(
                    static_cast<float>(index * 64 / views), 0,
                    static_cast<float>(64 / views), 64));
        }
        void init() override {}
        void update(float) override {}
        bool covers_screen() const override { return this->covering_; }
        void draw(labrador::Renderer& renderer) const override
        {
            this->scene_.draw(renderer,
                [this](int, labrador::DrawList& list)
                {
                    list.draw_sprite(this->texture_, mattmath::RectangleI(0, 0, 1, 1),
                        mattmath::RectangleF(0, 0, 4, 4), labrador::Colour::white,
                        0, mattmath::Vector2F::ZERO, labrador::SpriteFlip::none, 0);
                });
        }
    private:
        labrador::TextureHandle texture_;
        bool covering_;
        labrador::Scene scene_;
    };
}

int main()
{
    labrador::RenderResources resources;
    labrador::Renderer renderer;
    renderer.create_device(nullptr, 64, 64, 4);
    renderer.set_resources(&resources);
    labrador::TextureData texture;
    texture.width = 1;
    texture.height = 1;
    texture.levels.push_back(labrador::texture_level(texture.format, 1, 1, 0));
    texture.pixels.assign(4, 255);
    labrador::add_texture_asset(renderer, resources, "white", texture);
    const labrador::TextureHandle white = resources.resolve_texture("white");
    for (int underlying_views : {1, 2})
    {
        labrador::StateContext context;
        context.push(std::make_unique<SceneState>(white, underlying_views, true));
        context.push(std::make_unique<SceneState>(white, 1, false));
        renderer.begin_frame();
        try
        {
            context.draw(renderer);
            renderer.submit();
            std::printf("underlying_views=%d overlay_views=1 covers_screen=false recorded=%zu result=drawn\n",
                underlying_views, labrador::recorded_sprites(renderer).size());
        }
        catch (const std::logic_error& error)
        {
            std::printf("underlying_views=%d overlay_views=1 covers_screen=false result=throws message=%s\n",
                underlying_views, error.what());
        }
        renderer.end_frame();
    }
}
