# MattMath — the math static library. Depends on nothing but the
# DirectXTK headers it currently wraps (docs/design/ARCHITECTURE.md).

# xaudio2redist first: directxtk's config references Microsoft::XAudio2Redist
# but does not find it itself.
find_package(xaudio2redist CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)

add_library(MattMath STATIC
    ericson_math.cpp
    matt_math.cpp
)
target_include_directories(MattMath PUBLIC "${CMAKE_SOURCE_DIR}")
target_link_libraries(MattMath
    PUBLIC Microsoft::DirectXTK
    PRIVATE artattack_settings
)
