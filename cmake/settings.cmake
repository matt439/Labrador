# The shared compiler-settings target (docs/design/ARCHITECTURE.md, The
# build): every real target links labrador_settings, so compiler
# strictness lives in exactly one place (T5). Values carried over from the
# solution this build replaced.
add_library(labrador_settings INTERFACE)

target_compile_features(labrador_settings INTERFACE cxx_std_20)

target_compile_options(labrador_settings INTERFACE
    /W4          # highest regular warning level
    /WX          # warnings are errors, zero suppressions
    /permissive- # standards conformance mode
    /sdl         # additional security checks

    # IEEE-754 semantics, stated rather than inherited. This is already the
    # compiler default, and that is the reason to write it down: a great deal
    # of this tree is only correct under exactly-rounded arithmetic that does
    # not reassociate. Vector2F::normalized tests its length against zero,
    # narrow_phase compares an axis against Vector2F::ZERO through an exact
    # operator==, every are_equal tolerance assumes a known rounding bound,
    # and the collision path's NaN handling assumes NaN is produced and
    # propagated rather than optimised away - which /fp:fast permits.
    #
    # The solution this build replaced used /fp:fast in all four
    # configurations, so "swap this for speed" is not a hypothetical. Written
    # explicitly, that change means deleting a stated decision instead of
    # filling a blank.
    /fp:precise
)

target_compile_definitions(labrador_settings INTERFACE
    UNICODE _UNICODE
    WIN32 _WINDOWS
    NOMINMAX             # std::min/max, never the Windows.h macros
    WIN32_LEAN_AND_MEAN
)
