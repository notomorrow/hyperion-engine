#include "FBXModelLoader.hpp"
#include <Engine.hpp>
#include <rendering/Mesh.hpp>
#include <scene/Entity.hpp>
#include <rendering/Material.hpp>
#include <util/fs/FsUtil.hpp>

#include <algorithm>
#include <stack>
#include <string>

#ifdef HYP_ZLIB
#include <zlib.h>
#endif

namespace hyperion::v2 {

constexpr char header_string[] = "Kaydara FBX Binary  ";
constexpr UInt8 header_bytes[] = { 0x1A, 0x00 };

using FBXPropertyValue = Variant<Int16, Int32, Int64, UInt32, Float, Double, bool, String, ByteBuffer>;
using FBXNodeID = Int64;

struct FBXProperty
{
    static const FBXProperty empty;

    FBXPropertyValue value;
    Array<FBXPropertyValue> array_elements;

    explicit operator bool() const
    {
        return value.IsValid() || array_elements.Any([](const FBXPropertyValue &item) { return item.IsValid(); });
    }
};

const FBXProperty FBXProperty::empty = { };

struct FBXNode
{
    static const FBXNode empty;

    String name;
    Array<FBXProperty> properties;
    Array<UniquePtr<FBXNode>> children;

    const FBXProperty &GetProperty(UInt index) const
    {
        if (index >= properties.Size()) {
            return FBXProperty::empty;
        }

        return properties[index];
    }

    template <class T>
    bool GetFBXPropertyValue(UInt index, T &out) const
    {
        out = { };

        if (const FBXProperty &property = GetProperty(index)) {
            if (const T *ptr = property.value.TryGet<T>()) {
                out = *ptr;

                return true;
            }
        }

        return false;
    }

    const FBXNode &operator[](const String &child_name) const
    {
        for (auto &child : children) {
            if (child == nullptr) {
                continue;
            }

            if (child->name == child_name) {
                return *child.Get();
            }
        }

        return empty;
    }

    explicit operator bool() const
        { return !Empty(); }

