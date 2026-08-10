#include "engine/render/resolution_manager.h"

#include <stdexcept>
#include <string>

using namespace mattmath;

namespace artattack
{
    ResolutionManager::ResolutionManager()
    {
        // Through the setter, so the size cannot disagree with the preset it is
        // supposed to be the size of.
        this->set_resolution(this->resolution_);
    }

    Vector2I ResolutionManager::resolution_ivec() const
    {
    	return this->size_;
    }

    Vector2F ResolutionManager::resolution_vec() const
    {
        return Vector2F(this->size_);
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
        // Both members, from the one argument. Every other overload lands here,
        // so there is one place that decides what a preset means.
    	this->resolution_ = resolution;
        this->size_ = this->convert_resolution_to_ivec(resolution);
    }

    void ResolutionManager::set_resolution(const std::string& resolution)
    {
    	this->set_resolution(this->convert_string_to_resolution(resolution));
    }

    void ResolutionManager::set_resolution(const Vector2F& resolution)
    {
        this->set_resolution(this->convert_vec_to_resolution(resolution));
    }

    void ResolutionManager::set_resolution(const Vector2I& resolution)
    {
        // Still coercing, deliberately unchanged: this overload has always meant
        // "pick the preset nearest this", it answers 1280x720 for anything it
        // does not recognise, and a client may be relying on exactly that. The
        // non-coercing path is set_resolution_exactly, and the reason it is a
        // separate function rather than a change of heart here is that these two
        // are different questions with the same arguments.
        this->set_resolution(this->convert_ivec_to_resolution(resolution));
    }

    void ResolutionManager::set_resolution_exactly(const Vector2I& size)
    {
        if (size.x <= 0 || size.y <= 0)
        {
            throw std::invalid_argument("ResolutionManager::"
                "set_resolution_exactly - a resolution of " +
                std::to_string(size.x) + "x" + std::to_string(size.y) +
                " has no positive extent, and everything above this divides by "
                "it.");
        }

        this->size_ = size;

        // The label follows only if this size really is a preset, and the round
        // trip is what makes that test exact rather than approximate:
        // convert_ivec_to_resolution answers s_1280_720 for everything it does
        // not recognise, so converting the answer back and comparing is the
        // difference between "this size IS 720p" and "this size has just been
        // replaced by 720p". A size with no name keeps the last named one.
        const ScreenResolution candidate =
            this->convert_ivec_to_resolution(size);
        if (this->convert_resolution_to_ivec(candidate) == size)
        {
            this->resolution_ = candidate;
        }
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