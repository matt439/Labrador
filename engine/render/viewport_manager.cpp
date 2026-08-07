#include "engine/render/viewport_manager.h"

using namespace DirectX;
using namespace mattmath;

namespace artattack
{
    ViewportManager::ViewportManager(ResolutionManager* resolution_manager,
        DeviceResources* device_resources) :
        resolution_manager_(resolution_manager),
        device_resources_(device_resources)
    {
    }

    Viewport ViewportManager::fullscreen_viewport() const
    {
        Vector2F res = this->resolution_manager_->resolution_vec();
        return { 0.0f, 0.0f, res.x, res.y };
    }

    D3D11_VIEWPORT ViewportManager::fullscreen_d3d11_viewport() const
    {
        Viewport vp = this->fullscreen_viewport();
        return vp.d3d_viewport();
    }

    RectangleF ViewportManager::camera_adjusted_player_viewport_rect(
        int player_num, const Camera& camera) const
    {
        Viewport vp = this->player_viewport(player_num);
        const float scale = camera.scale;
        const Vector2F& translation = camera.translation;

        auto result = RectangleF(
            translation.x,
            translation.y,
            vp.width * scale,
            vp.height * scale);

        return result;
    }

    void ViewportManager::apply_player_viewport(int player_num,
        ID3D11DeviceContext* context) const
    {
        D3D11_VIEWPORT vp = this->calculate_d3d11_viewport(
            this->layout_, player_num, this->resolution_manager_->resolution_vec());
        context->RSSetViewports(1, &vp);
    }

    void ViewportManager::apply_player_viewport(int player_num,
        ID3D11DeviceContext* context,
        SpriteBatch* sprite_batch) const
    {
        D3D11_VIEWPORT vp = this->calculate_d3d11_viewport(
            this->layout_, player_num, this->resolution_manager_->resolution_vec());
        context->RSSetViewports(1, &vp);
        sprite_batch->SetViewport(vp);
    }

    D3D11_VIEWPORT ViewportManager::calculate_d3d11_viewport(ScreenLayout layout,
        int player_num, const Vector2F& screen_size) const
    {
        Viewport vp = this->calculate_viewport(layout, player_num, screen_size);
        return vp.d3d_viewport();
    }

    void ViewportManager::set_layout(ScreenLayout layout)
    {
        this->layout_ = layout;
    }

    Viewport ViewportManager::player_viewport(int player_num) const
    {
        return this->calculate_viewport(
            this->layout_, player_num, this->resolution_manager_->resolution_vec());
    }

    std::vector<Viewport> ViewportManager::all_viewports() const
    {
        std::vector<Viewport> result;
        int player_count = this->player_count_from_layout(this->layout_);
        for (int i = 0; i < player_count; i++)
        {
            result.push_back(this->player_viewport(i));
        }
        // Add a 4th viewport if there are 3 players
        if (player_count == 3)
        {
            result.push_back(this->player_viewport(4));
        }
        return result;
    }

    int ViewportManager::player_count_from_layout(ScreenLayout layout) const
    {
        switch (layout)
        {
        case ScreenLayout::one_player:
            return 1;
        case ScreenLayout::two_player:
            return 2;
        case ScreenLayout::three_player:
            return 3;
        case ScreenLayout::four_player:
            return 4;
        default:
            return 1;
        }
    }

    std::vector<mattmath::RectangleF> ViewportManager::viewport_dividers() const
    {
        std::vector<RectangleF> result = std::vector<RectangleF>();
        const Vector2F res = this->resolution_manager_->resolution_vec();
        ScreenLayout layout = this->layout_;

        if (layout == ScreenLayout::one_player)
        {
            return result;
        }
        else if (layout == ScreenLayout::two_player)
        {
            result.push_back(RectangleF(0.0f, res.y / 2.0f - DIVIDER_THICKNESS / 2.0f,
                res.x, DIVIDER_THICKNESS));
            return result;
        }
        else if (layout == ScreenLayout::three_player ||
            layout == ScreenLayout::four_player)
        {
            result.push_back(RectangleF(0.0f, res.y / 2.0f - DIVIDER_THICKNESS / 2.0f,
                res.x, DIVIDER_THICKNESS));
            result.push_back(RectangleF(res.x / 2.0f - DIVIDER_THICKNESS / 2.0f, 0.0f,
                DIVIDER_THICKNESS, res.y));
            return result;
        }
        else
        {
            throw std::exception("Invalid screen layout");
        }
    }

    Viewport ViewportManager::calculate_viewport(ScreenLayout layout,
        int player_num, const Vector2F& screen_size) const
    {
        Viewport result = Viewport();
        switch (layout)
        {
        case ScreenLayout::one_player:
            result.x = 0.0f;
            result.y = 0.0f;
            result.width = screen_size.x;
            result.height = screen_size.y;
            return result;
        case ScreenLayout::two_player:
            switch (player_num)
            {
            case 0:
                result.x = 0.0f;
                result.y = 0.0f;
                result.width = screen_size.x;
                result.height = screen_size.y / 2.0f;
                return result;
            case 1:
                result.x = 0.0f;
                result.y = screen_size.y / 2.0f;
                result.width = screen_size.x;
                result.height = screen_size.y / 2.0f;
                return result;
            default:
                break;
            }
        case ScreenLayout::three_player:
            switch (player_num)
            {
            case 0:
                result.x = 0.0f;
                result.y = 0.0f;
                result.width = screen_size.x / 2.0f;
                result.height = screen_size.y / 2.0f;
                return result;
            case 1:
                result.x = screen_size.x / 2.0f;
                result.y = screen_size.y / 2.0f;
                result.width = screen_size.x / 2.0f;
                result.height = screen_size.y / 2.0f;
                return result;
            case 2:
                result.x = 0.0f;
                result.y = screen_size.y / 2.0f;
                result.width = screen_size.x / 2.0f;
                result.height = screen_size.y / 2.0f;
                return result;
            default:
                break;
            }
        case ScreenLayout::four_player:
            switch (player_num)
            {
            case 0:
                result.x = 0.0f;
                result.y = 0.0f;
                result.width = screen_size.x / 2.0f;
                result.height = screen_size.y / 2.0f;
                return result;
            case 1:
                result.x = screen_size.x / 2.0f;
                result.y = 0.0f;
                result.width = screen_size.x / 2.0f;
                result.height = screen_size.y / 2.0f;
                return result;
            case 2:
                result.x = 0.0f;
                result.y = screen_size.y / 2.0f;
                result.width = screen_size.x / 2.0f;
                result.height = screen_size.y / 2.0f;
                return result;
            case 3:
                result.x = screen_size.x / 2.0f;
                result.y = screen_size.y / 2.0f;
                result.width = screen_size.x / 2.0f;
                result.height = screen_size.y / 2.0f;
                return result;
            default:
                break;
            }
        default: //1P screen
            result.x = 0.0f;
            result.y = 0.0f;
            result.width = screen_size.x;
            result.height = screen_size.y;
            return result;
        }
    }
}