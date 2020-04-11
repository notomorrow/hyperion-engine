#ifndef GAMMA_CORRECTION_FILTER_H
#define GAMMA_CORRECTION_FILTER_H

#include "../post_filter.h"

namespace apex {

class GammaCorrectionFilter : public PostFilter {
public:
  GammaCorrectionFilter();

  virtual void SetUniforms(Camera *cam) override;
};

} // namespace apex

#endif
