#pragma once

// a build that compiles but fails to load: the Game class it names doesn't exist
#define NATIVE_TEST_BUILD 1

namespace Hyperion {

static constexpr const char* NativeTestGameClassName = "NativeTestMissingGame";

} // namespace Hyperion
