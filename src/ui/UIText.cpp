/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIText.hpp>
#include <ui/UIStage.hpp>

#include <rendering/RenderGlobalState.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RenderConfig.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/Texture.hpp>

#include <scene/camera/OrthoCamera.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/Quaternion.hpp>

#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>

#include <util/MeshBuilder.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

struct FontAtlasCharacterIterator
{
    Vec2f placement;
    Vec2f atlasPixelSize;
    Vec2f cellDimensions;

    utf::u32char charValue;

    Vec2i charOffset;

    Vec2f glyphDimensions;
    Vec2f glyphScaling;

    float bearingY;
    float charWidth;
};

template <class Callback>
static void ForEachCharacter(const FontAtlas& fontAtlas, const String& text, const Vec2i& parentBounds, float textSize, Array<Vec2f>* outCharacterPlacements, Callback&& callback)
{
    HYP_SCOPE;

    Vec2f placement = Vec2f::Zero();

    const SizeType length = text.Length();

    if (outCharacterPlacements)
    {
        outCharacterPlacements->Reserve(length + 1);
        outCharacterPlacements->PushBack(placement);
    }

    const Vec2f cellDimensions = Vec2f(fontAtlas.GetCellDimensions()) / 64.0f;
    AssertThrowMsg(cellDimensions.x * cellDimensions.y != 0.0f, "Cell dimensions are invalid");

    const float cellDimensionsRatio = cellDimensions.x / cellDimensions.y;

    const Handle<Texture>& mainTextureAtlas = fontAtlas.GetAtlasTextures().GetMainAtlas();
    AssertThrowMsg(mainTextureAtlas.IsValid(), "Main texture atlas is invalid");

    const Vec2f atlasPixelSize = Vec2f::One() / Vec2f(mainTextureAtlas->GetExtent().GetXY());

    struct
    {
        Array<FontAtlasCharacterIterator> chars;
    } currentWord;

    const auto iterateCurrentWord = [&currentWord, &callback]()
    {
        for (const FontAtlasCharacterIterator& characterIterator : currentWord.chars)
        {
            callback(characterIterator);
        }

        currentWord = {};
    };

    for (SizeType i = 0; i < length; i++)
    {
        UITextCharacter character {};

        const utf::u32char ch = text.GetChar(i);

        if (ch == utf::u32char(' '))
        {

            if (currentWord.chars.Any() && parentBounds.x != 0 && (currentWord.chars.Back().placement.x + currentWord.chars.Back().charWidth) * textSize >= float(parentBounds.x))
            {
                // newline
                placement.x = 0.0f;
                placement.y += cellDimensions.y;
            }
            else
            {
                // add room for space
                placement.x += cellDimensions.x * 0.5f;
            }

            if (outCharacterPlacements)
            {
                outCharacterPlacements->PushBack(placement);
            }

            iterateCurrentWord();

            continue;
        }

        if (ch == utf::u32char('\n'))
        {
            // reset placement, add room for newline
            placement.x = 0.0f;
            placement.y += cellDimensions.y;

            if (outCharacterPlacements)
            {
                outCharacterPlacements->PushBack(placement);
            }

            iterateCurrentWord();

            continue;
        }

        Optional<Glyph::Metrics> glyphMetrics = fontAtlas.GetGlyphMetrics(ch);

        if (!glyphMetrics.HasValue() || (glyphMetrics->metrics.width == 0 || glyphMetrics->metrics.height == 0))
        {
            if (outCharacterPlacements)
            {
                outCharacterPlacements->PushBack(placement);
            }

            continue;
        }

        FontAtlasCharacterIterator& characterIterator = currentWord.chars.EmplaceBack();
        characterIterator.charValue = ch;
        characterIterator.placement = placement;
        characterIterator.atlasPixelSize = atlasPixelSize;
        characterIterator.cellDimensions = cellDimensions;
        characterIterator.charOffset = glyphMetrics->imagePosition;
        characterIterator.glyphDimensions = Vec2f { float(glyphMetrics->metrics.width), float(glyphMetrics->metrics.height) } / 64.0f;
        characterIterator.glyphScaling = Vec2f(characterIterator.glyphDimensions) / (Vec2f(cellDimensions));
        characterIterator.bearingY = float(glyphMetrics->metrics.height - glyphMetrics->metrics.bearingY) / 64.0f;
        characterIterator.charWidth = float(glyphMetrics->metrics.advance / 64) / 64.0f;

        placement.x += characterIterator.charWidth;

        if (outCharacterPlacements)
        {
            outCharacterPlacements->PushBack(placement);
        }
    }

    iterateCurrentWord();
}

