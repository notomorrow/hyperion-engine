#ifndef HYPERION_RANGE_H
#define HYPERION_RANGE_H

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

    inline const T &GetStart() const     { return m_start; }
    inline void SetStart(const T &start) { m_start = start; }
    inline const T &GetEnd()   const     { return m_end; }
    inline void SetEnd(const T &end)     { m_end = end; }
    inline T GetDistance() const         { return m_end - m_start; }

    iterator begin() const { return m_start; }
    iterator end() const { return m_end; }

private:
    T m_start, m_end;
};

} // namespace hyperion

#endif