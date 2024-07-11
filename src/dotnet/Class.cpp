/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/Object.hpp>

namespace hyperion::dotnet {

void Class::EnsureLoaded() const
{
    if (!m_class_holder || !m_class_holder->CheckAssemblyLoaded()) {
        HYP_THROW("Cannot use managed class: assembly has been unloaded");
    }
}

UniquePtr<Object> Class::NewObject()
{
    EnsureLoaded();

    AssertThrowMsg(m_new_object_fptr != nullptr, "New object function pointer not set for managed class %s", m_name.Data());

    ManagedObject managed_object = m_new_object_fptr();

    return UniquePtr<Object>::Construct(this, managed_object);
}

bool Class::HasParentClass(UTF8StringView parent_class_name) const
{
    const Class *parent_class = m_parent_class;

    while (parent_class) {
        if (parent_class->GetName() == parent_class_name) {
            return true;
        }

        parent_class = parent_class->GetParentClass();
    }

    return false;
}

bool Class::HasParentClass(const Class *parent_class) const
{
    const Class *current_parent_class = m_parent_class;

    while (current_parent_class) {
        if (current_parent_class == parent_class) {
            return true;
        }

        current_parent_class = current_parent_class->GetParentClass();
    }

    return false;
}

void *Class::InvokeStaticMethod(const ManagedMethod *method_ptr, void **args_vptr, void *return_value_vptr)
{
    EnsureLoaded();
    
    AssertThrowMsg(m_class_holder->GetInvokeMethodFunction() != nullptr, "Invoke method function pointer not set");

    return m_class_holder->GetInvokeMethodFunction()(method_ptr->guid, {}, args_vptr, return_value_vptr);
}

} // namespace hyperion::dotnet