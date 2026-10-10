# The shared compiler-settings target (docs/design/ARCHITECTURE.md, The
# build): every real target links labrador_settings, so compiler
# strictness lives in exactly one place (T5). Values carried over from the
# solution this build replaced.
add_library(labrador_settings INTERFACE)

target_compile_features(labrador_settings INTERFACE cxx_std_20)

# The standard language, not a vendor's dialect of it: CMake otherwise asks
# clang for gnu++20. MSVC has no dialect to ask for, and /permissive- below is
# the same refusal in its spelling.
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
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
else()
    # Clang, which is the WebAssembly build's compiler (docs/port/web.md). The
    # same two rules in its spelling: everything the compiler can say, and
    # every one of them an error, with no suppression anywhere in the tree.
    target_compile_options(labrador_settings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Werror

        # /fp:precise, argued again rather than translated, because clang
        # has no switch that means it. Two things the MSVC build does not do
        # - /fp:precise has left contraction to its own /fp:contract switch
        # since Visual Studio 2022 - are live under clang. Reassociation and
        # NaN elimination come only with
        # -ffast-math or its parts, which this tree never asks for, and that
        # absence is the decision - the same deletion-not-blank argument as
        # above, made by this comment rather than by a flag. Contraction is
        # the other one, and it is ON by default: clang fuses a * b + c into
        # one fused multiply-add wherever the target has one, rounding once
        # where the source rounds twice. Core WebAssembly has no scalar FMA,
        # so it would change nothing there yet - and "nothing yet" is exactly
        # the inherited-default reasoning the MSVC block refuses. Off, stated.
        -ffp-contract=off
    )
endif()

# T6 makes a throw the engine's way of reporting failure, and Emscripten does
# not catch C++ exceptions unless asked: native WebAssembly exception handling,
# on both sides of the link.
#
# THE STACK IS THE ONE EVERY WINDOWS BUILD ALREADY HAS, 1 MiB, where
# Emscripten's default is 64 KiB. Code in this tree was written and measured
# against the larger one - LineSweeper's particle field holds its ten thousand
# particles inline and its tests build it as a local, and the JSON reader reads
# through a 64 KiB buffer on the stack - and a smaller stack in one build would
# make "fits" a per-platform answer. The heap grows rather than being fixed, so
# running out of it is an allocation failure rather than an abort.
if(EMSCRIPTEN)
    target_compile_options(labrador_settings INTERFACE -fwasm-exceptions)
    target_link_options(labrador_settings INTERFACE
        -fwasm-exceptions
        -sSTACK_SIZE=1048576
        -sALLOW_MEMORY_GROWTH=1
    )
endif()

if(WIN32)
    target_compile_definitions(labrador_settings INTERFACE
        UNICODE _UNICODE
        WIN32 _WINDOWS
        NOMINMAX             # std::min/max, never the Windows.h macros
        WIN32_LEAN_AND_MEAN
    )
endif()