static BoundingBox CalculateTextAABB(const FontAtlas& fontAtlas, const String& text, const Vec2i& parentBounds, float textSize, bool includeBearing, Array<Vec2f>* outCharacterPlacements = nullptr)
{
    HYP_SCOPE;

    BoundingBox aabb;

    ForEachCharacter(fontAtlas, text, parentBounds, textSize, outCharacterPlacements, [includeBearing, &aabb](const FontAtlasCharacterIterator& iter)
        {
            BoundingBox characterAabb;

            if (includeBearing)
            {
                const float offsetY = (iter.cellDimensions.y - iter.glyphDimensions.y) + iter.bearingY;

                characterAabb = characterAabb
                                    .Union(Vec3f(iter.placement.x, iter.placement.y + offsetY, 0.0f))
                                    .Union(Vec3f(iter.placement.x + iter.glyphDimensions.x, iter.placement.y + offsetY + iter.cellDimensions.y, 0.0f));
            }
            else
            {
                characterAabb = characterAabb
                                    .Union(Vec3f(iter.placement.x, iter.placement.y, 0.0f))
                                    .Union(Vec3f(iter.placement.x + iter.glyphDimensions.x, iter.placement.y + iter.cellDimensions.y, 0.0f));
            }

            aabb = aabb.Union(characterAabb);
        });

    return aabb;
}

#pragma region UIText

UIText::UIText()
{
    m_textColor = Color(Vec4f::One());

    OnComputedVisibilityChange.Bind([this]()
                                  {
                                      if (GetComputedVisibility())
                                      {
                                          SetDeferredUpdate(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA, false);
                                      }

                                      return UIEventHandlerResult::OK;
                                  })
        .Detach();

    OnEnabled.Bind([this]()
                 {
                     UpdateMaterial(false);

                     return UIEventHandlerResult::OK;
                 })
        .Detach();

    OnDisabled.Bind([this]()
                  {
                      UpdateMaterial(false);

                      return UIEventHandlerResult::OK;
                  })
        .Detach();
}

UIText::~UIText()
{
}

void UIText::Init()
{
    HYP_SCOPE;

    UIObject::Init();
}

void UIText::SetText(const String& text)
{
    HYP_SCOPE;

    UIObject::SetText(text);

    if (!IsInitCalled())
    {
        return;
    }

    SetNeedsRepaintFlag(true);

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA);

        UpdateSize();
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA, false);
}

const RC<FontAtlas>& UIText::GetFontAtlasOrDefault() const
{
    HYP_SCOPE;

    const UIStage* stage = GetStage();

    if (!stage)
    {
        return m_fontAtlas;
    }

    return m_fontAtlas != nullptr
        ? m_fontAtlas
        : stage->GetDefaultFontAtlas();
}

void UIText::SetFontAtlas(const RC<FontAtlas>& fontAtlas)
{
    HYP_SCOPE;

    m_fontAtlas = fontAtlas;

    if (!IsInitCalled())
    {
        return;
    }

    SetNeedsRepaintFlag(true);

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA);

        UpdateSize();
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA, false);
}

Vec2f UIText::GetCharacterOffset(int characterIndex) const
{
    HYP_SCOPE;

    if (characterIndex < 0 || m_characterOffsets.Empty())
    {
        return Vec2f::Zero();
    }

    if (characterIndex >= int(m_characterOffsets.Size()))
    {
        characterIndex = int(m_characterOffsets.Size()) - 1;
    }

    return m_characterOffsets[characterIndex] * GetTextSize();
}

