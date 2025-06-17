/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <scene/RenderProxyable.hpp>

namespace hyperion {

RenderProxyable::RenderProxyable()
    : m_renderProxyVersion(0)
{
}

void RenderProxyable::Init()
{
    SetReady(true);
}

void RenderProxyable::UpdateRenderProxy(IRenderProxy* proxy)
{
    // no default impl
}

void RenderProxyable::SetNeedsRenderProxyUpdate()
{
    ++m_renderProxyVersion;
}

} // namespace hyperion