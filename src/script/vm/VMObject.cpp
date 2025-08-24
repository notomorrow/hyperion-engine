#include <script/vm/VMObject.hpp>
#include <script/vm/HeapValue.hpp>
#include <script/Hasher.hpp>
#include <core/debug/Debug.hpp>
#include <core/math/MathUtil.hpp>
#include <core/Core.hpp>

#include <cmath>

namespace hyperion {
namespace vm {

const uint32 VMObject::PROTO_MEMBER_HASH = hashFnv1("$proto");
const uint32 VMObject::BASE_MEMBER_HASH = hashFnv1("base");

ObjectMap::ObjectBucket::ObjectBucket()
    : m_data(new Member*[DEFAULT_BUCKET_CAPACITY]),
      m_capacity(DEFAULT_BUCKET_CAPACITY),
      m_size(0)
{
}

ObjectMap::ObjectBucket::ObjectBucket(const ObjectBucket &other)
    : m_data(new Member*[other.m_capacity]),
      m_capacity(other.m_capacity),
      m_size(other.m_size)
{
    Memory::MemCpy(
        m_data,
        other.m_data,
        sizeof(Member *) * other.m_size
    );
}

ObjectMap::ObjectBucket::~ObjectBucket()
{
    delete[] m_data;
}

ObjectMap::ObjectBucket &ObjectMap::ObjectBucket::operator=(const ObjectBucket &other)
{
    if (m_capacity < other.m_capacity) {
        // capacity must be resized
        delete[] m_data;
        m_data = new Member*[other.m_capacity];
        m_capacity = other.m_capacity;
    }

    // copy members
    Memory::MemCpy(
        m_data,
        other.m_data,
        sizeof(Member *) * other.m_size
    );

    return *this;
}

void ObjectMap::ObjectBucket::Resize(SizeType capacity)
{
    if (m_capacity < capacity) {
        const SizeType newCapacity = COMPUTE_CAPACITY(capacity);
        // create new bucket and copy
        Member **newData = new Member*[newCapacity];
        Memory::MemCpy(
            newData,
            m_data,
            sizeof(Member *) * m_size
        );
        m_capacity = newCapacity;
        delete[] m_data;
        m_data = newData;
    }
}

void ObjectMap::ObjectBucket::Push(Member *member)
{
    if (m_size >= m_capacity) {
        Resize(COMPUTE_CAPACITY(MathUtil::Max(m_capacity, m_size) + 1));
    }

    m_data[m_size++] = member;
}

bool ObjectMap::ObjectBucket::Lookup(uint32 hash, Member **out)
{
    for (SizeType i = 0; i < m_size; i++) {
        if (m_data[i]->hash == hash) {
            *out = m_data[i];

            return true;
        }
    }

    return false;
}


ObjectMap::ObjectMap(SizeType size)
    : m_size(size)
{
    
    m_buckets = new ObjectMap::ObjectBucket[m_size];
}

ObjectMap::ObjectMap(const ObjectMap &other)
    : m_size(other.m_size)
{
    m_buckets = new ObjectMap::ObjectBucket[m_size];

    for (SizeType i = 0; i < m_size; i++) {
        m_buckets[i] = other.m_buckets[i];
    }
}

ObjectMap::~ObjectMap()
{
    delete[] m_buckets;
}

ObjectMap &ObjectMap::operator=(const ObjectMap &other)
{
    if (m_size != other.m_size) {
        // delete buckets and recreate with new size
        delete[] m_buckets;
        m_buckets = new ObjectMap::ObjectBucket[other.m_size];
    }

    m_size = other.m_size;

    for (SizeType i = 0; i < m_size; i++) {
        m_buckets[i] = other.m_buckets[i];
    }

    return *this;
}

void ObjectMap::Push(uint32 hash, Member *member)
{
    Assert(m_size != 0);
    m_buckets[hash % m_size].Push(member);
}

Member *ObjectMap::Get(uint32 hash)
{
    if (m_size == 0) {
        return nullptr;
    }

    Member *res = nullptr;
    m_buckets[hash % m_size].Lookup(hash, &res);

    return res;
}

VMObject::VMObject(HeapValue *classPtr)
    : m_classPtr(classPtr)
{
    Assert(m_classPtr != nullptr);

    const VMObject *protoObj = m_classPtr->GetPointer<VMObject>();
    Assert(protoObj != nullptr);

    SizeType size = protoObj->GetSize();

    m_members = new Member[size];
    Memory::MemCpy(m_members, protoObj->GetMembers(), sizeof(Member) * size);

    m_objectMap = new ObjectMap(size);
    for (SizeType i = 0; i < size; i++) {
        m_objectMap->Push(m_members[i].hash, &m_members[i]);
    }
}

VMObject::VMObject(const Member *members, SizeType size, HeapValue *classPtr)
    : m_classPtr(classPtr)
{
    Assert(members != nullptr);

    m_members = new Member[size];
    Memory::MemCpy(m_members, members, sizeof(Member) * size);

    m_objectMap = new ObjectMap(size);
    for (SizeType i = 0; i < size; i++) {
        m_objectMap->Push(m_members[i].hash, &m_members[i]);
    }
}

VMObject::VMObject(const VMObject &other)
    : m_classPtr(other.m_classPtr)
{
    const SizeType size = other.GetSize();

    m_members = new Member[size];

    Assert(other.GetObjectMap() != nullptr);
    m_objectMap = new ObjectMap(*other.GetObjectMap());

    Memory::MemCpy(m_members, other.m_members, sizeof(Member) * size);

    for (SizeType i = 0; i < size; i++) {
        m_objectMap->Push(m_members[i].hash, &m_members[i]);
    }
}

VMObject::~VMObject()
{
    m_classPtr = nullptr;

    delete m_objectMap;
    m_objectMap = nullptr;

    delete[] m_members;
    m_members = nullptr;
}

Member *VMObject::LookupMemberFromHash(uint32 hash, bool deep) const
{
    if (Member *member = m_objectMap->Get(hash)) {
        return member;
    }

    if (deep && m_classPtr != nullptr) {
        if (VMObject *baseObject = m_classPtr->GetPointer<VMObject>()) {
            return baseObject->LookupMemberFromHash(hash);
        }
    }

    return nullptr;
}

void VMObject::SetMember(const char *name, const Value &value)
{
    const uint32 hash = hashFnv1(name);
    
    Member *member = LookupMemberFromHash(hash, /* deep */ false);

    if (member) {
        member->value = value;

        return;
    }

    const SizeType prevSize = m_objectMap->GetSize();
    const SizeType newSize = prevSize + 1;

    Member *newMembers = new Member[newSize];
    Memory::MemCpy(newMembers, m_members, sizeof(Member) * prevSize);

    Memory::StrCpy(newMembers[newSize - 1].name, name, sizeof(newMembers[newSize - 1].name));
    newMembers[newSize - 1].hash = hash;
    newMembers[newSize - 1].value = value;

    ObjectMap *newObjectMap = new ObjectMap(newSize);
    for (SizeType i = 0; i < newSize; i++) {
        newObjectMap->Push(newMembers[i].hash, &newMembers[i]);
    }

    newObjectMap->Push(hash, &newMembers[newSize - 1]);

    delete[] m_members;
    delete m_objectMap;

    m_members = newMembers;
    m_objectMap = newObjectMap;
}

void VMObject::GetRepresentation(
    std::stringstream &ss,
    bool addTypeName,
    int depth
) const
{
    if (depth == 0) {
        ss << static_cast<const void *>(this);

        return;
    }

    const SizeType size = GetSize();

    ss << "{";

    for (SizeType i = 0; i < size; i++) {
        vm::Member &mem = m_members[i];

        ss << mem.name << ": ";

        if (mem.value.m_type == Value::HEAP_POINTER &&
            mem.value.m_value.ptr != nullptr &&
            mem.value.m_value.ptr->GetRawPointer() == static_cast<const void *>(this))
        {
            mem.value.ToRepresentation(
                ss,
                addTypeName,
                0 // prevent circular reference causing infinite loop
            );
        } else {
            mem.value.ToRepresentation(
                ss,
                addTypeName,
                depth - 1
            );
        }

        if (i != size - 1) {
            ss << ", ";
        }
    }

    ss << "}";
}

HashCode VMObject::GetHashCode() const
{
    // Hash of memory address

    // @NOTE: If we ever implement a generational garbage collector,
    // we'll need to change this to something else. Perhaps
    // we could use a hash of an ID that is unique to each
    // object.

    return HashCode::GetHashCode(static_cast<const void *>(this));
}

} // namespace hyperion
} // namespace vm
