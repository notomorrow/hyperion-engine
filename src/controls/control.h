#ifndef CONTROL_H
#define CONTROL_H

#include "../asset/fbom/fbom.h"

namespace hyperion {

class Node;

class Control : public fbom::FBOMLoadable {
    friend class Node;
public:
    Control(const fbom::FBOMType &loadable_type, const double tps = 30.0);
    Control(const Control &other) = delete;
    Control &operator=(const Control &other) = delete;
    virtual ~Control() = default;

    virtual void OnAdded() = 0;
    virtual void OnRemoved() = 0;
    virtual void OnFirstRun(double dt);

    virtual std::shared_ptr<Loadable> Clone() override;

protected:
    virtual std::shared_ptr<Control> CloneImpl() = 0;

private:
    const double tps;
    double tick;
    bool m_first_run;
};


} // namespace hyperion

#endif