#ifndef HYPERION_RANGE_H
#define HYPERION_RANGE_H

#include <math/math_util.h>

namespace hyperion {

template <class T>
class Range {
public:
    using iterator = T;

    Range() : m_start{}, m_end{} {}
    Range(const T &start, const T &end) : m_start(start), m_end(end) {}
    Range(const Range &other) = default;
    Range &operator=(const Range &other) = default;
    Range(Range &&other) = default;
    Range &operator=(Range &&other) = default;
    ~Range() = default;

    inline explicit operator bool() const { return bool(GetDistance() > 0); }

    inline const T &GetStart() const     { return m_start; }
    inline void SetStart(const T &start) { m_start = start; }
    inline const T &GetEnd()   const     { return m_end; }
    inline void SetEnd(const T &end)     { m_end = end; }
    inline T GetDistance() const         { return m_end - m_start; }

    Range operator|(const Range &other) const
    {
        return {MathUtil::Min(m_start, other.m_start), MathUtil::Max(m_end, other.m_end)};
    }

    Range &operator|=(const Range &other)
    {
        m_start = MathUtil::Min(m_start, other.m_start);
        m_end   = MathUtil::Max(m_end, other.m_end);

        return *this;
    }

    bool operator<(const Range &other) const { return GetDistance() < other.GetDistance(); }
    bool operator>(const Range &other) const { return GetDistance() > other.GetDistance(); }
    bool operator==(const Range &other) const
    {
        if (this == &other) {
            return true;
        }

        return m_start == other.m_start
            && m_end == other.m_end;
    }

    Range Without(T value, T step = 1) const
    {
        return {
            MathUtil::Max(m_start, value + step),
            MathUtil::Min(m_end, value - step)
        };
    }

    iterator begin() const { return m_start; }
    iterator end() const { return m_end; }

private:
    T m_start, m_end;
};

} // namespace hyperion

#endif