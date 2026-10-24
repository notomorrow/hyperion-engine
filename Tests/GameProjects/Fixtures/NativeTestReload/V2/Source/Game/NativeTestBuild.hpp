#pragma once

// the reload tests build the game more than once, replacing this file for each build (see Fixtures/NativeTestReload)
#define NATIVE_TEST_BUILD 2

namespace Hyperion {

static constexpr const char* NativeTestGameClassName = "NativeTestGame";

} // namespace Hyperion
