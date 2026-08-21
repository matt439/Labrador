#pragma once

#include <Windows.h>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace labrador
{
    // WHERE THIS LIVES, because it moved and the move is the argument. Its
    // only includers are the two Direct3D backends - three translation units
    // each, and nothing else in the tree - so the rule that a backend's
    // headers are its own leaves two shapes: a copy in d3d11/ and a copy in
    // d3d12/, or one file here that neither owns. It is the second, for the
    // reason render/sprite.hlsl beside it gives for being one file compiled
    // twice: a second copy is a file that can silently disagree with the
    // first, and what these two would disagree about is an exception type
    // whose whole point is that a catch site cannot tell (see
    // tests/render/throw_if_failed_tests.cpp, which is why that test is in
    // this module now and still compiles in all five configurations).
    //
    // IT USED TO LIVE IN core/, WHICH IS THE FOLDER THAT HAD ALREADY REFUSED
    // IT. core/registry.h keeps its COM-facing specialisation out - "rather
    // than dragging <wrl/client.h> in here" - and puts it in
    // render/d3d11/backend.h, where COM is already in scope. This file dragged
    // <Windows.h> and an HRESULT into the one module everything is allowed to
    // lean on, in the same folder as the comment saying not to, and nothing in
    // core/ ever included it.
    //
    // THE NAMES ARE MICROSOFT'S AND STAY THAT WAY. NOTICE lists this file
    // among those adopted from the DirectX samples. ThrowIfFailed is
    // PascalCase where CONVENTIONS wants snake_case and com_exception is
    // snake_case where it wants PascalCase, because upstream spells them so -
    // and one of the callers is upstream's own file, carried with upstream's
    // naming: engine/render/d3d11/device_resources.cpp reaches it at fifteen
    // sites. Renaming would make that file call an engine function in the
    // engine's style, which is the one thing NOTICE says these files do not
    // do.

    // Helper class for COM exceptions
    //
    // std::runtime_error, NOT std::exception, AND THE DIFFERENCE IS A CONTRACT.
    // render/resource_factory.h declares that add_texture_asset throws
    // std::runtime_error naming the texture and the format when the device
    // will not take it. The GL backend does exactly that at four sites; both
    // Direct3D backends reach this class instead, which derived from
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
