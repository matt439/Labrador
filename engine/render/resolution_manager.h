#pragma once

#include <string>
#include "engine/math/matt_math.h"
#include "engine/render/screen_resolution.h"

namespace artattack
{
    // The size everything above the backend lays itself out against.
    // ViewportManager derives every viewport and every divider from it, and it
    // is what a game means when it says "the resolution".
    //
    // IT IS A SIZE, AND ScreenResolution IS A LABEL FOR FOUR OF ITS VALUES.
    // That is the way round it has to be, because a window can be any size at
    // all - a user drags an edge, a borderless full-screen window takes the
    // whole monitor - and a closed enum of four 16:9 presets cannot hold most
    // of them. The enum used to be the ONLY member, so any size outside those
    // four was answered with 1280x720 and every layout above here was computed
    // for a window that was not on the screen.
    class ResolutionManager
    {
    public:
        // The two defaults are one decision, not two that must be kept in step:
        // the size is whatever the default preset's size is.
        ResolutionManager();

        mattmath::Vector2I resolution_ivec() const;
        mattmath::Vector2F resolution_vec() const;
        std::string resolution_string() const;
        ScreenResolution resolution() const;
        void set_resolution(ScreenResolution resolution);
        void set_resolution(const std::string& resolution);
        void set_resolution(const mattmath::Vector2F& resolution);
        void set_resolution(const mattmath::Vector2I& resolution);

        // The size as given, with no preset anywhere near it. This is what a
        // real window is, and Application calls it from on_window_size_changed
        // so that a resize, a full-screen toggle and a dragged window edge all
        // land here - they all arrive as WM_SIZE, so one call covers the three.
        //
        // IT MUST NOT BE ROUTED THROUGH set_resolution(const Vector2I&), and
        // that is not a style preference. That overload converts to the enum
        // first, and the conversion answers 1280x720 for anything outside the
        // four presets - so pushing a window size through it reports 720p for
        // every window that is not exactly one of them, which is the bug rather
        // than the fix. This setter exists because a non-coercing path had to.
        //
        // The label follows only on an EXACT match. Ask for 1920x1080 and
        // resolution() becomes s_1920_1080; ask for 1600x900 and it keeps
        // whatever preset was last requested, because that size does not have a
        // name in this enum and inventing one would be the coercion again.
        // resolution() and resolution_string() therefore mean "the preset last
        // asked for" and can differ from the live size - a game showing a
        // resolution in an options menu wants the label, and anything laying
        // itself out wants resolution_ivec().
        //
        // Throws std::invalid_argument for a non-positive extent (T6): it is a
        // divisor for everything downstream. No window can reach it - WM_SIZE
        // is guarded against SIZE_MINIMIZED and WM_GETMINMAXINFO floors the
        // drag - so this is aimed at a caller that computed a size wrongly.
        //
        // ONE CONSEQUENCE WORTH A CLIENT'S ATTENTION: a game can now be handed
        // a size that is not 16:9, which the closed enum used to make
        // unreachable. Anything assuming a single scale factor from one axis
        // has a letterboxing decision to make that it did not have before. That
        // is correct rather than free.
        void set_resolution_exactly(const mattmath::Vector2I& size);

        static std::string convert_resolution_to_string(
            ScreenResolution resolution);

    private:
        // The preset last asked for: a label, and what resolution() answers.
        ScreenResolution resolution_ = ScreenResolution::s_1280_720;

        // The live size, and the authority. Every accessor above that returns a
        // size returns this one.
        mattmath::Vector2I size_;

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
}
