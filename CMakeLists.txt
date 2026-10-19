cmake_minimum_required(VERSION 3.20)
project(strikers_ai_core VERSION 0.1.0 LANGUAGES CXX)

# =============================================================================
# 1. Padrão C++20 Estrito e Flags de Compilação Rigorosas
# =============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -pedantic)

# =============================================================================
# 2. Dependências Externas: SDL2 e nlohmann/json
# =============================================================================
list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(SDL2 REQUIRED)

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
    )
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
endif()

# =============================================================================
# 3. Biblioteca de Domínio, Entrada (SDL2 HAL) e Ingestão de Dados
# =============================================================================
add_library(strikers_engine_core STATIC
    src/input/config_loader.cpp
    src/input/input_manager.cpp
    src/core/character.cpp
    src/loader/character_loader.cpp
)

target_include_directories(strikers_engine_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${SDL2_INCLUDE_DIRS}
)

if(TARGET SDL2::SDL2)
    target_link_libraries(strikers_engine_core PUBLIC SDL2::SDL2 nlohmann_json::nlohmann_json)
else()
    target_link_libraries(strikers_engine_core PUBLIC ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
endif()

# =============================================================================
# 4. Executável de Teste de Integração e Verificação (build/tests/test_engine_base)
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
