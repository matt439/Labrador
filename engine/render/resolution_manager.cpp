#include "engine/render/resolution_manager.h"

using namespace MattMath;

Vector2I ResolutionManager::get_resolution_ivec() const
{
	return this->convert_resolution_to_ivec(this->resolution_);
}

Vector2F ResolutionManager::get_resolution_vec() const
{
    Vector2F result = this->convert_resolution_to_vec(this->resolution_);
    return result;
}

std::string ResolutionManager::get_resolution_string() const
{
	return this->convert_resolution_to_string(this->resolution_);
}

screen_resolution ResolutionManager::get_resolution() const
{
	return this->resolution_;
}

void ResolutionManager::set_resolution(screen_resolution resolution)
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
    screen_resolution resolution) const
{
    switch (resolution)
    {
    case screen_resolution::S_1280_720:
        return {1280, 720};
    case screen_resolution::S_1920_1080:
        return {1920, 1080};
    case screen_resolution::S_2560_1440:
        return {2560, 1440};
    case screen_resolution::S_3840_2160:
        return {3840, 2160};
    default:
        return {-1, -1};
    }
}

Vector2F ResolutionManager::convert_resolution_to_vec(
    screen_resolution resolution) const
{
    Vector2I res =
        this->convert_resolution_to_ivec(resolution);
    return {
	    static_cast<float>(res.x),
        static_cast<float>(res.y)
    };
}

std::string ResolutionManager::convert_resolution_to_string(
    screen_resolution resolution)
{
    switch (resolution)
    {
    case screen_resolution::S_1280_720:
        return std::string("1280x720");
    case screen_resolution::S_1920_1080:
        return std::string("1920x1080");
    case screen_resolution::S_2560_1440:
        return std::string("2560x1440");
    case screen_resolution::S_3840_2160:
        return std::string("3840x2160");
    default:
        return std::string("-1x-1");
    }
}

screen_resolution ResolutionManager::convert_string_to_resolution(
    const std::string& string) const
{
    if (string == this->convert_resolution_to_string(
        screen_resolution::S_1280_720))
    {
        return screen_resolution::S_1280_720;
    }
    else if (string == this->convert_resolution_to_string(
        screen_resolution::S_1920_1080))
    {
        return screen_resolution::S_1920_1080;
    }
    else if (string == this->convert_resolution_to_string(
        screen_resolution::S_2560_1440))
    {
        return screen_resolution::S_2560_1440;
    }
    else if (string == this->convert_resolution_to_string(
        screen_resolution::S_3840_2160))
    {
        return screen_resolution::S_3840_2160;
    }
    else
    {
        return screen_resolution::S_1280_720;
    }
}

screen_resolution ResolutionManager::convert_vec_to_resolution(
    const Vector2F& vec) const
{
    if (vec == this->convert_resolution_to_vec(
        screen_resolution::S_1280_720))
    {
		return screen_resolution::S_1280_720;
	}
    else if (vec == this->convert_resolution_to_vec(
        screen_resolution::S_1920_1080))
    {
		return screen_resolution::S_1920_1080;
	}
    else if (vec == this->convert_resolution_to_vec(
        screen_resolution::S_2560_1440))
    {
		return screen_resolution::S_2560_1440;
	}
    else if (vec == this->convert_resolution_to_vec(
        screen_resolution::S_3840_2160))
    {
		return screen_resolution::S_3840_2160;
	}
    else
    {
		return screen_resolution::S_1280_720;
	}
}

screen_resolution ResolutionManager::convert_ivec_to_resolution(
    const Vector2I& vec) const
{
    if (vec.x == 1280 && vec.y == 720)
    {
        return screen_resolution::S_1280_720;
    }
    else if (vec.x == 1920 && vec.y == 1080)
    {
		return screen_resolution::S_1920_1080;
	}
    else if (vec.x == 2560 && vec.y == 1440)
    {
		return screen_resolution::S_2560_1440;
	}
    else if (vec.x == 3840 && vec.y == 2160)
    {
		return screen_resolution::S_3840_2160;
	}
    else
    {
		return screen_resolution::S_1280_720;
	}
}