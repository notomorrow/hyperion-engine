/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef SIMPLEX_HPP
#define SIMPLEX_HPP

#include <util/random/OpenSimplexNoise.hpp>

#define OSN_OCTAVE_COUNT 8

struct osn_context;

namespace hyperion {
struct SimplexNoiseData {
    osn_context *octaves[OSN_OCTAVE_COUNT];
    double frequencies[OSN_OCTAVE_COUNT];
    double amplitudes[OSN_OCTAVE_COUNT];
};
} // namespace hyperion

#endif
