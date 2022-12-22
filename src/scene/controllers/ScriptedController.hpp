#ifndef HYPERION_V2_SCRIPTED_CONTROLLER_H
#define HYPERION_V2_SCRIPTED_CONTROLLER_H

#include <scene/Controller.hpp>
#include <script/Script.hpp>
#include <core/ID.hpp>

namespace hyperion::v2 {

class ScriptedController : public Controller
{
public:
    static constexpr const char *controller_name = "ScriptedController";

    ScriptedController();
    ScriptedController(Handle<Script> &&script);
    virtual ~ScriptedController() override = default;
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

    virtual void Serialize(fbom::FBOMObject &out) const override;
    virtual fbom::FBOMResult Deserialize(const fbom::FBOMObject &in) override;
};

} // namespace hyperion::v2

#endif