void UIText::UpdateTextAABB()
{
    HYP_SCOPE;

    if (const RC<FontAtlas>& fontAtlas = GetFontAtlasOrDefault())
    {
        const Vec2i parentBounds = GetParentBounds();
        const float textSize = GetTextSize();

        m_characterOffsets.Clear();

        m_textAabbWithBearing = CalculateTextAABB(*fontAtlas, m_text, parentBounds, textSize, true, nullptr);
        m_textAabbWithoutBearing = CalculateTextAABB(*fontAtlas, m_text, parentBounds, textSize, false, &m_characterOffsets);

        AssertThrow(m_characterOffsets.Size() == m_text.Length() + 1);
    }
    else
    {
        HYP_LOG_ONCE(UI, Warning, "No font atlas for UIText {}", GetName());
    }
}

void UIText::UpdateRenderData()
{
    HYP_SCOPE;

    if (m_lockedUpdates & UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA)
    {
        return;
    }

    m_deferredUpdates &= ~(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA | UIObjectUpdateType::UPDATE_CHILDREN_TEXT_RENDER_DATA);

    // Only update render data if computed visibility is true (visible)
    // When this changes to be true, UpdateRenderData will be called - no need to update it if we are not visible
    if (!GetComputedVisibility())
    {
        return;
    }

    if (const RC<FontAtlas>& fontAtlas = GetFontAtlasOrDefault())
    {
        const float textSize = GetTextSize();

        m_currentFontAtlasTexture = fontAtlas->GetAtlasTextures().GetAtlasForPixelSize(textSize);

        if (!m_currentFontAtlasTexture.IsValid())
        {
            HYP_LOG_ONCE(UI, Warning, "No font atlas texture for text size {}", textSize);
        }

        UpdateMaterial(false);
        UpdateMeshData(false);

        SetNeedsRepaintFlag(true);
    }
}

void UIText::UpdateMeshData_Internal()
{
    HYP_SCOPE;

    UIObject::UpdateMeshData_Internal();

    const RC<FontAtlas>& fontAtlas = GetFontAtlasOrDefault();

    if (!fontAtlas)
    {
        HYP_LOG_ONCE(UI, Warning, "No font atlas, cannot update text mesh data");

        return;
    }

    BoundingBox parentAabbClamped;

    if (UIObject* parent = GetParentUIObject())
    {
        parentAabbClamped = parent->GetAABBClamped();
    }

    MeshComponent& meshComponent = GetScene()->GetEntityManager()->GetComponent<MeshComponent>(GetEntity());

    const Vec2f position = GetAbsolutePosition();
    const float textSize = GetTextSize();

    Array<Matrix4> instanceTransforms;
    Array<Vec4f> instanceTexcoords;
    Array<Vec4f> instanceOffsets;
    Array<Vec4f> instanceSizes;

    ForEachCharacter(*fontAtlas, m_text, GetParentBounds(), textSize, nullptr, [&](const FontAtlasCharacterIterator& iter)
        {
            Transform characterTransform;
            characterTransform.SetScale(Vec3f(iter.glyphDimensions.x * textSize, iter.glyphDimensions.y * textSize, 1.0f));
            characterTransform.GetTranslation().y += (iter.cellDimensions.y - iter.glyphDimensions.y) * textSize;
            characterTransform.GetTranslation().y += iter.bearingY * textSize;
            characterTransform.GetTranslation() += Vec3f(iter.placement.x, iter.placement.y, 0.0f) * textSize;
            characterTransform.UpdateMatrix();

            BoundingBox characterAabb = characterTransform * BoundingBox(Vec3f::Zero(), Vec3f::One());
            characterAabb.min += Vec3f(position, 0.0f);
            characterAabb.max += Vec3f(position, 0.0f);

            BoundingBox characterAabbClamped = characterAabb.Intersection(parentAabbClamped);

            Matrix4 instanceTransform;
            instanceTransform[0][0] = characterAabbClamped.max.x - characterAabbClamped.min.x;
            instanceTransform[1][1] = characterAabbClamped.max.y - characterAabbClamped.min.y;
            instanceTransform[2][2] = 1.0f;
            instanceTransform[0][3] = characterAabbClamped.min.x;
            instanceTransform[1][3] = characterAabbClamped.min.y;
            instanceTransform[2][3] = 0.0f;

            instanceTransforms.PushBack(instanceTransform);

            const Vec2f size = characterAabb.GetExtent().GetXY();
            const Vec2f clampedSize = characterAabbClamped.GetExtent().GetXY();
            const Vec2f clampedOffset = characterAabb.min.GetXY() - characterAabbClamped.min.GetXY();

            Vec2f texcoordStart = Vec2f(iter.charOffset) * iter.atlasPixelSize;
            Vec2f texcoordEnd = (Vec2f(iter.charOffset) + (iter.glyphDimensions * 64.0f)) * iter.atlasPixelSize;

            instanceTexcoords.PushBack(Vec4f(texcoordStart, texcoordEnd));
            instanceOffsets.PushBack(Vec4f(clampedOffset, 0.0f, 0.0f));
            instanceSizes.PushBack(Vec4f(size, clampedSize));
        });

    meshComponent.instanceData.numInstances = instanceTransforms.Size();
    meshComponent.instanceData.SetBufferData(0, instanceTransforms.Data(), instanceTransforms.Size());
    meshComponent.instanceData.SetBufferData(1, instanceTexcoords.Data(), instanceTexcoords.Size());
    meshComponent.instanceData.SetBufferData(2, instanceOffsets.Data(), instanceOffsets.Size());
    meshComponent.instanceData.SetBufferData(3, instanceSizes.Data(), instanceSizes.Size());

    GetScene()->GetEntityManager()->AddTag<EntityTag::UPDATE_RENDER_PROXY>(GetEntity());
}

