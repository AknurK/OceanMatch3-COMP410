cmake_minimum_required(VERSION 3.16)

project(OceanMatch3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Packages ────────────────────────────────────────────────────────────────
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm QUIET)
if(NOT glm_FOUND)
    find_path(GLM_INCLUDE_DIR NAMES glm/glm.hpp)
    if(NOT GLM_INCLUDE_DIR)
        message(FATAL_ERROR "GLM headers not found. Install GLM or set GLM_INCLUDE_DIR.")
    endif()
    add_library(glm::glm INTERFACE IMPORTED)
    set_target_properties(glm::glm PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${GLM_INCLUDE_DIR}"
    )
endif()
find_package(JPEG REQUIRED)
find_package(ZLIB REQUIRED)

# ── Assimp handling ─────────────────────────────────────────────────────────
# On some macOS/Homebrew setups, the imported assimp::assimp CMake target may
# contain an invalid SDK-specific zlib path. Therefore, we locate the Assimp
# headers and library directly instead of linking through assimp::assimp.
if(APPLE)
    execute_process(
        COMMAND brew --prefix
        OUTPUT_VARIABLE HOMEBREW_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    find_path(ASSIMP_INCLUDE_DIR
        NAMES assimp/Importer.hpp
        PATHS
            "${HOMEBREW_PREFIX}/include"
            "${HOMEBREW_PREFIX}/opt/assimp/include"
            "/opt/homebrew/include"
            "/opt/homebrew/opt/assimp/include"
            "/usr/local/include"
            "/usr/local/opt/assimp/include"
    )

    find_library(ASSIMP_LIBRARY
        NAMES assimp
        PATHS
            "${HOMEBREW_PREFIX}/lib"
            "${HOMEBREW_PREFIX}/opt/assimp/lib"
            "/opt/homebrew/lib"
            "/opt/homebrew/opt/assimp/lib"
            "/usr/local/lib"
            "/usr/local/opt/assimp/lib"
    )

    if(NOT ASSIMP_INCLUDE_DIR OR NOT ASSIMP_LIBRARY)
        message(FATAL_ERROR "Assimp not found. On macOS, install it with: brew install assimp")
    endif()
else()
    find_package(assimp REQUIRED)
endif()

# ── Source files ─────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
)

# ── Include directories ──────────────────────────────────────────────────────
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

if(APPLE)
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${ASSIMP_INCLUDE_DIR}"
    )
endif()

# ── Link libraries ───────────────────────────────────────────────────────────
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        OpenGL::GL
        glfw
        glm::glm
        JPEG::JPEG
        ZLIB::ZLIB
        "${ASSIMP_LIBRARY}"
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        OpenGL::GL
        glfw
        glm::glm
        JPEG::JPEG
        ZLIB::ZLIB
        assimp::assimp
    )
endif()

# ── Copy runtime assets/shaders next to executable ───────────────────────────
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/shaders"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
)
