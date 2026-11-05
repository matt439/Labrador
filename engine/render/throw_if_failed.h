#pragma once

#include <Windows.h>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace labrador
{
    // WHERE THIS LIVES, and why it is not in core/. Its only engine
    // includers are the two Direct3D backends - three translation units each
    // - so the rule that a backend's headers are its own leaves two shapes: a
    // copy in d3d11/ and a copy in d3d12/, or one file here that neither owns.
    // It is the second, for the reason render/sprite.hlsl beside it gives for
    // being one file rather than one per backend: a second copy is a file
    // that can silently disagree with the first, and what these two would
    // disagree about is an exception type whose whole point is that a catch
    // site cannot tell (tests/render/throw_if_failed_tests.cpp pins it, and
    // compiles in every configuration).
    //
    // NOT core/. This file carries <Windows.h> and an HRESULT, which is
    // exactly what the one module everything may lean on must not - and
    // core/registry.h keeps COM out for the same reason, specialising for a
    // ComPtr in render/d3d11/backend.h, where COM is already in scope.
    //
    // THE NAMES ARE MICROSOFT'S AND STAY THAT WAY. NOTICE lists this file
    // among those adopted from the DirectX samples. ThrowIfFailed is
    // PascalCase where CONVENTIONS wants snake_case and com_exception is
    // snake_case where it wants PascalCase, because upstream spells them so -
    // and one of the callers is upstream's own file, carried with upstream's
    // naming: engine/render/d3d11/device_resources.cpp reaches it at fifteen
    // sites. Renaming would make that file call an engine function in the
    // engine's style, which is the one thing NOTICE says these files do not
    // do. The two-argument overloads keep upstream's names so that one call
    // spelling covers both forms; hresult_name is the engine's own and is
    // spelt the engine's way.

    // An HRESULT as the engine's own messages spell it: "HRESULT 80004005",
    // eight upper-case hexadecimal digits, the same digits com_exception's
    // one-argument message prints, so a search for a code finds both.
    inline std::string hresult_name(HRESULT hr)
    {
        char text[24] = {};
        sprintf_s(text, "HRESULT %08X", static_cast<unsigned int>(hr));
        return text;
    }

    // Helper class for COM exceptions
    //
    // std::runtime_error, NOT std::exception. render/resource_factory.h
    // declares that add_texture_asset throws std::runtime_error, so a caller
    // catches one type whichever backend refused. This base keeps the type
    // half of that promise at every ThrowIfFailed site; it does not keep the
    // message half. A message naming the texture and the format is written by
    // each backend's add_texture_asset, which is the code that knows them.
    //
    // The message is formatted once, in the constructor, and held by the base
    // - not written into a function-local static on each call to what(), which
    // two live com_exceptions would share.
    class com_exception : public std::runtime_error
    {
    public:
        // Not noexcept, and it cannot be: the base stores a string. Nothing
        // constructs one of these outside a throw expression.
        explicit com_exception(HRESULT hr) : std::runtime_error(format(hr)) {}

        // The message is `what`, then the HRESULT in hresult_name's spelling:
        // "The swap chain could not be created (HRESULT 887A0005)."
        com_exception(HRESULT hr, const char* what) : std::runtime_error(format(hr, what)) {}

    private:
        static std::string format(HRESULT hr)
        {
            char text[64] = {};
            sprintf_s(text, "Failure with HRESULT of %08X",
                static_cast<unsigned int>(hr));
            return text;
        }

        static std::string format(HRESULT hr, const char* what)
        {
            return std::string(what) + " (" + hresult_name(hr) + ").";
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

    // The engine's form: `what` says which operation failed, in a sentence
    // such as "The swap chain could not be created", and the thrown
    // com_exception's message is that sentence followed by the HRESULT.
    // Engine-written Direct3D code calls this one; the one-argument form is
    // kept for the files carried from upstream.
    inline void ThrowIfFailed(HRESULT hr, const char* what)
    {
        if (FAILED(hr))
        {
            throw com_exception(hr, what);
        }
    }
}
