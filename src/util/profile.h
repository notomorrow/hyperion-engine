#ifndef HYPERION_PROFILE_H
#define HYPERION_PROFILE_H

#include <type_traits>

namespace hyperion {

class Profile {
public:
    using LambdaFunction = std::add_pointer_t<void()>;
    
    Profile(LambdaFunction lambda)
        : m_lambda(lambda),
          m_result(0.0),
          m_iteration(0)
    {
    }

    Profile(const Profile &other) = delete;
    Profile &operator=(const Profile &other) = delete;

    ~Profile() = default;
    
    Profile &Run(size_t num_iterations = 100, size_t runs_per_iteration = 100)
    {
        using namespace std;
        using namespace std::chrono;

        double *times = new double[num_iterations];

        for (size_t i = 0; i < num_iterations; i++) {
            auto start = high_resolution_clock::now();

            for (int j = 0; j < runs_per_iteration; j++) {
                m_lambda();
            }

            auto stop = high_resolution_clock::now();

            double iter_time = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(stop - start).count();

            times[i] = iter_time;
        }

        double result = 0.0;

        for (size_t i = 0; i < num_iterations; i++) {
            result += times[i];
        }

        result /= double(num_iterations);
        
        m_result = (m_result + result) * (1.0 / (m_iteration + 1));
        m_iteration++;

        delete[] times;

        return *this;
    }

    double GetResult() const { return m_result; }

    Profile &Reset()
    {
        m_result = 0.0;
        m_iteration = 0;

        return *this;
    }

private:
    LambdaFunction m_lambda;
    double m_result;
    size_t m_iteration;
};

} // namespace hyperion

#endif