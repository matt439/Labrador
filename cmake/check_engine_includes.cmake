# docs/design/ARCHITECTURE.md: an engine file including a game header "fails to
# compile, and that is the feature (T5)". It does not fail yet, and E1 - the
# item that was going to fix it - established why not rather than fixing it.
#
# Includes are written from the repository root (CONVENTIONS, Files), so an
# engine file says "engine/render/renderer.h". That means the engine's own
# include root has to be the directory above engine/ - and in this tree, that
# directory also holds game/. There is no include path that admits the first
# spelling and refuses the second while the two are siblings. What closes it is
# the repo split: engine/ and samples/ in one repository, the paint-shooter in
# its own, consuming the engine as a submodule. E1 made the include roots
# relative so that split is a move and not a build rewrite.
#
# Until then, this is the wall. It runs on every build, which is the property
# that matters: the rule is enforced, not merely written down.
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
        "  See docs/design/ARCHITECTURE.md, The module graph.")
endif()

# The second wall: a backend's folder is for itself.
#
# ARCHITECTURE says a file outside engine/render/<backend>/ including that
# backend's header would be a mistake. That was review's job until now, and
# review counted .cpp files - which is how engine/assets/resource_loader.h came
# to name ID3D11Device1 in a constructor, and engine/app/application.h to
# include it, and every state file in both samples to compile <d3d11_1.h>
# without anyone choosing that. A header carries a backend further in one line
# than a translation unit can, so the check reads both.
#
# THE FOLDER, NOT ONE FILENAME IN IT. This used to match "backend.h" alone,
# which left three ways through: the angle-bracket spelling, which the include
# root admits exactly as it admits the quoted one; the "../render/d3d11/..."
# relative form; and - the one that matters - EVERY OTHER HEADER IN THE FOLDER.
# device_resources.h opens with <d3d11_1.h>, <dxgi1_6.h> and a D3DDeviceNotify
# interface, and naming it from engine/app/application.h passed this check
# silently while doing precisely the damage the paragraph above describes. It
# is the same header as that incident. gl_functions.h and sprite_shader.h were
# the same hole on the other backend.
#
# So the rule is now what it always meant: a subfolder of render/ IS a backend,
# and its headers are its own. Nothing needs editing here when a fourth one
# lands, which is the property the filename form was reaching for and missed.
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
        REGEX "^[ \t]*#[ \t]*include[ \t]*[\"<](\\.\\./)*(engine/)?render/[A-Za-z0-9_]+/[A-Za-z0-9_]+\\.h[\">]")
    foreach(hit IN LISTS hits)
        # The folder a header lives in is the one folder allowed to name it.
        # Extract the backend from the include and compare it with the
        # directory the file is in, so a new backend needs no edit here.
        string(REGEX MATCH "render/([A-Za-z0-9_]+)/[A-Za-z0-9_]+\\.h" ignored "${hit}")
        get_filename_component(source_dir "${source}" DIRECTORY)
        if(NOT source_dir MATCHES "/render/${CMAKE_MATCH_1}$")
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
        "  See docs/design/ARCHITECTURE.md, Modules.")
endif()
