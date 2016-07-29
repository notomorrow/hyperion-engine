#ifndef APX_LOADER_H
#define APX_LOADER_H

#include "../asset_loader.h"
#include "../../entity.h"
#include "../../rendering/mesh.h"
#include "../../animation/animation.h"
#include "../../animation/bone.h"
#include "../byte_reader.h"

#include <vector>
#include <memory>

namespace apex {

const std::string TOKEN_FACES = "faces";
const std::string TOKEN_FACE = "face";
const std::string TOKEN_VERTEX = "vertex";
const std::string TOKEN_VERTICES = "vertices";
const std::string TOKEN_POSITION = "position";
const std::string TOKEN_MESH = "mesh";
const std::string TOKEN_NODE = "node";
const std::string TOKEN_NAME = "name";
const std::string TOKEN_ID = "id";
const std::string TOKEN_PARENT = "parent";
const std::string TOKEN_GEOMETRY = "geom";
const std::string TOKEN_TEXCOORD0 = "uv0";
const std::string TOKEN_TEXCOORD1 = "uv1";
const std::string TOKEN_TEXCOORD2 = "uv2";
const std::string TOKEN_TEXCOORD3 = "uv3";
const std::string TOKEN_NORMAL = "normal";
const std::string TOKEN_BONEWEIGHT = "bone_weight";
const std::string TOKEN_BONEINDEX = "bone_index";
const std::string TOKEN_VERTEXINDEX = "vertex_index";
const std::string TOKEN_MATERIAL = "material";
const std::string TOKEN_MATERIAL_PROPERTY = "material_property";
const std::string TOKEN_MATERIAL_BUCKET = "bucket";
const std::string TOKEN_SHADER = "shader";
const std::string TOKEN_SHADERPROPERTIES = "shader_properties";
const std::string TOKEN_SHADERPROPERTY = "property";
const std::string TOKEN_CLASS = "class";
const std::string TOKEN_TYPE = "type";
const std::string TOKEN_TYPE_UNKNOWN = "unknown";
const std::string TOKEN_TYPE_STRING = "string";
const std::string TOKEN_TYPE_BOOLEAN = "boolean";
const std::string TOKEN_TYPE_FLOAT = "float";
const std::string TOKEN_TYPE_VECTOR2 = "vec2";
const std::string TOKEN_TYPE_VECTOR3 = "vec3";
const std::string TOKEN_TYPE_VECTOR4 = "vec4";
const std::string TOKEN_TYPE_INT = "int";
const std::string TOKEN_TYPE_COLOR = "color";
const std::string TOKEN_TYPE_TEXTURE = "texture";
const std::string TOKEN_VALUE = "value";
const std::string TOKEN_HAS_POSITIONS = "has_positions";
const std::string TOKEN_HAS_NORMALS = "has_normals";
const std::string TOKEN_HAS_TEXCOORDS0 = "has_texcoords0";
const std::string TOKEN_HAS_TEXCOORDS1 = "has_texcoords1";
const std::string TOKEN_HAS_BONES = "has_bones";
const std::string TOKEN_SKELETON = "skeleton";
const std::string TOKEN_BONE = "bone";
const std::string TOKEN_SKELETON_ASSIGN = "skeleton_assign";
const std::string TOKEN_ANIMATIONS = "animations";
const std::string TOKEN_ANIMATION = "animation";
const std::string TOKEN_ANIMATION_TRACK = "track";
const std::string TOKEN_KEYFRAME = "keyframe";
const std::string TOKEN_KEYFRAME_TRANSLATION = "keyframe_translation";
const std::string TOKEN_KEYFRAME_ROTATION = "keyframe_rotation";
const std::string TOKEN_TIME = "time";
const std::string TOKEN_BONE_ASSIGNS = "bone_assigns";
const std::string TOKEN_BONE_ASSIGN = "bone_assign";
const std::string TOKEN_BONE_BINDPOSITION = "bind_position";
const std::string TOKEN_BONE_BINDROTATION = "bind_rotation";
const std::string TOKEN_MODEL = "model";
const std::string TOKEN_TRANSLATION = "translation";
const std::string TOKEN_SCALE = "scale";
const std::string TOKEN_ROTATION = "rotation";
const std::string TOKEN_CONTROL = "control";

enum ApxCommand {
    CMD_FACES = 0x00,
    CMD_FACE,
    CMD_VERTEX,
    CMD_VERTICES,
    CMD_POSITION,
    CMD_MESH,
    CMD_NODE,
    CMD_END_NODE,
    CMD_NAME,
    CMD_ID,
    CMD_PARENT,
    CMD_GEOMETRY,
    CMD_END_GEOMETRY,
    CMD_TEXCOORD0,
    CMD_TEXCOORD1,
    CMD_TEXCOORD2,
    CMD_TEXCOORD3,
    CMD_NORMAL,
    CMD_BONEWEIGHT,
    CMD_BONEINDEX,
    CMD_VERTEXINDEX,
    CMD_MATERIAL,
    CMD_MATERIAL_PROPERTY,
    CMD_MATERIAL_BUCKET,
    CMD_SHADER,
    CMD_SHADERPROPERTIES,
    CMD_SHADERPROPERTY,
    CMD_CLASS,
    CMD_TYPE,
    CMD_TYPE_UNKNOWN,
    CMD_TYPE_STRING,
    CMD_TYPE_BOOLEAN,
    CMD_TYPE_FLOAT,
    CMD_TYPE_VECTOR2,
    CMD_TYPE_VECTOR3,
    CMD_TYPE_VECTOR4,
    CMD_TYPE_INT,
    CMD_TYPE_COLOR,
    CMD_TYPE_TEXTURE,
    CMD_VALUE,
    CMD_HAS_POSITIONS,
    CMD_HAS_NORMALS,
    CMD_HAS_TEXCOORDS0,
    CMD_HAS_TEXCOORDS1,
    CMD_HAS_BONES,
    CMD_SKELETON,
    CMD_BONE,
    CMD_SKELETON_ASSIGN,
    CMD_ANIMATIONS,
    CMD_ANIMATION,
    CMD_ANIMATION_TRACK,
    CMD_KEYFRAME,
    CMD_KEYFRAME_TRANSLATION,
    CMD_KEYFRAME_ROTATION,
    CMD_TIME,
    CMD_BONE_ASSIGNS,
    CMD_BONE_ASSIGN,
    CMD_BONE_BINDPOSITION,
    CMD_BONE_BINDROTATION,
    CMD_MODEL,
    CMD_END_MODEL,
    CMD_TRANSLATION,
    CMD_SCALE,
    CMD_ROTATION,
    CMD_CONTROL
};

struct ApxModel {
    ApxModel();

    void BuildModel();

    int n_faces_per_vertex;
    std::vector<std::shared_ptr<Entity>> entities;
    std::vector<std::shared_ptr<Mesh>> meshes;

    Entity *last_entity;

    std::vector<std::vector<Vector3>> positions;
    std::vector<std::vector<Vector3>> normals;
    std::vector<std::vector<Vector2>> texcoords0;
    std::vector<std::vector<Vector2>> texcoords1;
    std::vector<std::vector<Vertex>> vertices;
    std::vector<std::vector<size_t>> faces;

    bool has_animations;
    std::vector<Animation> animations;
    std::vector<std::shared_ptr<Bone>> bones;

    std::vector<Material> materials;
};

class ApxLoader : public AssetLoader {
public:
    std::shared_ptr<Loadable> LoadFromFile(const std::string &);

    void Read(ByteReader *reader, ApxCommand expected);
    void Handle(ApxModel &model, ByteReader *&reader, ApxCommand cmd);
};
}

#endif