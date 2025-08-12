#include <editor/ui/property_panels/TransformEditorPropertyPanel.hpp>
#include <editor/ui/EditorUI.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/object/HypProperty.hpp>
#include <core/object/HypData.hpp>

#include <scene/Node.hpp>

#include <ui/UIGrid.hpp>
#include <ui/UIText.hpp>
#include <ui/UIDataSource.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

TransformEditorPropertyPanel::TransformEditorPropertyPanel()
    : EditorPropertyPanelBase()
{
}

TransformEditorPropertyPanel::~TransformEditorPropertyPanel() = default;

void TransformEditorPropertyPanel::Build_Impl(const HypData& hypData, const HypProperty* property)
{
    HYP_NAMED_SCOPE("TransformEditorPropertyPanel::Build");

    Assert(hypData.IsValid());
    
    const Handle<Node>& node = hypData.Get<Handle<Node>>();
    Assert(node != nullptr);

    Assert(property != nullptr);
    Assert(property->CanGet());

    HypData resultData = property->Get(hypData);
    Assert(resultData.IsValid());
    
    Transform transform = resultData.Get<Transform>();
    m_currentValue = std::move(resultData);
    
    OnValueChange
        .Bind([nodeWeak = node.ToWeak(), property](const HypData& value) -> UIEventHandlerResult
        {
            Handle<Node> node = nodeWeak.Lock();
            if (!node)
            {
                return UIEventHandlerResult::ERR;
            }
            
            NodeUnlockTransformScope scope(*node);
            
            HypData targetData(node.ToRef());
            property->Set(targetData, value);
            
            return UIEventHandlerResult::OK;
        })
        .Detach();

    Handle<UIGrid> grid = CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    AddChildUIObject(grid);

    {
        Handle<UIGridRow> translationHeaderRow = grid->AddRow();
        Handle<UIGridColumn> translationHeaderColumn = translationHeaderRow->AddColumn();

        Handle<UIText> translationHeader = CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        translationHeader->SetText("Translation");
        translationHeaderColumn->AddChildUIObject(translationHeader);

        Handle<UIGridRow> translationValueRow = grid->AddRow();
        Handle<UIGridColumn> translationValueColumn = translationValueRow->AddColumn();

        if (Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory<Vec3f>())
        {
            Handle<UIObject> translationElement = factory->CreateUIObject(this, HypData(transform.GetTranslation()), {});
            
            AddDelegateHandler(translationElement->OnValueChange.Bind([this, weakThis = WeakHandleFromThis()](const HypData& value) -> UIEventHandlerResult
            {
                Handle<TransformEditorPropertyPanel> strongThis = weakThis.Lock();
                if (!strongThis)
                {
                    return UIEventHandlerResult::OK;
                }
                
                Transform currentValue = GetCurrentValue().Get<Transform>();
                currentValue.SetTranslation(value.Get<Vec3f>());
                SetCurrentValue(HypData(currentValue));
                
                return UIEventHandlerResult::OK;
            }));
            
            translationValueColumn->AddChildUIObject(translationElement);
        }
    }

    {
        Handle<UIGridRow> rotationHeaderRow = grid->AddRow();
        Handle<UIGridColumn> rotationHeaderColumn = rotationHeaderRow->AddColumn();

        Handle<UIText> rotationHeader = CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        rotationHeader->SetText("Rotation");
        rotationHeaderColumn->AddChildUIObject(rotationHeader);

        Handle<UIGridRow> rotationValueRow = grid->AddRow();
        Handle<UIGridColumn> rotationValueColumn = rotationValueRow->AddColumn();

        if (Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory<Quaternion>())
        {
            Handle<UIObject> rotationElement = factory->CreateUIObject(this, HypData(transform.GetRotation()), {});
            
            AddDelegateHandler(rotationElement->OnValueChange.Bind([this, weakThis = WeakHandleFromThis()](const HypData& value) -> UIEventHandlerResult
            {
                Handle<TransformEditorPropertyPanel> strongThis = weakThis.Lock();
                if (!strongThis)
                {
                    return UIEventHandlerResult::OK;
                }
                
                Transform currentValue = GetCurrentValue().Get<Transform>();
                currentValue.SetRotation(value.Get<Quaternion>());
                SetCurrentValue(HypData(currentValue));
                
                return UIEventHandlerResult::OK;
            }));
            
            rotationValueColumn->AddChildUIObject(rotationElement);
        }
    }

    {
        Handle<UIGridRow> scaleHeaderRow = grid->AddRow();
        Handle<UIGridColumn> scaleHeaderColumn = scaleHeaderRow->AddColumn();

        Handle<UIText> scaleHeader = CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        scaleHeader->SetText("Scale");
        scaleHeaderColumn->AddChildUIObject(scaleHeader);

        Handle<UIGridRow> scaleValueRow = grid->AddRow();
        Handle<UIGridColumn> scaleValueColumn = scaleValueRow->AddColumn();

        if (Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory<Vec3f>())
        {
            Handle<UIObject> scaleElement = factory->CreateUIObject(this, HypData(transform.GetScale()), {});
            
            AddDelegateHandler(scaleElement->OnValueChange.Bind([this, weakThis = WeakHandleFromThis()](const HypData& value) -> UIEventHandlerResult
            {
                Handle<TransformEditorPropertyPanel> strongThis = weakThis.Lock();
                if (!strongThis)
                {
                    return UIEventHandlerResult::OK;
                }
                
                Transform currentValue = GetCurrentValue().Get<Transform>();
                currentValue.SetScale(value.Get<Vec3f>());
                SetCurrentValue(HypData(currentValue));
                
                return UIEventHandlerResult::OK;
            }));
            
            scaleValueColumn->AddChildUIObject(scaleElement);
        }
    }
}

} // namespace hyperion
