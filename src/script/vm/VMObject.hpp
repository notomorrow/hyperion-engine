#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <script/vm/Value.hpp>
#include <script/vm/VMTypeInfo.hpp>
#include <core/Types.hpp>
#include <core/HashCode.hpp>

#include <sstream>
#include <cstdint>
#include <cmath>

#define DEFAULT_BUCKET_CAPACITY 4
#define COMPUTE_CAPACITY(size) 1ull << (unsigned)std::ceil(std::log(size) / std::log(2.0))

namespace hyperion {
namespace vm {

struct Member
{
    char    name[256];
    uint32  hash;
    Value   value;
};

class ObjectMap
{
public:
    ObjectMap(SizeType size);
    ObjectMap(const ObjectMap &other);
    ~ObjectMap();

    ObjectMap &operator=(const ObjectMap &other);

    void Push(uint32 hash, Member *member);
    Member *Get(uint32 hash);

    SizeType GetSize() const
        { return m_size; }

private:
    struct ObjectBucket
    {
        Member      **m_data;
        SizeType    m_capacity;
        SizeType    m_size;

        ObjectBucket();
        ObjectBucket(const ObjectBucket &other);
        ~ObjectBucket();

        ObjectBucket &operator=(const ObjectBucket &other);

        void Resize(SizeType capacity);
        void Push(Member *member);
        bool Lookup(uint32 hash, Member **out);
    };

    ObjectBucket    *m_buckets;
    SizeType        m_size;
};

class VMObject
{
public:
    static const uint32 PROTO_MEMBER_HASH;
    static const uint32 BASE_MEMBER_HASH;

    // construct from prototype (holds pointer)
    VMObject(HeapValue *classPtr);
    VMObject(const Member *members, SizeType size, HeapValue *classPtr);
    VMObject(const VMObject &other);
    ~VMObject();

    VMObject &operator=(const VMObject &other);
    VMObject &operator=(VMObject &&other) noexcept;

    // compare by memory address
    bool operator==(const VMObject &other) const
        { return this == &other; }

    Member *LookupMemberFromHash(uint32 hash, bool deep = true) const;

    Member *GetMembers() const
        { return m_members; }

    Member &GetMember(SizeType index)
        { return m_members[index]; }

    const Member &GetMember(SizeType index) const
        { return m_members[index]; }

    ObjectMap *GetObjectMap() const
        { return m_objectMap; }

    void SetMember(const char *name, const Value &value);

    SizeType GetSize() const
        { return m_objectMap->GetSize(); }

    HeapValue *GetClassPointer() const
        { return m_classPtr; }

    bool LookupBasePointer(Value *out) const
    {
        Member *memberPtr = LookupMemberFromHash(BASE_MEMBER_HASH, false);

        if (memberPtr) {
            *out = memberPtr->value;

            return true;
        }

        return false;
    }
    
    void GetRepresentation(
        std::stringstream &ss,
        bool addTypeName = true,
        int depth = 3
    ) const;

    HashCode GetHashCode() const;

private:
    HeapValue   *m_classPtr;

    ObjectMap   *m_objectMap;
    Member      *m_members;
};

} // namespace vm
} // namespace hyperion

#endif
