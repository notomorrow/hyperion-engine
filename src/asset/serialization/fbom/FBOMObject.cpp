#include <asset/serialization/fbom/FBOMObject.hpp>

namespace hyperion::v2::fbom {

FBOMObject::FBOMObject()
    : m_object_type(FBOMUnset()),
      nodes(new FBOMNodeHolder())
{
}

FBOMObject::FBOMObject(const FBOMType &loader_type)
    : m_object_type(loader_type),
      nodes(new FBOMNodeHolder())
{
}

FBOMObject::FBOMObject(const FBOMObject &other)
    : m_object_type(other.m_object_type),
      nodes(new FBOMNodeHolder(*other.nodes)),
      properties(other.properties),
      deserialized(other.deserialized),
      m_external_info(other.m_external_info)
{
}

auto FBOMObject::operator=(const FBOMObject &other) -> FBOMObject&
{
    delete nodes;

    m_object_type = other.m_object_type;
    nodes = new FBOMNodeHolder(*other.nodes);
    properties = other.properties;
    deserialized = other.deserialized;
    m_external_info = other.m_external_info;

    return *this;
}

FBOMObject::FBOMObject(FBOMObject &&other) noexcept
    : m_object_type(std::move(other.m_object_type)),
      nodes(new FBOMNodeHolder(std::move(*other.nodes))),
      properties(std::move(other.properties)),
      deserialized(std::move(other.deserialized)),
      m_external_info(std::move(other.m_external_info))
{
}

auto FBOMObject::operator=(FBOMObject &&other) noexcept -> FBOMObject&
{
    delete nodes;

    m_object_type = std::move(other.m_object_type);
    nodes = new FBOMNodeHolder(std::move(*other.nodes));
    properties = std::move(other.properties);
    deserialized = std::move(other.deserialized);
    m_external_info = std::move(other.m_external_info);

    return *this;
}

FBOMObject::~FBOMObject()
{
    delete nodes;
}

const FBOMData &FBOMObject::GetProperty(const String &key) const
{
    auto it = properties.Find(key);

    if (it == properties.End()) {
        return FBOMData::UNSET;
    }

    return it->second;
}

void FBOMObject::SetProperty(const String &key, const FBOMData &data)
{
    properties.Set(key, data);
}

void FBOMObject::SetProperty(const String &key, const FBOMType &type, size_t size, const void *bytes)
{
    FBOMData data(type);
    data.SetBytes(size, reinterpret_cast<const unsigned char *>(bytes));

    SetProperty(key, data);
}

void FBOMObject::SetProperty(const String &key, const FBOMType &type, const void *bytes)
{
    AssertThrowMsg(!type.IsUnbouned(), "Cannot determine size of an unbounded type, please manually specify size");

    SetProperty(key, type, type.size, bytes);
}

void FBOMObject::AddChild(const FBOMObject &object, const String &external_object_key)
{
    FBOMObject _object(object);
    _object.SetExternalObjectInfo(FBOMExternalObjectInfo {
        .key = external_object_key
    });

    nodes->PushBack(std::move(_object));
}

void FBOMObject::AddChild(FBOMObject &&object, const String &external_object_key)
{
    FBOMObject _object(std::move(object));
    _object.SetExternalObjectInfo(FBOMExternalObjectInfo {
        .key = external_object_key
    });

    nodes->PushBack(std::move(_object));
}

HashCode FBOMObject::GetHashCode() const
{
    HashCode hc;

    hc.Add(m_object_type.GetHashCode());

    for (const auto &it : *nodes) {
        hc.Add(it.GetHashCode());
    }

    for (const auto &it : properties) {
        hc.Add(it.first.GetHashCode());
        hc.Add(it.second.GetHashCode());
    }

    return hc;
}

std::string FBOMObject::ToString() const
{
    std::stringstream ss;

    ss << m_object_type.ToString();
    ss << " { properties: { ";
    for (auto &prop : properties) {
        ss << prop.first;// << ": ";
        //ss << prop.second.ToString();
        ss << ", ";
    }

    ss << " }, nodes: [ ";

    ss << nodes->Size();

    ss << " ] ";

    ss << " } ";

    return ss.str();
}

} // namespace hyperion::v2::fbom