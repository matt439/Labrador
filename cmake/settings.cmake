# The shared compiler-settings target (docs/design/ARCHITECTURE.md, The
# build): every real target links artattack_settings, so compiler
# strictness lives in exactly one place (T5). Values carried over from the
# solution this build replaced.
add_library(artattack_settings INTERFACE)

target_compile_features(artattack_settings INTERFACE cxx_std_20)

target_compile_options(artattack_settings INTERFACE
    /W4          # highest regular warning level
    /WX          # warnings are errors, zero suppressions
    /permissive- # standards conformance mode
    /sdl         # additional security checks
)

target_compile_definitions(artattack_settings INTERFACE
    UNICODE _UNICODE
    WIN32 _WINDOWS
    NOMINMAX             # std::min/max, never the Windows.h macros
    WIN32_LEAN_AND_MEAN
)
