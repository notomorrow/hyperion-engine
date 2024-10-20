/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_FIELD_HPP
#define HYPERION_CORE_HYP_FIELD_HPP

#include <core/object/HypData.hpp>
#include <core/object/HypClassAttribute.hpp>
#include <core/object/HypMemberFwd.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/functional/Proc.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Span.hpp>

#include <core/containers/String.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/AnyRef.hpp>

#include <asset/serialization/fbom/FBOMData.hpp>

#include <Types.hpp>

namespace hyperion {

class HypClass;

struct HypField : public IHypMember
{
    Name                                            name;
    TypeID                                          type_id;
    TypeID                                          target_type_id;
    uint32                                          offset;
    uint32                                          size;

    HypClassAttributeSet                            attributes;

    Proc<HypData, const HypData &>                  get_proc;
    Proc<void, HypData &, const HypData &>          set_proc;

    Proc<fbom::FBOMData, const HypData &>           serialize_proc;
    Proc<void, HypData &, const fbom::FBOMData &>   deserialize_proc;

    HypField(Span<HypClassAttribute> attributes = { })
        : name(Name::Invalid()),
          type_id(TypeID::Void()),
          target_type_id(TypeID::Void()),
          offset(0),
          size(0),
          attributes(attributes)
    {
    }

    template <class ThisType, class FieldType>
    HypField(Name name, FieldType ThisType::*member, uint32 offset, Span<HypClassAttribute> attributes = { })
        : name(name),
          type_id(TypeID::ForType<FieldType>()),
          target_type_id(TypeID::ForType<ThisType>()),
          offset(offset),
          size(sizeof(FieldType)),
          attributes(attributes)
    {
        get_proc = [member](const HypData &target_data) -> HypData
        {
            if constexpr (!std::is_copy_assignable_v<NormalizedType<FieldType>> && !std::is_array_v<NormalizedType<FieldType>>) {
                HYP_FAIL("Cannot assign non-copy-assignable field");
            } else {
                ConstAnyRef target_ref = target_data.ToRef();

                AssertThrow(target_ref.HasValue());
                AssertThrowMsg(target_ref.Is<ThisType>(), "Invalid target type: Expected %s (TypeID: %u), but got TypeID: %u",
                    TypeName<ThisType>().Data(), TypeID::ForType<ThisType>().Value(), target_ref.GetTypeID().Value());

                return HypData(static_cast<const ThisType *>(target_ref.GetPointer())->*member);
            }
        };

        set_proc = [member](HypData &target_data, const HypData &data) -> void
        {
            if constexpr (!std::is_copy_assignable_v<NormalizedType<FieldType>> && !std::is_array_v<NormalizedType<FieldType>>) {
                HYP_FAIL("Cannot deserialize non-copy-assignable field");
            } else {
                AnyRef target_ref = target_data.ToRef();

                AssertThrow(target_ref.HasValue());
                AssertThrowMsg(target_ref.Is<ThisType>(), "Invalid target type: Expected %s (TypeID: %u), but got TypeID: %u",
                    TypeName<ThisType>().Data(), TypeID::ForType<ThisType>().Value(), target_ref.GetTypeID().Value());

                ThisType *target = static_cast<ThisType *>(target_ref.GetPointer());

                if constexpr (std::is_array_v<NormalizedType<FieldType>>) {
                    using InnerType = std::remove_extent_t<NormalizedType<FieldType>>;

                    if (data.IsValid()) {
                        auto &array_value = data.Get<NormalizedType<FieldType>>();

                        for (SizeType i = 0; i < array_value.Size(); i++) {
                            (target->*member)[i] = array_value[i];
                        }
                    } else {
                        for (SizeType i = 0; i < std::extent_v<NormalizedType<FieldType>>; i++) {
                            (target->*member)[i] = NormalizedType<InnerType> { };
                        }
                    }
                } else {
                    if (data.IsValid()) {
                        target->*member = data.Get<NormalizedType<FieldType>>();
                    } else {
                        target->*member = NormalizedType<FieldType> { };
                    }
                }
            }
        };

        if (this->attributes["serialize"]) {
            serialize_proc = [member](const HypData &target_data) -> fbom::FBOMData
            {
                if constexpr (!std::is_copy_assignable_v<NormalizedType<FieldType>> && !std::is_array_v<NormalizedType<FieldType>>) {
                    HYP_FAIL("Cannot serialize non-copy-assignable field");
                } else {
                    ConstAnyRef target_ref = target_data.ToRef();

                    AssertThrow(target_ref.HasValue());
                    AssertThrowMsg(target_ref.Is<ThisType>(), "Invalid target type: Expected %s (TypeID: %u), but got TypeID: %u",
                        TypeName<ThisType>().Data(), TypeID::ForType<ThisType>().Value(), target_ref.GetTypeID().Value());

                    fbom::FBOMData out;

                    if (fbom::FBOMResult err = HypDataHelper<NormalizedType<FieldType>>::Serialize(HypData(static_cast<const ThisType *>(target_ref.GetPointer())->*member), out)) {
                        HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                    }

                    return out;
                }
            };

            deserialize_proc = [member](HypData &target_data, const fbom::FBOMData &data) -> void
            {
                if constexpr (!std::is_copy_assignable_v<NormalizedType<FieldType>> && !std::is_array_v<NormalizedType<FieldType>>) {
                    HYP_FAIL("Cannot deserialize non-copy-assignable field");
                } else {
                    AnyRef target_ref = target_data.ToRef();

                    AssertThrow(target_ref.HasValue());
                    AssertThrowMsg(target_ref.Is<ThisType>(), "Invalid target type: Expected %s (TypeID: %u), but got TypeID: %u",
                        TypeName<ThisType>().Data(), TypeID::ForType<ThisType>().Value(), target_ref.GetTypeID().Value());

                    HypData value;
        
                    if (fbom::FBOMResult err = HypDataHelper<NormalizedType<FieldType>>::Deserialize(data, value)) {
                        HYP_FAIL("Failed to deserialize data: %s", err.message.Data());
                    }

                    ThisType *target = static_cast<ThisType *>(target_ref.GetPointer());

                    if constexpr (std::is_array_v<NormalizedType<FieldType>>) {
                        using InnerType = std::remove_extent_t<NormalizedType<FieldType>>;

                        if (value.IsValid()) {
                            auto &array_value = value.Get<NormalizedType<FieldType>>();

                            for (SizeType i = 0; i < array_value.Size(); i++) {
                                (target->*member)[i] = array_value[i];
                            }
                        } else {
                            for (SizeType i = 0; i < std::extent_v<NormalizedType<FieldType>>; i++) {
                                (target->*member)[i] = NormalizedType<InnerType> { };
                            }
                        }
                    } else {
                        if (value.IsValid()) {
                            target->*member = value.Get<NormalizedType<FieldType>>();
                        } else {
                            target->*member = NormalizedType<FieldType> { };
                        }
                    }
                }
            };
        }
    }

