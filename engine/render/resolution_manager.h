#pragma once

#include <string>
#include "engine/math/matt_math.h"
#include "engine/render/screen_resolution.h"

class ResolutionManager
{
public:
    ResolutionManager() = default;
    mattmath::Vector2I resolution_ivec() const;
    mattmath::Vector2F resolution_vec() const;
    std::string resolution_string() const;
    ScreenResolution resolution() const;
    void set_resolution(ScreenResolution resolution);
    void set_resolution(const std::string& resolution);
    void set_resolution(const mattmath::Vector2F& resolution);
    void set_resolution(const mattmath::Vector2I& resolution);

    static std::string convert_resolution_to_string(
        ScreenResolution resolution);

private:
    ScreenResolution resolution_ = ScreenResolution::s_1280_720;

    mattmath::Vector2I convert_resolution_to_ivec(
        ScreenResolution resolution) const;
    mattmath::Vector2F convert_resolution_to_vec(
        ScreenResolution resolution) const;
    ScreenResolution convert_string_to_resolution(
        const std::string& string) const;
    ScreenResolution convert_vec_to_resolution(
        const mattmath::Vector2F& vec) const;
    ScreenResolution convert_ivec_to_resolution(
        const mattmath::Vector2I& vec) const;
};
