# =============================================================================
# SDL2Config.cmake - Local CMake package configuration for SDL2 (2.30.0 ABI)
# Links against system /usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0 and provides
# standard SDL2::SDL2 targets and SDL2_INCLUDE_DIRS / SDL2_LIBRARIES variables.
# =============================================================================

find_path(SDL2_INCLUDE_DIR
    NAMES SDL2/SDL.h SDL.h
    PATHS
        /usr/include
        /usr/local/include
        "${CMAKE_CURRENT_LIST_DIR}/../third_party/SDL2/include"
        "${CMAKE_CURRENT_LIST_DIR}/../third_party/SDL2/include/SDL2"
    NO_DEFAULT_PATH
)

find_library(SDL2_LIBRARY
    NAMES SDL2 SDL2-2.0 libSDL2-2.0.so.0
    PATHS
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib
        /usr/lib64
        /usr/lib
)

if(SDL2_INCLUDE_DIR AND SDL2_LIBRARY)
    set(SDL2_FOUND TRUE)
    set(SDL2_INCLUDE_DIRS
        "${CMAKE_CURRENT_LIST_DIR}/../third_party/SDL2/include"
        "${CMAKE_CURRENT_LIST_DIR}/../third_party/SDL2/include/SDL2"
    )
    if(EXISTS "/usr/include/SDL2")
        list(PREPEND SDL2_INCLUDE_DIRS "/usr/include" "/usr/include/SDL2")
    endif()
    set(SDL2_LIBRARIES "${SDL2_LIBRARY}")

    if(NOT TARGET SDL2::SDL2)
        add_library(SDL2::SDL2 SHARED IMPORTED)
        set_target_properties(SDL2::SDL2 PROPERTIES
            IMPORTED_LOCATION "${SDL2_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIRS}"
        )
    endif()
else()
    set(SDL2_FOUND FALSE)
    if(SDL2_FIND_REQUIRED)
        message(FATAL_ERROR "Could not locate SDL2 library or headers.")
    endif()
endif()
