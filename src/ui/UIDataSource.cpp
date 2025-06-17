/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIDataSource.hpp>

namespace hyperion {

#pragma region UIElementFactoryRegistry

UIElementFactoryRegistry& UIElementFactoryRegistry::GetInstance()
{
    static UIElementFactoryRegistry instance {};

    return instance;
}

UIElementFactoryBase* UIElementFactoryRegistry::GetFactory(TypeId typeId)
{
    auto it = m_elementFactories.Find(typeId);

    if (it == m_elementFactories.End())
    {
        return nullptr;
    }

    FactoryInstance& factoryInstance = it->second;

    if (!factoryInstance.factoryInstance)
    {
        factoryInstance.factoryInstance = factoryInstance.makeFactoryFunction();
    }

    return factoryInstance.factoryInstance.Get();
}

void UIElementFactoryRegistry::RegisterFactory(TypeId typeId, Handle<UIElementFactoryBase> (*makeFactoryFunction)(void))
{
    m_elementFactories.Set(typeId, FactoryInstance { makeFactoryFunction, nullptr });
}

#pragma endregion UIElementFactoryRegistry

#pragma region UIElementFactoryRegistrationBase

UIElementFactoryRegistrationBase::UIElementFactoryRegistrationBase(TypeId typeId, Handle<UIElementFactoryBase> (*makeFactoryFunction)(void))
    : m_makeFactoryFunction(makeFactoryFunction)
{
    UIElementFactoryRegistry::GetInstance().RegisterFactory(typeId, makeFactoryFunction);
}

UIElementFactoryRegistrationBase::~UIElementFactoryRegistrationBase()
{
}

#pragma endregion UIElementFactoryRegistrationBase

} // namespace hyperion