bool UIText::Repaint_Internal()
{
    // Do nothing
    return true;
}

MaterialAttributes UIText::GetMaterialAttributes() const
{
    return UIObject::GetMaterialAttributes();
}

Material::ParameterTable UIText::GetMaterialParameters() const
{
    Color color = GetTextColor();

    if (!IsEnabled())
    {
        color.a *= 0.5f;
    }

    return {
        { Material::MATERIAL_KEY_ALBEDO, Vec4f(color) }
    };
}

Material::TextureSet UIText::GetMaterialTextures() const
{
    if (!m_currentFontAtlasTexture.IsValid())
    {
        return UIObject::GetMaterialTextures();
    }

    return {
        { MaterialTextureKey::ALBEDO_MAP, m_currentFontAtlasTexture }
    };
}

void UIText::Update_Internal(float delta)
{
    HYP_SCOPE;

    if (m_deferredUpdates)
    {
        if (m_deferredUpdates & UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA)
        {
            UpdateRenderData();
        }
    }

    UIObject::Update_Internal(delta);
}

void UIText::UpdateSize_Internal(bool updateChildren)
{
    HYP_SCOPE;

    UpdateTextAABB();

    UIObject::UpdateSize_Internal(updateChildren);

    const Vec3f extentWithBearing = m_textAabbWithBearing.GetExtent();
    const Vec3f extentWithoutBearing = m_textAabbWithoutBearing.GetExtent();

    if (extentWithBearing.y <= MathUtil::epsilonF || extentWithoutBearing.y <= MathUtil::epsilonF)
    {
        HYP_LOG(UI, Warning, "Text AABB has zero height, cannot update size for UIText {}", GetName());

        return;
    }

    m_actualSize.y *= extentWithBearing.y / extentWithoutBearing.y;
}

BoundingBox UIText::CalculateInnerAABB_Internal() const
{
    return m_textAabbWithoutBearing * Vec3f(Vec2f(GetTextSize()), 1.0f);
}

void UIText::OnFontAtlasUpdate_Internal()
{
    HYP_SCOPE;

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA, false);
    SetDeferredUpdate(UIObjectUpdateType::UPDATE_SIZE, false);
}

void UIText::OnTextSizeUpdate_Internal()
{
    HYP_SCOPE;

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_TEXT_RENDER_DATA, false);
    SetDeferredUpdate(UIObjectUpdateType::UPDATE_SIZE, false);
}

Vec2i UIText::GetParentBounds() const
{
    Vec2i parentBounds;

    if (const UIObject* parent = GetParentUIObject())
    {
        const UIObjectSize parentSize = parent->GetSize();

        if (!(parentSize.GetFlagsX() & UIObjectSize::AUTO))
        {
            parentBounds.x = parent->GetActualSize().x;
        }

        if (!(parentSize.GetFlagsY() & UIObjectSize::AUTO))
        {
            parentBounds.y = parent->GetActualSize().y;
        }
    }

    return parentBounds;
}

#pragma endregion UIText

} // namespace hyperion