#include <scene/terrain/TerrainErosion.hpp>

#include <math/MathUtil.hpp>

namespace hyperion::v2 {

const FixedArray<Pair<Int, Int>, 8> TerrainErosion::offsets {
    Pair { 1, 0 },
    Pair { 1, 1 },
    Pair { 1, -1 },
    Pair { 0, 1 },
    Pair { 0, -1 },
    Pair { -1, 0 },
    Pair { -1, 1 },
    Pair { -1, -1 }
};

void TerrainErosion::Erode(TerrainHeightData &height_data)
{
    for (UInt iteration = 0; iteration < num_iterations; iteration++) {
        for (int z = 1; z < height_data.patch_info.extent.depth - 2; z++) {
            for (int x = 1; x < height_data.patch_info.extent.width - 2; x++) {
                auto &height_info = height_data.heights[height_data.GetHeightIndex(x, z)];
                height_info.displacement = 0.0f;

                for (const auto &offset : offsets) {
                    const auto &neighbor_height_info = height_data.heights[height_data.GetHeightIndex(x + offset.first, z + offset.second)];

                    height_info.displacement += MathUtil::Max(height_info.height - neighbor_height_info.height, 0.0f);
                }

                if (height_info.displacement != 0.0f) {
                    Float water = height_info.water * evaporation;
                    Float staying_water = (water * 0.0002f) / (height_info.displacement * erosion_scale + 1);
                    water -= staying_water;

                    for (const auto &offset : offsets) {
                        auto &neighbor_height_info = height_data.heights[height_data.GetHeightIndex(x + offset.first, z + offset.second)];

                        neighbor_height_info.new_water += MathUtil::Max(height_info.height - neighbor_height_info.height, 0.0f) / height_info.displacement * water;
                    }

                    height_info.water = staying_water + 1.0f;
                }

            }
        }

        for (int z = 1; z < height_data.patch_info.extent.depth - 2; z++) {
            for (int x = 1; x < height_data.patch_info.extent.width - 2; x++) {
                auto &height_info = height_data.heights[height_data.GetHeightIndex(x, z)];

                height_info.water += height_info.new_water;
                height_info.new_water = 0.0f;

                const auto old_height = height_info.height;
                height_info.height += (-(height_info.displacement - (0.005f / erosion_scale)) * height_info.water) * erosion + height_info.water * deposition;
                height_info.erosion = old_height - height_info.height;

                if (old_height < height_info.height) {
                    height_info.water = MathUtil::Max(height_info.water - (height_info.height - old_height) * 1000.0f, 0.0f);
                }
            }
        }
    }
}

} // namespace hyperion::v2