    HypField(const HypField &other)                 = delete;
    HypField &operator=(const HypField &other)      = delete;
    HypField(HypField &&other) noexcept             = default;
    HypField &operator=(HypField &&other) noexcept  = default;
    virtual ~HypField() override                    = default;

    virtual HypMemberType GetMemberType() const override
    {
        return HypMemberType::TYPE_FIELD;
    }

    virtual Name GetName() const override
    {
        return name;
    }

    virtual TypeID GetTypeID() const override
    {
        return type_id;
    }

    virtual const HypClassAttributeValue &GetAttribute(ANSIStringView key) const override
    {
        return attributes.Get(key);
    }

    virtual const HypClassAttributeValue &GetAttribute(ANSIStringView key, const HypClassAttributeValue &default_value) const override
    {
        return attributes.Get(key, default_value);
    }

    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return name.IsValid()
            && type_id != TypeID::Void()
            && size != 0;
    }

    HYP_FORCE_INLINE bool CanSerialize() const
        { return IsValid() && serialize_proc.IsValid(); }

    HYP_FORCE_INLINE fbom::FBOMData Serialize(const HypData &target_data) const
    {
        AssertThrow(CanSerialize());

        return serialize_proc(target_data);
    }

    HYP_FORCE_INLINE bool CanDeserialize() const
        { return IsValid() && deserialize_proc.IsValid(); }

    HYP_FORCE_INLINE void Deserialize(HypData &target_data, const fbom::FBOMData &data) const
    {
        AssertThrow(CanDeserialize());

        deserialize_proc(target_data, data);
    }

    HYP_FORCE_INLINE HypData Get(const HypData &target_data) const
    {
        return get_proc(target_data);
    }

    HYP_FORCE_INLINE void Set(HypData &target_data, const HypData &data) const
    {
        return set_proc(target_data, data);
    }
};

} // namespace hyperion

#endif