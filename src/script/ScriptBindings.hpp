#ifndef SCRIPT_BINDINGS_HPP
#define SCRIPT_BINDINGS_HPP

#include <script/ScriptApi.hpp>

namespace hyperion {

namespace scriptapi2 {
class Context;
} // namespace scriptapi2

class ScriptBindings
{
public:
    static APIInstance::ClassBindings classBindings;

    static void DeclareAll(APIInstance& apiInstance);
};

} // namespace hyperion

#endif
