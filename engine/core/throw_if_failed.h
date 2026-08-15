#pragma once

#include <Windows.h>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace labrador
{
    // Helper class for COM exceptions
    //
    // std::runtime_error, NOT std::exception, AND THE DIFFERENCE IS A CONTRACT.
    // render/resource_factory.h declares that add_texture_asset throws
    // std::runtime_error naming the texture and the format when the device
    // will not take it. The GL backend does exactly that at four sites; the
    // D3D11 backend reached this class instead, which derived from
    // std::exception, so the sentence in that header had never once described
    // the implementation that existed when it was written. Deriving here fixes
    // every ThrowIfFailed site at once rather than the one the review found -
    // and a backend is free to keep throwing this, which is the point.
    //
    // The message is formatted once, in the constructor, and held by the base.
    // It used to be written into a function-local static char[64] on each call
    // to what(), which made two live com_exceptions share one buffer.
    class com_exception : public std::runtime_error
    {
    public:
        // Not noexcept, and it cannot be: the base stores a string. Nothing
        // constructs one of these outside a throw expression.
        explicit com_exception(HRESULT hr) : std::runtime_error(format(hr)) {}

    private:
        static std::string format(HRESULT hr)
        {
            char text[64] = {};
            sprintf_s(text, "Failure with HRESULT of %08X",
                static_cast<unsigned int>(hr));
            return text;
        }
    };

    // Helper utility converts D3D API failures into exceptions.
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw com_exception(hr);
        }
    }
}
