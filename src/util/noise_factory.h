#ifndef NOISE_FACTORY_H
#define NOISE_FACTORY_H

#include "random/simplex.h"
#include "random/worley_noise.h"

#include <vector>
#include <map>
#include <utility>

namespace hyperion {

using Seed_t = unsigned int;

enum NoiseGenerationType {
    SIMPLEX_NOISE,
    WORLEY_NOISE
};

class NoiseGenerator {
    friend class NoiseFactory;
public:
    NoiseGenerator(NoiseGenerationType type, Seed_t seed);
    virtual ~NoiseGenerator() = default;

    inline Seed_t GetSeed() const { return m_seed; }

    virtual double GetNoise(double x, double z) const = 0;

protected:
    Seed_t m_seed;

private:
    NoiseGenerationType m_type;
};

class SimplexNoiseGenerator : public NoiseGenerator {
public:
    SimplexNoiseGenerator(Seed_t seed);
    virtual ~SimplexNoiseGenerator();

    virtual double GetNoise(double x, double z) const override;

private:
    SimplexNoiseData m_simplex_noise;
};

class WorleyNoiseGenerator : public NoiseGenerator {
public:
    WorleyNoiseGenerator(Seed_t seed);
    virtual ~WorleyNoiseGenerator();

    virtual double GetNoise(double x, double z) const override;

private:
    WorleyNoise *m_worley_noise;
};

struct NoiseGeneratorRefCounter {
    NoiseGenerator *noise;
    size_t uses;
};

class NoiseFactory {
public:
    static NoiseFactory *instance;
    static NoiseFactory *GetInstance();

    NoiseFactory() = default;
    ~NoiseFactory() = default;

    NoiseGenerator *Capture(NoiseGenerationType type, Seed_t seed);
    void Release(NoiseGenerator *noise);
    void Release(NoiseGenerationType type, Seed_t seed);

private:
    std::map<
        std::pair<NoiseGenerationType, Seed_t>,
        NoiseGeneratorRefCounter
    > m_noise_generators;
};

} // namespace hyperion

#endif
