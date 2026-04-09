cmake_minimum_required(VERSION 3.25)
project(opengl_template LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Dependencies ---

include(FetchContent)

# GLFW via FetchContent
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

# GLM via FetchContent
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# CLI11 via FetchContent
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11
    GIT_TAG        v2.4.2
)
FetchContent_MakeAvailable(CLI11)

# Dear ImGui via FetchContent
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)

# glad2 (vendored, GL 4.5 core)
add_library(glad STATIC deps/glad/src/gl.c)
target_include_directories(glad PUBLIC deps/glad/include)

# --- Main executable ---

add_executable(opengl_template
    src/main.cpp
    src/core/shader.cpp
    src/core/gl_init.cpp
    src/core/camera.cpp
)

target_include_directories(opengl_template PRIVATE src/core)
target_link_libraries(opengl_template PRIVATE glad glfw glm::glm CLI11::CLI11 imgui_lib)
target_compile_options(opengl_template PRIVATE
    -Wall -Wextra -Wpedantic
    -Wshadow
    -Wswitch-enum
    -Wconversion -Wsign-conversion
    -Wnon-virtual-dtor
    -Werror=return-type
    -Werror=switch-enum
    -Werror=implicit-fallthrough
    -Werror=unused-result
)

# Copy shader subdirs to build directory (skips non-shader files like CLAUDE.md)
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders/render  $<TARGET_FILE_DIR:opengl_template>/shaders/render
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders/compute $<TARGET_FILE_DIR:opengl_template>/shaders/compute
    COMMENT "Copying shaders to build directory"
)
add_dependencies(copy_shaders opengl_template)

# On Linux, link OpenGL explicitly
if(UNIX AND NOT APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(opengl_template PRIVATE OpenGL::GL)
endif()

# --- Test executable ---

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.8.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(app_tests
    tests/test_shader_loading.cpp
    src/core/shader.cpp
    src/core/gl_init.cpp
)
target_include_directories(app_tests PRIVATE src/core)
target_link_libraries(app_tests PRIVATE Catch2::Catch2WithMain glad glfw)
target_compile_options(app_tests PRIVATE
    -Wall -Wextra -Wpedantic
    -Wshadow
    -Wswitch-enum
    -Wconversion -Wsign-conversion
    -Wnon-virtual-dtor
    -Werror=return-type
    -Werror=switch-enum
    -Werror=implicit-fallthrough
    -Werror=unused-result
)
if(UNIX AND NOT APPLE)
    target_link_libraries(app_tests PRIVATE OpenGL::GL)
endif()
add_dependencies(app_tests copy_shaders)

include(CTest)
include(Catch)
catch_discover_tests(app_tests)
