#include "engine/render/drawer.h"

using namespace DirectX;
using namespace mattmath;

namespace artattack
{
    Drawer::Drawer(RenderResources* render_resources) :
        render_resources_(render_resources)
    {
    }

    void Drawer::set_render_resources(RenderResources* render_resources)
    {
        this->render_resources_ = render_resources;
    }

    RenderResources* Drawer::render_resources() const
    {
        return this->render_resources_;
    }

    RectangleI Drawer::calculate_draw_rectangle(const RectangleI& rec,
        const Vector3F& camera) const
    {
        return this->calculate_draw_rectangle(
            Vector2F(static_cast<float>(rec.x), static_cast<float>(rec.y)),
            Vector2F(static_cast<float>(rec.width), static_cast<float>(rec.height)),
            camera);
    }

    RectangleI Drawer::calculate_draw_rectangle(const Vector2F& position,
        const Vector2F& size, const Vector3F& camera) const
    {
        Vector2F draw_pos = (position - Vector2F(camera.x, camera.y)) * camera.z;
        Vector2F draw_size = Vector2F(size) * camera.z;
        return {
            static_cast<int>(draw_pos.x),
            static_cast<int>(draw_pos.y),
            static_cast<int>(draw_size.x),
            static_cast<int>(draw_size.y)
        };
    }

    Vector2F Drawer::calculate_sprite_origin(
        const Vector2F& size, RotationOrigin origin) const
    {
        switch (origin)
        {
        case RotationOrigin::center:
            return Vector2F(size) / 2.0f;
        case RotationOrigin::left_center:
            return { 0.0f, size.y / 2.0f };
        default: // RotationOrigin::top_left
            return Vector2F::ZERO;
        }
    }
}