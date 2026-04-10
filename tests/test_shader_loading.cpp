#include <catch2/catch_test_macros.hpp>

#include "gl_test_fixture.hpp"
#include "shader.hpp"

// Shader loading tests will be expanded in Phase 1 once the compute shaders exist.
// For now, just verify the test fixture and GL context setup works.

TEST_CASE("headless GL context initializes", "[gl]") {
    // is_gl_available() returns false if no display/GPU — tests will SKIP in CI if needed.
    if (!is_gl_available())
        SKIP("No GL context available");
    REQUIRE(gl_test_window() != nullptr);
}
