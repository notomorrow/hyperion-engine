/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/BakeData.hpp>
#include <Baking/BakerMemory.hpp>

namespace Hyperion {

class Light;

namespace Baking {

template <>
class BakeData<Light> : public BakeDataBase
{
public:
    BakeData()
        : m_light(nullptr)
    {
    }

    BakeData(Span<const BakeEntity> bakeEntities, Light* light)
        : BakeDataBase(bakeEntities),
          m_light(light)
    {
    }

    BakeData(const BakeData& other) = default;
    BakeData(BakeData&& other) noexcept = default;

    BakeData& operator=(const BakeData& other) = default;
    BakeData& operator=(BakeData&& other) noexcept = default;

    ~BakeData() override = default;

    HYP_FORCE_INLINE Light* GetLight() const
    {
        return m_light;
    }

    virtual Result Build() override;

protected:
    Light* m_light;
};

} // namespace Baking
} // namespace Hyperion