    bool Empty() const
    {
        return name.Empty()
            && properties.Empty()
            && children.Empty();
    }
};

const FBXNode FBXNode::empty = { };

static bool ReadMagic(ByteReader &reader)
{
    if (reader.Max() < sizeof(header_string) + sizeof(header_bytes)) {
        return false;
    }

    char read_magic[sizeof(header_string)] = { '\0' };

    reader.Read(read_magic, sizeof(header_string));

    if (std::strncmp(read_magic, header_string, sizeof(header_string)) != 0) {
        return false;
    }

    UInt8 bytes[sizeof(header_bytes)];
    reader.Read(bytes, sizeof(header_bytes));

    if (std::memcmp(bytes, header_bytes, sizeof(header_bytes)) != 0) {
        return false;
    }

    return true;
}

static LoaderResult ReadFBXPropertyValue(ByteReader &reader, UInt8 type, FBXPropertyValue &out_value)
{
    switch (type) {
    case 'R':
    case 'S':
        {
            UInt32 length;
            reader.Read(&length);

            ByteBuffer byte_buffer;
            reader.Read(length, byte_buffer);

            if (type == 'R') {
                out_value.Set(std::move(byte_buffer));
            } else {
                out_value.Set(String(byte_buffer));
            }

            return { };
        }
    case 'Y':
        {
            // Int16 (signed)
            Int16 i;
            reader.Read(&i);

            out_value.Set(i);

            return { };
        }
    case 'I':
        {
            // Int32 (signed)
            Int32 i;
            reader.Read(&i);

            out_value.Set(i);

            return { };
        }
    case 'L':
        {
            // Int64 (Signed)
            Int64 i;
            reader.Read(&i);

            out_value.Set(i);

            return { };
        }
    case 'C': // fallthrough
    case 'B':
        {
            UInt8 b;
            reader.Read(&b);

            out_value.Set(bool(b != 0));

            return { };
        }
    case 'F':
        {
            // Float
            Float f;
            reader.Read(&f);

            out_value.Set(f);

            return { };
        }
    case 'D':
        {
            // Double
            Double d;
            reader.Read(&d);

            out_value.Set(d);

            return { };
        }
    default:
        return { LoaderResult::Status::ERR, "Invalid property type" };
    }
}

static SizeType PrimitiveSize(UInt8 primitive_type)
{
    switch (primitive_type) {
    case 'Y':
        return 2;
    case 'C':
    case 'B': // fallthrough
        return 1;
    case 'I':
        return 4;
    case 'F':
        return 4;
    case 'D':
        return 8;
    case 'L':
        return 8;
    default:
        return 0;
    }
}

static LoaderResult ReadFBXProperty(ByteReader &reader, FBXProperty &out_property)
{
    UInt8 type;
    reader.Read(&type);

    if (type < 'Z') {
        LoaderResult result = ReadFBXPropertyValue(reader, type, out_property.value);

        if (!result) {
            return result;
        }
    } else {
        // array -- can be zlib compressed

        UInt32 num_elements;
        reader.Read(&num_elements);

        UInt32 encoding;
        reader.Read(&encoding);

        UInt32 length;
        reader.Read(&length);

        if (encoding != 0) {
            ByteBuffer compressed_buffer;
            reader.Read(length, compressed_buffer);

#ifdef HYP_ZLIB
            unsigned long compressed_size(length);
            unsigned long decompressed_size(PrimitiveSize(type - ('a' - 'A')) * num_elements);

            ByteBuffer decompressed_buffer(decompressed_size);

            const unsigned long compressed_size_before = compressed_size;
            const unsigned long decompressed_size_before = decompressed_size;

            if (uncompress2(decompressed_buffer.Data(), &decompressed_size, compressed_buffer.Data(), &compressed_size) != Z_OK) {
                return { LoaderResult::Status::ERR, "Failed to decompress data" };
            }

            if (compressed_size != compressed_size_before || decompressed_size != decompressed_size_before) {
                return { LoaderResult::Status::ERR, "Decompressed data was incorrect" };
            }

            MemoryByteReader memory_reader(&decompressed_buffer);

            for (UInt32 index = 0; index < num_elements; ++index) {
                FBXPropertyValue value;

                LoaderResult result = ReadFBXPropertyValue(memory_reader, type - ('a' - 'A'), value);
                
                if (!result) {
                    return result;
                }

                out_property.array_elements.PushBack(std::move(value));
            }
#endif
        } else {
            for (UInt32 index = 0; index < num_elements; ++index) {
                FBXPropertyValue value;
                
                LoaderResult result = ReadFBXPropertyValue(reader, type - ('a' - 'A'), value);
                
                if (!result) {
                    return result;
                }

                out_property.array_elements.PushBack(std::move(value));
            }
        }
    }

    return { };
}

static LoaderResult ReadFBXNode(ByteReader &reader, UniquePtr<FBXNode> &out_node)
{
    out_node.Reset(new FBXNode);

    const SizeType start_position = reader.Position();

    UInt32 end_position;
    reader.Read(&end_position);

    UInt32 num_properties;
    reader.Read(&num_properties);

    UInt32 property_list_length;
    reader.Read(&property_list_length);

    UInt8 name_length;
    reader.Read(&name_length);
    
    ByteBuffer name_buffer;
    reader.Read(name_length, name_buffer);

    out_node->name = String(name_buffer);

    for (UInt32 index = 0; index < num_properties; ++index) {
        FBXProperty property;

        LoaderResult result = ReadFBXProperty(reader, property);

        if (!result) {
            return result;
        }

        out_node->properties.PushBack(std::move(property));
    }

    while (reader.Position() < end_position) {
        UniquePtr<FBXNode> child_node;

        LoaderResult result = ReadFBXNode(reader, child_node);

        if (!result) {
            return result;
        }

        if (child_node != nullptr) {
            out_node->children.PushBack(std::move(child_node));
        }
    }
    
    return { };
}


enum FBXVertexMappingType
{
    INVALID_MAPPING_TYPE = 0,
    BY_POLYGON_VERTEX,
    BY_POLYGON,
    BY_VERTEX
};


static FBXVertexMappingType GetVertexMappingType(const FBXNode &node)
{
    FBXVertexMappingType mapping_type = INVALID_MAPPING_TYPE;

    if (node) {
        String mapping_type_str;

        if (node.GetFBXPropertyValue<String>(0, mapping_type_str)) {
            if (mapping_type_str == "ByPolygonVertex") {
                mapping_type = BY_POLYGON_VERTEX;
            } else if (mapping_type_str == "ByPolygon") {
                mapping_type = BY_POLYGON;
            } else if (mapping_type_str.StartsWith("ByVert")) {
                mapping_type = BY_VERTEX;
            }
        }
    }

    return mapping_type;
}

LoadedAsset FBXModelLoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);

    auto top = UniquePtr<Node>::Construct();
    
    // Include our root dir as part of the path
    const auto path = String(state.filepath.c_str());
    const auto current_dir = FileSystem::CurrentPath();
    const auto base_path = StringUtil::BasePath(path.Data());

    FileByteReader reader(FileSystem::Join(base_path, std::string(FilePath(path).Basename().Data())));

    if (reader.Eof()) {
        return { { LoaderResult::Status::ERR, "File not open" } };
    }

    if (!ReadMagic(reader)) {
        return { { LoaderResult::Status::ERR, "Invalid magic header" } };
    }

    UInt32 version;
    reader.Read(&version);

    FBXNode root;

    //Array<UniquePtr<FBXNode>> nodes;

    do {
        UniquePtr<FBXNode> node;

        LoaderResult result = ReadFBXNode(reader, node);

        if (!result) {
            return { result };
        }

        if (node == nullptr || node->Empty()) {
            break;
        }

        root.children.PushBack(std::move(node));
    } while (true);

    /*for (auto &node : nodes) {
        std::cout << "node : " << node->name << "\n";

        std::cout << "properties: \n";

        for (auto &prop : node->properties) {
            std::cout << "  " << prop.value.GetTypeID().value << "\n";
        }

        for (auto &ch : node->children) {
            std::cout << "  child: " << ch->name << "\n";
        }
    }*/

    struct FBXDefinitionProperty
    {
        UInt8 type;
        String name;
    };

    FlatMap<String, FBXDefinitionProperty> definitions;

    if (const auto &definitions_node = root["Definitions"]) {
        Int32 num_definitions = 0;

        if (const auto &count_node = definitions_node["Count"]) {
            count_node.GetFBXPropertyValue<Int32>(0, num_definitions);
        }

        for (UInt index = 0; index < UInt(num_definitions); ++index) {
            
        }

        for (auto &child : definitions_node.children) {

            if (child->name == "ObjectType") {
                String definition_name;
                child->GetFBXPropertyValue(0, definition_name);
            }
        }
    }

    struct FBXNodeMapping
    {
        FBXNode *node;

        Variant<
            Handle<Mesh>,
            Handle<Entity>
        > data;
    };

    struct FBXConnection
    {
        FBXNodeID left, right;
    };

    FlatMap<FBXNodeID, FBXNodeMapping> object_mapping;
    Array<FBXConnection> connections;

    if (const auto &connections_node = root["Connections"]) {
        for (auto &child : connections_node.children) {
            FBXConnection connection { };

            if (!child->GetFBXPropertyValue<FBXNodeID>(1, connection.left)) {
                DebugLog(LogType::Warn, "Invalid FBX Node connection, cannot get left ID value\n");
                continue;
            }
            
            if (!child->GetFBXPropertyValue<FBXNodeID>(2, connection.right)) {
                DebugLog(LogType::Warn, "Invalid FBX Node connection, cannot get right ID value\n");
                continue;
            }

            connections.PushBack(connection);
        }
    }

    if (const auto &objects_node = root["Objects"]) {
        for (auto &child : objects_node.children) {
            FBXNodeID node_id;
            child->GetFBXPropertyValue<FBXNodeID>(0, node_id);

            String node_name;
            child->GetFBXPropertyValue<String>(0, node_name);

            FBXNodeMapping mapping { child.Get() };

            if (child->name == "Geometry") {
                Array<Vector3> model_vertices;
                Array<Mesh::Index> model_indices;
                VertexAttributeSet attributes;

                Array<String> layer_node_names;

                if (const FBXNode &layer_node = (*child)["Layer"]) {
                    for (const auto &layer_node_child : layer_node.children) {
                        if (layer_node_child->name == "LayerElement") {
                            if (const auto &layer_node_type = (*layer_node_child)["Type"]) {
                                String layer_node_type_name;

                                if (layer_node_type.GetFBXPropertyValue(0, layer_node_type_name)) {
                                    layer_node_names.PushBack(std::move(layer_node_type_name));
                                }
                            }
                        }
                    }
                }

                if (const FBXNode &vertices_node = (*child)["Vertices"]) {
                    const FBXProperty &vertices_property = vertices_node.GetProperty(0);
                    const SizeType count = vertices_property.array_elements.Size();

                    if (count % 3 != 0) {
                        return { { LoaderResult::Status::ERR, "Not a valid vertices array" } };
                    }

                    const SizeType num_vertices = count / 3;

                    model_vertices.Resize(num_vertices);

                    for (SizeType index = 0; index < num_vertices; ++index) {
                        Vector3 position;

                        for (UInt sub_index = 0; sub_index < 3; ++sub_index) {
                            const auto &array_elements = vertices_property.array_elements[(index * 3) + SizeType(sub_index)];

                            union { Float float_value; Double double_value; };

                            if (array_elements.Get<Float>(&float_value)) {
                                position[sub_index] = float_value;
                            } else if (array_elements.Get<Double>(&double_value)) {
                                position[sub_index] = Float(double_value);
                            } else {
                                return { { LoaderResult::Status::ERR, "Invalid type for vertex position element -- must be float or double" } };
                            }
                        }

                        model_vertices[index] = position;
                    }
                }

                if (const FBXNode &indices_node = (*child)["PolygonVertexIndex"]) {
                    const FBXProperty &indices_property = indices_node.GetProperty(0);
                    const SizeType count = indices_property.array_elements.Size();

                    if (count % 3 != 0) {
                        return { { LoaderResult::Status::ERR, "Not a valid triangle mesh" } };
                    }

                    model_indices.Resize(count);

                    for (SizeType index = 0; index < count; ++index) {
                        Int32 i = 0;

                        if (!indices_property.array_elements[index].Get<Int32>(&i)) {
                            return { { LoaderResult::Status::ERR, "Invalid index value" } };
                        }

                        if (i < 0) {
                            i = (i * -1) - 1;
                        }

                        if (SizeType(i) >= model_vertices.Size()) {
                            return { { LoaderResult::Status::ERR, "Index out of range" } };
                        }
                        
                        model_indices[index] = Mesh::Index(i);
                    }
                }

                std::vector<Vertex> vertices;
                vertices.resize(model_indices.Size());

                for (SizeType index = 0; index < model_indices.Size(); ++index) {
                    vertices[index].SetPosition(model_vertices[model_indices[index]]);
                }
                
                for (const String &name : layer_node_names) {
                    if (name == "LayerElementUV") {
                        if (const auto &uv_node = (*child)[name]["UV"]) {
                            attributes |= VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0;

                            const SizeType count = uv_node.GetProperty(0).array_elements.Size();
                        }
                    } else if (name == "LayerElementNormal") {
                        const FBXVertexMappingType mapping_type = GetVertexMappingType((*child)[name]["MappingInformationType"]);
                        
                        if (const auto &normals_node = (*child)[name]["Normals"]) {
                            attributes |= VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL;

                            const FBXProperty &normals_property = normals_node.GetProperty(0);
                            const SizeType num_normals = normals_property.array_elements.Size() / 3;

                            if (num_normals % 3 != 0) {
                                return { { LoaderResult::Status::ERR, "Invalid normals count, must be triangulated" } };
                            }

                            for (SizeType triangle_index = 0; triangle_index < num_normals / 3; ++triangle_index) {
                                for (SizeType normal_index = 0; normal_index < 3; ++normal_index) {
                                    Vector3 normal;

                                    for (SizeType element_index = 0; element_index < 3; ++element_index) {
                                        const auto &array_elements = normals_property.array_elements[triangle_index * 9 + normal_index * 3 + element_index];

                                        union { Float float_value; Double double_value; };

                                        if (array_elements.Get<Float>(&float_value)) {
                                            normal[element_index] = float_value;
                                        } else if (array_elements.Get<Double>(&double_value)) {
                                            normal[element_index] = Float(double_value);
                                        } else {
                                            return { { LoaderResult::Status::ERR, "Invalid type for vertex position element -- must be float or double" } };
                                        }
                                    }
                                    
                                    vertices[triangle_index * 3 + normal_index].SetNormal(normal);
                                }
                            }
                        }
                    }
                }

                auto vertices_and_indices = Mesh::CalculateIndices(vertices);

                Handle<Mesh> mesh = CreateObject<Mesh>(
                    vertices_and_indices.first,
                    vertices_and_indices.second,
                    Topology::TRIANGLES,
                    renderer::static_mesh_vertex_attributes
                );

                mesh->SetName(node_name);
                mesh->CalculateTangents();

                mapping.data.Set(std::move(mesh));
            } else if (child->name == "Model") {
                Transform transform;

                for (const auto &model_child : child->children) {
                    if (model_child->name.StartsWith("Properties")) {
                        for (const auto &properties_child : model_child->children) {
                            String properties_child_name;
                            
                            if (!properties_child->GetFBXPropertyValue<String>(0, properties_child_name)) {
                                continue;
                            }

                            Double x, y, z;

                            if (properties_child_name == "Lcl Translation") {
                                properties_child->GetFBXPropertyValue<Double>(4, x);
                                properties_child->GetFBXPropertyValue<Double>(5, y);
                                properties_child->GetFBXPropertyValue<Double>(6, z);

                                transform.SetTranslation(Vector3(Float(x), Float(y), Float(z)));
                            } else if (properties_child_name == "Lcl Scaling") {
                                properties_child->GetFBXPropertyValue<Double>(4, x);
                                properties_child->GetFBXPropertyValue<Double>(5, y);
                                properties_child->GetFBXPropertyValue<Double>(6, z);

                                transform.SetScale(Vector3(Float(x), Float(y), Float(z)));
                            } else if (properties_child_name == "Lcl Rotation") {
                                properties_child->GetFBXPropertyValue<Double>(4, x);
                                properties_child->GetFBXPropertyValue<Double>(5, y);
                                properties_child->GetFBXPropertyValue<Double>(6, z);

                                transform.SetRotation(Quaternion(Vector3(Float(x), Float(y), Float(z))));
                            }
                        }
                    }
                }
                
                auto shader = Engine::Get()->shader_manager.GetShader(ShaderManager::Key::BASIC_FORWARD);
                auto material = Engine::Get()->GetMaterialCache().GetOrCreate({ .bucket = Bucket::BUCKET_OPAQUE });

                Handle<Entity> entity = CreateObject<Entity>();
                entity->SetName(node_name);
                entity->SetShader(shader);
                entity->SetMaterial(material);
                entity->SetTransform(transform);

                mapping.data.Set(std::move(entity));
            }

            object_mapping[node_id] = std::move(mapping);
        }
    }

    for (const FBXConnection &connection : connections) {
        if (connection.left == 0) {
            continue;
        }

        const auto left_it = object_mapping.Find(connection.left);

        if (left_it == object_mapping.End()) {
            DebugLog(LogType::Warn, "Invalid node connection (%u -> %u): Left ID not found in node map\n", connection.left, connection.right);

            continue;
        }

        if (!left_it->second.data.IsValid()) {
            DebugLog(LogType::Warn, "Invalid node connection (%u -> %u): Left node has no valid data\n", connection.left, connection.right);

            continue;
        }

        if (auto *left_entity = left_it->second.data.TryGet<Handle<Entity>>()) {
            if (connection.right == 0) {
                NodeProxy sub_node(new Node);
                sub_node.SetName((*left_entity)->GetName());
                sub_node.SetEntity(*left_entity);

                top->AddChild(sub_node);

                continue;
            }
        }

        const auto right_it = object_mapping.Find(connection.right);

        if (right_it == object_mapping.End()) {
            DebugLog(LogType::Warn, "Invalid node connection (%u -> %u): Right ID not found in node map\n", connection.left, connection.right);

            continue;
        }

        if (!right_it->second.data.IsValid()) {
            DebugLog(LogType::Warn, "Invalid node connection (%u -> %u): Right node has no valid data\n", connection.left, connection.right);

            continue;
        }

        if (auto *left_mesh = left_it->second.data.TryGet<Handle<Mesh>>()) {
            if (auto *right_entity = right_it->second.data.TryGet<Handle<Entity>>()) {
                (*right_entity)->SetMesh(*left_mesh);
                (*right_entity)->RebuildRenderableAttributes();

                AssertThrow((*right_entity)->IsRenderable());

                continue;
            }
        }

        DebugLog(LogType::Warn, "Invalid node connection (%u -> %u): Unable to process connection\n", connection.left, connection.right);
    }

    //if (const auto &objects_node = root["Objects"]) {
    //    objects_node.
  //  }
//

    top->UpdateWorldTransform();

    return { { LoaderResult::Status::OK }, top.Cast<void>() };
}

} // namespace hyperion::v2
