#include "engine/render/resolution_manager.h"

#include <string>

using namespace mattmath;

namespace artattack
{
    Vector2I ResolutionManager::resolution_ivec() const
    {
    	return this->convert_resolution_to_ivec(this->resolution_);
    }

    Vector2F ResolutionManager::resolution_vec() const
    {
        Vector2F result = this->convert_resolution_to_vec(this->resolution_);
        return result;
    }

    std::string ResolutionManager::resolution_string() const
    {
    	return this->convert_resolution_to_string(this->resolution_);
    }

    ScreenResolution ResolutionManager::resolution() const
    {
    	return this->resolution_;
    }

    void ResolutionManager::set_resolution(ScreenResolution resolution)
    {
    	this->resolution_ = resolution;
    }

    void ResolutionManager::set_resolution(const std::string& resolution)
    {
    	this->resolution_ = this->convert_string_to_resolution(resolution);
    }

    void ResolutionManager::set_resolution(const Vector2F& resolution)
    {
        this->resolution_ = this->convert_vec_to_resolution(resolution);
    }

    void ResolutionManager::set_resolution(const Vector2I& resolution)
    {
        this->resolution_ = this->convert_ivec_to_resolution(resolution);
    }

    Vector2I ResolutionManager::convert_resolution_to_ivec(
        ScreenResolution resolution) const
    {
        switch (resolution)
        {
        case ScreenResolution::s_1280_720:
            return {1280, 720};
        case ScreenResolution::s_1920_1080:
            return {1920, 1080};
        case ScreenResolution::s_2560_1440:
            return {2560, 1440};
        case ScreenResolution::s_3840_2160:
            return {3840, 2160};
        default:
            return {-1, -1};
        }
    }

    Vector2F ResolutionManager::convert_resolution_to_vec(
        ScreenResolution resolution) const
    {
        Vector2I res =
            this->convert_resolution_to_ivec(resolution);
        return {
    	    static_cast<float>(res.x),
            static_cast<float>(res.y)
        };
    }

    std::string ResolutionManager::convert_resolution_to_string(
        ScreenResolution resolution)
    {
        switch (resolution)
        {
        case ScreenResolution::s_1280_720:
            return std::string("1280x720");
        case ScreenResolution::s_1920_1080:
            return std::string("1920x1080");
        case ScreenResolution::s_2560_1440:
            return std::string("2560x1440");
        case ScreenResolution::s_3840_2160:
            return std::string("3840x2160");
        default:
            return std::string("-1x-1");
        }
    }

    ScreenResolution ResolutionManager::convert_string_to_resolution(
        const std::string& string) const
    {
        if (string == this->convert_resolution_to_string(
            ScreenResolution::s_1280_720))
        {
            return ScreenResolution::s_1280_720;
        }
        else if (string == this->convert_resolution_to_string(
            ScreenResolution::s_1920_1080))
        {
            return ScreenResolution::s_1920_1080;
        }
        else if (string == this->convert_resolution_to_string(
            ScreenResolution::s_2560_1440))
        {
            return ScreenResolution::s_2560_1440;
        }
        else if (string == this->convert_resolution_to_string(
            ScreenResolution::s_3840_2160))
        {
            return ScreenResolution::s_3840_2160;
        }
        else
        {
            return ScreenResolution::s_1280_720;
        }
    }

    ScreenResolution ResolutionManager::convert_vec_to_resolution(
        const Vector2F& vec) const
    {
        if (vec == this->convert_resolution_to_vec(
            ScreenResolution::s_1280_720))
        {
    		return ScreenResolution::s_1280_720;
    	}
        else if (vec == this->convert_resolution_to_vec(
            ScreenResolution::s_1920_1080))
        {
    		return ScreenResolution::s_1920_1080;
    	}
        else if (vec == this->convert_resolution_to_vec(
            ScreenResolution::s_2560_1440))
        {
    		return ScreenResolution::s_2560_1440;
    	}
        else if (vec == this->convert_resolution_to_vec(
            ScreenResolution::s_3840_2160))
        {
    		return ScreenResolution::s_3840_2160;
    	}
        else
        {
    		return ScreenResolution::s_1280_720;
    	}
    }

    ScreenResolution ResolutionManager::convert_ivec_to_resolution(
        const Vector2I& vec) const
    {
        if (vec.x == 1280 && vec.y == 720)
        {
            return ScreenResolution::s_1280_720;
        }
        else if (vec.x == 1920 && vec.y == 1080)
        {
    		return ScreenResolution::s_1920_1080;
    	}
        else if (vec.x == 2560 && vec.y == 1440)
        {
    		return ScreenResolution::s_2560_1440;
    	}
        else if (vec.x == 3840 && vec.y == 2160)
        {
    		return ScreenResolution::s_3840_2160;
    	}
        else
        {
    		return ScreenResolution::s_1280_720;
    	}
    }
}