#include "engine/render/viewport_manager.h"

using namespace mattmath;

namespace artattack
{
    ViewportManager::ViewportManager(ResolutionManager* resolution_manager) :
        resolution_manager_(resolution_manager)
    {
    }

    Viewport ViewportManager::fullscreen_viewport() const
    {
        Vector2F res = this->resolution_manager_->resolution_vec();
        return { 0.0f, 0.0f, res.x, res.y };
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

    void ViewportManager::set_layout(ScreenLayout layout)
    {
        this->layout_ = layout;
    }

    Viewport ViewportManager::player_viewport(int player_num) const
    {
        return this->calculate_viewport(
            this->layout_, player_num, this->resolution_manager_->resolution_vec());
    }

    // Every viewport the layout covers the screen with, which is not the same
    // as one per player: a three-player split leaves a quadrant nobody is in,
    // and a caller drawing the whole screen still has to draw it. Callers
    // index set_viewport() by position in this vector, so the counts
    // and the indices have to be the same ones calculate_viewport() knows.
    std::vector<Viewport> ViewportManager::all_viewports() const
    {
        const int count = this->viewport_count_from_layout(this->layout_);
        std::vector<Viewport> result;
        result.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; i++)
        {
            result.push_back(this->player_viewport(i));
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

    int ViewportManager::viewport_count_from_layout(ScreenLayout layout) const
    {
        // Three players, four quadrants.
        if (layout == ScreenLayout::three_player)
        {
            return 4;
        }
        return this->player_count_from_layout(layout);
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

    // Pure arithmetic on a layout, an index and a size - no member is read, so
    // this is the piece of the class a test can drive without a device.
    //
    // Every layout returns from its own block. The inner switches used to fall
    // through: two_player's invalid-index `default: break` landed in
    // three_player's cases, three_player's landed in four_player's, and
    // four_player's landed in the fullscreen fallback. So an out-of-range
    // index did not fail, it silently answered from the *next* layout down.
    Viewport ViewportManager::calculate_viewport(ScreenLayout layout,
        int player_num, const Vector2F& screen_size) const
    {
        const float half_width = screen_size.x / 2.0f;
        const float half_height = screen_size.y / 2.0f;

        // Quadrants, in the order the four-player layout numbers them.
        const Viewport top_left = { 0.0f, 0.0f, half_width, half_height };
        const Viewport top_right = { half_width, 0.0f, half_width, half_height };
        const Viewport bottom_left = { 0.0f, half_height, half_width, half_height };
        const Viewport bottom_right =
            { half_width, half_height, half_width, half_height };
        const Viewport fullscreen = { 0.0f, 0.0f, screen_size.x, screen_size.y };

        switch (layout)
        {
        case ScreenLayout::one_player:
            return fullscreen;

        case ScreenLayout::two_player:
            switch (player_num)
            {
            case 0:
                return { 0.0f, 0.0f, screen_size.x, half_height };
            case 1:
                return { 0.0f, half_height, screen_size.x, half_height };
            default:
                return fullscreen;
            }

        case ScreenLayout::three_player:
            switch (player_num)
            {
            case 0:
                return top_left;
            case 1:
                return bottom_right;
            case 2:
                return bottom_left;
            // Three players occupy three quadrants; index 3 is the one nobody
            // is in. It is a real viewport rather than an error because
            // callers that cover the whole screen - the menus - still have to
            // draw it. all_viewports() used to reach for it by asking for
            // index *4*, which fell through to the fullscreen fallback, so
            // every menu drew a fourth full-screen pass over the other three.
            case 3:
                return top_right;
            default:
                return fullscreen;
            }

        case ScreenLayout::four_player:
            switch (player_num)
            {
            case 0:
                return top_left;
            case 1:
                return top_right;
            case 2:
                return bottom_left;
            case 3:
                return bottom_right;
            default:
                return fullscreen;
            }

        default:
            return fullscreen;
        }
    }
}