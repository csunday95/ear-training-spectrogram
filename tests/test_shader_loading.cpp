#include <catch2/catch_test_macros.hpp>

#include "gl_test_fixture.hpp"
#include "shader.hpp"

TEST_CASE("compute shader compiles", "[gl][shader]") {
    if (!is_gl_available()) SKIP("No GL context available");
    const auto prog = load_compute_program("shaders/compute/update.comp");
    REQUIRE(prog != 0);
    glDeleteProgram(prog);
}

TEST_CASE("render shaders compile", "[gl][shader]") {
    if (!is_gl_available()) SKIP("No GL context available");
    const auto prog = load_render_program("shaders/render/points.vert", "shaders/render/points.frag");
    REQUIRE(prog != 0);
    glDeleteProgram(prog);
}
