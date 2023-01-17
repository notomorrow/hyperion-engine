#include <script/compiler/Configuration.hpp>

namespace hyperion::compiler {

const SizeType Config::max_data_members = 255;
const char *Config::global_module_name = "global";
bool Config::use_static_objects = true;
bool Config::cull_unused_objects = false;

} // namespace hyperion::compiler
