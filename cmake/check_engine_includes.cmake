# docs/design/ARCHITECTURE.md: an engine file including a game header "fails to
# compile, and that is the feature (T5)".
#
# Includes are written from the repository root (CONVENTIONS, Files), so an
# engine file says "engine/render/renderer.h" and the engine's include root is
# the directory above engine/. Built standalone there is no game/ beside
# engine/, so the compiler refuses a game include on its own.
#
# SO WHY THIS STILL RUNS. The compiler enforces the rule only when this
# repository is built standalone, which is the configuration a contributor and
# CI have and not the one a downstream game has: consumed as a submodule, its
# own headers are on an include path and "game/..." resolves. The wall below is
# the half that does not depend on who is building. It runs on every build,
# which is the property that matters: the rule is enforced, not merely written
# down.
#
# Run with: cmake -DENGINE_DIR=<path> -P cmake/check_engine_includes.cmake

if(NOT DEFINED ENGINE_DIR)
    message(FATAL_ERROR "check_engine_includes.cmake: ENGINE_DIR is not set")
endif()

file(GLOB_RECURSE engine_sources "${ENGINE_DIR}/*.h" "${ENGINE_DIR}/*.cpp")

set(offenders "")
foreach(source IN LISTS engine_sources)
    # Both spellings: the root-relative "game/..." the include roots make
    # possible, and the "../game/..." a relative include would reach for.
    file(STRINGS "${source}" hits
        REGEX "^[ \t]*#[ \t]*include[ \t]*[\"<](\\.\\./)*game/")
    foreach(hit IN LISTS hits)
        string(STRIP "${hit}" hit)
        list(APPEND offenders "${source}\n      ${hit}")
    endforeach()
endforeach()

if(offenders)
    list(JOIN offenders "\n    " report)
    message(FATAL_ERROR
        "Dependencies point one way. These engine files include a game header:\n"
        "    ${report}\n"
        "  See docs/design/ARCHITECTURE.md, Modules.")
endif()

# The second wall: a backend's folder is for itself.
#
# ARCHITECTURE says a file outside engine/render/<backend>/ including that
# backend's header is a mistake. A header carries a backend further in one line
# than a translation unit can - one public header naming <d3d11_1.h> puts it on
# the command line of every sample state and every downstream game - so the
# check reads headers as well as .cpp files.
#
# THE FOLDER, NOT ONE FILENAME IN IT. Every header in a backend folder is that
# backend's: device_resources.h opens with <d3d11_1.h> and <dxgi1_6.h> as
# surely as backend.h does. The check also catches the angle-bracket spelling,
# which the include root admits exactly as it admits the quoted one, and the
# "../render/d3d11/..." relative form. A subfolder of render/ IS a backend, so
# a new backend needs no edit here.
#
# THE MODULE IS CAPTURED TOO, NOT LISTED. engine/audio/, engine/input/ and
# engine/app/ have platform folders under exactly the same rule, and another
# module that grows them needs no edit here either.
#
# WHAT THE PATTERN PAYS FOR CAPTURING THE MODULE is that it cannot match on
# shape alone. A three-component include is not by itself an engine one -
# <rapidjson/error/en.h> has that shape - so the path must be the engine's own
# spelling: rooted at engine/, or relative with ../, which are the two forms
# CONVENTIONS and the compiler between them admit.
#
# Deliberately not a compiler error: the include root has to admit
# "engine/render/d3d11/backend.h" for the folder's own three files, and there is
# no include path that admits it there and refuses it next door. Same shape as
# the game-header wall above, and for the same reason.

set(backend_offenders "")
foreach(source IN LISTS engine_sources)
    # Both spellings and the relative form, matching the game-header wall - a
    # rule that only catches one of three ways to write the same include is a
    # rule the next person writes around without meaning to.
    file(STRINGS "${source}" hits
        REGEX "^[ \t]*#[ \t]*include[ \t]*[\"<]((\\.\\./)*engine/|(\\.\\./)+)[A-Za-z0-9_]+/[A-Za-z0-9_]+/[A-Za-z0-9_]+\\.h[\">]")
    foreach(hit IN LISTS hits)
        # The folder a header lives in is the one folder allowed to name it.
        # Extract the module and the backend from the include and compare them
        # with the directory the file is in, so neither a new backend nor a new
        # module with backends needs an edit here.
        string(REGEX MATCH "([A-Za-z0-9_]+)/([A-Za-z0-9_]+)/[A-Za-z0-9_]+\\.h" ignored "${hit}")
        get_filename_component(source_dir "${source}" DIRECTORY)
        if(NOT source_dir MATCHES "/${CMAKE_MATCH_1}/${CMAKE_MATCH_2}$")
            string(STRIP "${hit}" hit)
            list(APPEND backend_offenders "${source}\n      ${hit}")
        endif()
    endforeach()
endforeach()

if(backend_offenders)
    list(JOIN backend_offenders "\n    " report)
    message(FATAL_ERROR
        "A backend header is for its own folder. These files reach across:\n"
        "    ${report}\n"
        "  Everything that draws goes through DrawList, and everything that\n"
        "  builds a resource on a device goes through render/resource_factory.h.\n"
        "  Everything that makes a noise goes through audio/audio_device.h.\n"
        "  Everything that reads a pad goes through input/gamepad_reader.h.\n"
        "  See docs/design/ARCHITECTURE.md, Modules.")
endif()
