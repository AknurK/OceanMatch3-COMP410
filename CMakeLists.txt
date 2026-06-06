cmake_minimum_required(VERSION 3.16)

project(OceanMatch3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Packages ────────────────────────────────────────────────────────────────
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(assimp REQUIRED)
find_package(glm REQUIRED)
find_package(JPEG REQUIRED)
find_package(ZLIB REQUIRED)

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

# ── Link libraries ───────────────────────────────────────────────────────────
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    glfw
    assimp::assimp
    glm::glm
    JPEG::JPEG
    ZLIB::ZLIB
)

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