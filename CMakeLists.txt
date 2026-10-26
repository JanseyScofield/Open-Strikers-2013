cmake_minimum_required(VERSION 3.20)
project(strikers_ai_core VERSION 0.2.0 LANGUAGES CXX)

# =============================================================================
# 1. Padrão C++20 Estrito e Flags de Compilação Rigorosas
# =============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -pedantic)

# =============================================================================
# 2. Dependências Externas: SDL2, OpenGL e nlohmann/json
# =============================================================================
list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(SDL2 REQUIRED)
set(OpenGL_GL_PREFERENCE LEGACY)
find_package(OpenGL REQUIRED)

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
    )
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
endif()

# =============================================================================
# 3. Biblioteca do Motor (Domínio, Entrada SDL2, Física e Renderização OpenGL)
# =============================================================================
add_library(strikers_engine_core STATIC
    src/input/config_loader.cpp
    src/input/input_manager.cpp
    src/core/character.cpp
    src/loader/character_loader.cpp
    src/physics/kinematic_body.cpp
    src/physics/ball.cpp
    src/gameplay/duel_system.cpp
    src/render/window.cpp
    src/render/camera.cpp
    src/render/debug_renderer.cpp
)

target_include_directories(strikers_engine_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
)

if(TARGET SDL2::SDL2)
    target_link_libraries(strikers_engine_core PUBLIC
        SDL2::SDL2
        OpenGL::GL
        nlohmann_json::nlohmann_json
    )
else()
    target_link_libraries(strikers_engine_core PUBLIC
        ${SDL2_LIBRARIES}
        OpenGL::GL
        nlohmann_json::nlohmann_json
    )
endif()

# =============================================================================
# 4. Executável Principal do Jogo (build/open_strikers_app)
# =============================================================================
add_executable(open_strikers_app
    src/main.cpp
)

set_target_properties(open_strikers_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_link_libraries(open_strikers_app PRIVATE strikers_engine_core)

# =============================================================================
# 5. Executável de Teste de Integração e Verificação (build/tests/test_engine_base)
# =============================================================================
enable_testing()

add_executable(test_engine_base
    tests/test_engine_base.cpp
)

set_target_properties(test_engine_base PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

target_link_libraries(test_engine_base PRIVATE strikers_engine_core)

add_test(
    NAME test_engine_base
    COMMAND test_engine_base
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
