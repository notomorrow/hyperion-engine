/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/Handle.hpp>

#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/utilities/TypeId.hpp>

namespace hyperion {

class HypClass;

class UIStage;
class UIObject;

class EditorObjectPropertiesBase
{
protected:
    EditorObjectPropertiesBase(TypeId typeId);

public:
    EditorObjectPropertiesBase(const EditorObjectPropertiesBase&) = delete;
    EditorObjectPropertiesBase& operator=(const EditorObjectPropertiesBase&) = delete;
    EditorObjectPropertiesBase(EditorObjectPropertiesBase&&) noexcept = delete;
    EditorObjectPropertiesBase& operator=(EditorObjectPropertiesBase&&) noexcept = delete;
    virtual ~EditorObjectPropertiesBase() = default;

    const HypClass* GetClass() const;

    virtual Handle<UIObject> CreateUIObject(UIObject* parent) const = 0;

private:
    TypeId m_typeId;
};

template <class T>
class EditorObjectProperties;

template <>
class EditorObjectProperties<Vec2f> : public EditorObjectPropertiesBase
{
public:
    EditorObjectProperties()
        : EditorObjectPropertiesBase(TypeId::ForType<Vec2f>())
    {
    }

    Handle<UIObject> CreateUIObject(UIObject* parent) const override;
};

} // namespace hyperion
