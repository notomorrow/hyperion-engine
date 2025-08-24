#include <script/compiler/Configuration.hpp>

namespace hyperion::compiler {

const SizeType Config::maxDataMembers = 255;
const char *Config::globalModuleName = "global";
bool Config::cullUnusedObjects = false;

} // namespace hyperion::compiler
