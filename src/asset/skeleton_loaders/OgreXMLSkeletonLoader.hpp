#ifndef HYPERION_V2_OGRE_XML_SKELETON_LOADER_H
#define HYPERION_V2_OGRE_XML_SKELETON_LOADER_H

#include <asset/LoaderObject.hpp>
#include <asset/Loader.hpp>
#include <animation/Skeleton.hpp>
#include <core/lib/String.hpp>

#include <math/Quaternion.hpp>

namespace hyperion::v2 {

template <>
struct LoaderObject<Skeleton, LoaderFormat::OGRE_XML_SKELETON>
{
    class Loader : public LoaderBase<Skeleton, LoaderFormat::OGRE_XML_SKELETON>
    {
        static LoaderResult LoadFn(LoaderState *state, Object &);
        static std::unique_ptr<Skeleton> BuildFn(Engine *engine, const Object &);

    public:
        Loader()
            : LoaderBase({
                .load_fn = LoadFn,
                .build_fn = BuildFn
            })
        {
        }
    };

    struct BoneData
    {
        String name;
        UInt id;

        String parent_name;
        Vector3 binding_translation;
        Quaternion binding_rotation;
    };

    struct KeyframeData
    {
        float time;
        Vector3 translation;
        Quaternion rotation;
    };

    struct AnimationTrackData
    {
        String bone_name;
        std::vector<KeyframeData> keyframes;
    };

    struct AnimationData
    {
        String name;
        std::vector<AnimationTrackData> tracks;
    };

    std::vector<BoneData> bones;
    std::vector<AnimationData> animations;
};

} // namespace hyperion::v2

#endif