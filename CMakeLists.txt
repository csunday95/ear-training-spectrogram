cmake_minimum_required(VERSION 3.25)
project(ear_training LANGUAGES C CXX)

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
    URL      https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(glfw)

# GLM via FetchContent
FetchContent_Declare(
    glm
    URL      https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(glm)

# CLI11 via FetchContent
FetchContent_Declare(
    CLI11
    URL      https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.4.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(CLI11)

# Dear ImGui via FetchContent
FetchContent_Declare(
    imgui
    URL      https://github.com/ocornut/imgui/archive/refs/tags/v1.91.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
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

# glad2 (vendored, GL 4.5 core). SYSTEM suppresses warnings from khrplatform.h in dependents.
add_library(glad STATIC deps/glad/src/gl.c)
target_include_directories(glad SYSTEM PUBLIC deps/glad/include)

# nlohmann/json (header-only JSON for config file loading)
FetchContent_Declare(
    nlohmann_json
    URL      https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# miniaudio (single-header, header-only interface target)
FetchContent_Declare(
    miniaudio
    URL      https://github.com/mackron/miniaudio/archive/refs/tags/0.11.21.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(miniaudio)
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${miniaudio_SOURCE_DIR})

# --- Compiler warning flags ---

set(COMMON_WARNINGS
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

# --- Main executable ---

add_executable(ear_training
    src/main.cpp
    src/core/shader.cpp
    src/core/gl_init.cpp
    src/core/gpu_pipeline.cpp
    src/core/app_config.cpp
    src/audio/audio_capture.cpp
    src/audio/fft_config.cpp
    src/audio/noise_floor.cpp
    src/audio/pitch_detect.cpp
    src/audio/yin.cpp
    src/audio/miniaudio_impl.cpp
    src/ui/imgui_renderer.cpp
    src/ui/waveform_widget.cpp
    src/ui/tuner_widget.cpp
    src/ui/spectrum_axis_widget.cpp
)

target_include_directories(ear_training PRIVATE src/core src/audio src/ui)
# nlohmann/json added as SYSTEM to suppress warnings from its headers
target_include_directories(ear_training SYSTEM PRIVATE ${nlohmann_json_SOURCE_DIR}/include)
target_link_libraries(ear_training PRIVATE glad glfw glm::glm CLI11::CLI11 imgui_lib miniaudio)
target_compile_options(ear_training PRIVATE ${COMMON_WARNINGS})
# miniaudio is third-party — suppress warnings for its implementation TU only
set_source_files_properties(src/audio/miniaudio_impl.cpp PROPERTIES COMPILE_OPTIONS "-w")

# Copy shader subdirs to build directory
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders/compute $<TARGET_FILE_DIR:ear_training>/shaders/compute
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders/render  $<TARGET_FILE_DIR:ear_training>/shaders/render
    COMMENT "Copying shaders to build directory"
)
add_dependencies(copy_shaders ear_training)

# On Linux, link OpenGL explicitly
if(UNIX AND NOT APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(ear_training PRIVATE OpenGL::GL)
endif()

# On Linux, link threading for miniaudio
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(ear_training PRIVATE Threads::Threads)
    # miniaudio needs dl and m on Linux
    target_link_libraries(ear_training PRIVATE dl m)
endif()

# --- Test executable ---

FetchContent_Declare(
    Catch2
    URL      https://github.com/catchorg/Catch2/archive/refs/tags/v3.8.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(app_tests
    tests/test_shader_loading.cpp
    tests/test_ring_buffer.cpp
    tests/test_fft.cpp
    tests/test_music_theory.cpp
    tests/test_pitch_detect.cpp
    tests/test_yin.cpp
    src/core/shader.cpp
    src/core/gl_init.cpp
    src/audio/fft_config.cpp
    src/audio/pitch_detect.cpp
    src/audio/yin.cpp
)
target_include_directories(app_tests PRIVATE src/core src/audio)
target_link_libraries(app_tests PRIVATE Catch2::Catch2WithMain glad glfw)
# Catch2 uses __COUNTER__ internally which triggers -Wc2y-extensions under -Wpedantic
target_compile_options(app_tests PRIVATE ${COMMON_WARNINGS} -Wno-c2y-extensions)
if(UNIX AND NOT APPLE)
    target_link_libraries(app_tests PRIVATE OpenGL::GL)
endif()
if(UNIX)
    target_link_libraries(app_tests PRIVATE Threads::Threads dl m)
endif()
add_dependencies(app_tests copy_shaders)

include(CTest)
include(Catch)
catch_discover_tests(app_tests)
