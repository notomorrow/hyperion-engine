#ifndef WORLEY_NOISE_H
#define WORLEY_NOISE_H

#include "../../math/vector3.h"

#include <vector>

#define OFFSET_BASIS 2166136261

#define FNV_PRIME 16777619

#define WORLEY_HASH(i, j, k) \
    ((((((OFFSET_BASIS ^ i) * FNV_PRIME) ^ j) * FNV_PRIME) ^ k) * FNV_PRIME)

#define WORLEY_LCG_RANDOM(last) \
    ((1103515245ULL * last + 12345ULL) % 0x100000000ULL)

namespace apex {
class WorleyNoise {
public:
    WorleyNoise(int seed);

    double Noise(double x, double y, double z);

private:
    int seed;

    double CombinerFunc1(double *data);
    double CombinerFunc2(double *data);
    double CombinerFunc3(double *data);

    double EuclidianDistance(const Vector3 &v1, const Vector3 &v2);
    double ManhattanDistance(const Vector3 &v1, const Vector3 &v2);
    double ChebyshevDistance(const Vector3 &v1, const Vector3 &v2);

    static unsigned char ProbLookup(unsigned int value);

    void Insert(std::vector<double> &data, double value);
};
}

#endif