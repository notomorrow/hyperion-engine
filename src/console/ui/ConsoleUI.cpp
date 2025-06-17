/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <console/ui/ConsoleUI.hpp>
#include <console/ConsoleCommandManager.hpp>

#include <ui/UIDataSource.hpp>
#include <ui/UITextbox.hpp>
#include <ui/UIListView.hpp>

#include <core/containers/Array.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);
HYP_DECLARE_LOG_CHANNEL(Console);

#pragma region ConsoleHistory

enum class ConsoleHistoryEntryType : uint32
{
    NONE,
    TEXT,
    COMMAND,
    ERR
};

struct ConsoleHistoryEntry
{
    UUID uuid;
    ConsoleHistoryEntryType type = ConsoleHistoryEntryType::NONE;
    String text;
};

class ConsoleHistory
{
public:
    ConsoleHistory(const Handle<UIDataSource>& dataSource, int maxHistorySize = 100)
        : m_dataSource(dataSource),
          m_maxHistorySize(maxHistorySize)
    {
        m_entries.Reserve(m_maxHistorySize);
    }

    ~ConsoleHistory()
    {
    }

    HYP_FORCE_INLINE const Handle<UIDataSource>& GetDataSource() const
    {
        return m_dataSource;
    }

    void AddEntry(const String& text, ConsoleHistoryEntryType entryType)
    {
        if (m_entries.Any() && int(m_entries.Size() + 1) > m_maxHistorySize)
        {
            ConsoleHistoryEntry* entry = &m_entries.Front();

            if (m_dataSource)
            {
                m_dataSource->Remove(entry->uuid);
            }

            m_entries.PopFront();
        }

        ConsoleHistoryEntry entry;
        entry.type = entryType;
        entry.text = text;

        if (m_dataSource)
        {
            m_dataSource->Push(entry.uuid, HypData(entry), UUID::Invalid());
        }

        m_entries.PushBack(std::move(entry));
    }

    void ClearHistory()
    {
        m_entries.Clear();

        if (m_dataSource)
        {
            m_dataSource->Clear();
        }
    }

private:
    Handle<UIDataSource> m_dataSource;
    int m_maxHistorySize;
    Array<ConsoleHistoryEntry> m_entries;
};

#pragma endregion ConsoleHistory

#pragma region ConsoleUI

ConsoleUI::ConsoleUI()
    : m_loggerRedirectId(-1)
{
    SetBorderRadius(0);
    SetBorderFlags(UIObjectBorderFlags::ALL);
    SetPadding({ 5, 0 });
    SetBackgroundColor(Vec4f { 0.25f, 0.25f, 0.25f, 0.8f });
    SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    SetTextSize(12.0f);
    SetOriginAlignment(UIObjectAlignment::BOTTOM_LEFT);
    SetParentAlignment(UIObjectAlignment::BOTTOM_LEFT);

    m_loggerRedirectId = Logger::GetInstance().GetOutputStream()->AddRedirect(
        Log_Console.GetMaskBitset(),
        (void*)this,
        [](void* context, const LogChannel& channel, const LogMessage& message)
        {
            ConsoleUI* consoleUi = (ConsoleUI*)context;

            if (consoleUi->m_history)
            {
                consoleUi->m_history->AddEntry(message.message, ConsoleHistoryEntryType::TEXT);
            }
        },
        [](void* context, const LogChannel& channel, const LogMessage& message)
        {
            ConsoleUI* consoleUi = (ConsoleUI*)context;

            if (consoleUi->m_history)
            {
                consoleUi->m_history->AddEntry(message.message, ConsoleHistoryEntryType::ERR);
            }
        });
}

ConsoleUI::~ConsoleUI()
{
    if (m_loggerRedirectId != -1)
    {
        Logger::GetInstance().GetOutputStream()->RemoveRedirect(m_loggerRedirectId);
    }
}

void ConsoleUI::Init()
{
    UIObject::Init();

    OnMouseDown.Bind([this](const MouseEvent& eventData) -> UIEventHandlerResult
                   {
                       return UIEventHandlerResult::STOP_BUBBLING;
                   })
        .Detach();

    OnKeyDown.Bind([this](const KeyboardEvent& eventData) -> UIEventHandlerResult
                 {
                     return UIEventHandlerResult::STOP_BUBBLING;
                 })
        .Detach();

    OnKeyUp.Bind([this](const KeyboardEvent& eventData) -> UIEventHandlerResult
               {
                   return UIEventHandlerResult::STOP_BUBBLING;
               })
        .Detach();

    Handle<UIDataSource> dataSource = CreateObject<UIDataSource>(
        TypeWrapper<ConsoleHistoryEntry> {},
        [this](UIObject* parent, ConsoleHistoryEntry& value, const HypData& context) -> Handle<UIObject>
        {
            Handle<UIText> text = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));
            text->SetText(value.text);
            text->SetTextSize(12.0f);

            switch (value.type)
            {
            case ConsoleHistoryEntryType::COMMAND:
                text->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
                break;
            case ConsoleHistoryEntryType::TEXT:
                text->SetTextColor(Vec4f { 0.9f, 0.9f, 0.9f, 1.0f });
                break;
            case ConsoleHistoryEntryType::ERR:
                text->SetTextColor(Vec4f { 1.0f, 0.0f, 0.0f, 1.0f });
                break;
            default:
                text->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
                break;
            }

            return text;
        },
        [](UIObject* uiObject, ConsoleHistoryEntry& value, const HypData& context) -> void
        {
        });

    m_history = MakePimpl<ConsoleHistory>(dataSource, 100);

    Handle<UIListView> historyListView = CreateUIObject<UIListView>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { GetActualSize().y - 30, UIObjectSize::PIXEL }));
    historyListView->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    historyListView->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    historyListView->SetBackgroundColor(Vec4f { 0.0f, 0.0f, 0.0f, 0.0f });
    historyListView->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    historyListView->SetDataSource(dataSource);

    historyListView->OnChildAttached
        .Bind([this](UIObject* child) -> UIEventHandlerResult
        {
            // m_historyListView->SetScrollOffset(Vec2i {
            //     m_historyListView->GetScrollOffset().x,
            //     m_historyListView->GetActualInnerSize().y - m_historyListView->GetActualSize().y
            // }, /* smooth */ false);

            return UIEventHandlerResult::STOP_BUBBLING;
        })
        .Detach();

    AddChildUIObject(historyListView);

    m_historyListView = historyListView;

    m_historyListView->OnSelectedItemChange
        .Bind([this](UIListViewItem* item) -> UIEventHandlerResult
            {
                if (item)
                {
                    const ConsoleHistoryEntry& entry = m_history->GetDataSource()->Get(item->GetDataSourceElementUUID())->GetValue().Get<ConsoleHistoryEntry>();

                    m_textbox->SetText(entry.text);
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            })
        .Detach();

    Handle<UITextbox> textbox = CreateUIObject<UITextbox>(Vec2i { 0, historyListView->GetActualSize().y }, UIObjectSize({ 100, UIObjectSize::FILL }, { 25, UIObjectSize::PIXEL }));
    textbox->SetPlaceholder("Enter command");
    textbox->SetBackgroundColor(Vec4f { 0.0f, 0.0f, 0.0f, 0.5f });
    textbox->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    textbox->SetTextSize(16.0f);
    AddChildUIObject(textbox);

    m_textbox = textbox;

    m_textbox->OnKeyDown
        .Bind([this](const KeyboardEvent& eventData) -> UIEventHandlerResult
            {
                if (eventData.keyCode == KeyCode::RETURN)
                {
                    const String& text = m_textbox->GetText();

                    if (text.Any())
                    {
                        if (text.ToLower() == "clear")
                        {
                            m_history->ClearHistory();
                        }
                        else
                        {
                            m_history->AddEntry(text, ConsoleHistoryEntryType::COMMAND);
                        }

                        if (Result result = ConsoleCommandManager::GetInstance().ExecuteCommand(text); result.HasError())
                        {
                            HYP_LOG(Console, Error, "Error executing command: {}", result.GetError().GetMessage());
                        }
                        else
                        {
                            HYP_LOG(Console, Info, "Executed command: {}", text);
                        }

                        m_currentCommandText.Clear();
                        m_textbox->SetText("");
                    }
                }
                else if (eventData.keyCode == KeyCode::ARROW_UP)
                {
                    // @TODO Only cycle through COMMAND items..
                    if (m_historyListView && m_historyListView->GetListViewItems().Any())
                    {
                        const int selectedItemIndex = m_historyListView->GetSelectedItemIndex() - 1;

                        if (selectedItemIndex >= 0 && selectedItemIndex < int(m_historyListView->GetListViewItems().Size()))
                        {
                            m_historyListView->SetSelectedItemIndex(selectedItemIndex);
                        }
                    }
                }
                else if (eventData.keyCode == KeyCode::ARROW_DOWN)
                {
                    if (m_historyListView && m_historyListView->GetListViewItems().Any())
                    {
                        const int selectedItemIndex = m_historyListView->GetSelectedItemIndex() + 1;

                        if (selectedItemIndex < 0)
                        {
                            m_historyListView->SetSelectedItemIndex(0);
                        }
                        else if (selectedItemIndex < int(m_historyListView->GetListViewItems().Size()))
                        {
                            m_historyListView->SetSelectedItemIndex(selectedItemIndex);
                        }
                        else
                        {
                            m_textbox->SetText(m_currentCommandText);
                        }
                    }
                }
                else if (eventData.keyCode == KeyCode::ESC)
                {
                    m_textbox->SetText("");
                    m_currentCommandText.Clear();

                    // Lose focus of the console.
                    Blur();
                }
                else
                {
                    m_currentCommandText = m_textbox->GetText();
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            })
        .Detach();

    for (int i = 0; i < 10; i++)
    {
        HYP_LOG(Console, Info, "Console initialized {}", i);
    }
}

void ConsoleUI::UpdateSize_Internal(bool updateChildren)
{
    UIObject::UpdateSize_Internal(updateChildren);

    if (m_historyListView)
    {
        m_historyListView->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { GetActualSize().y - 30, UIObjectSize::PIXEL }));
    }

    if (m_textbox)
    {
        m_textbox->SetPosition(Vec2i { 0, m_historyListView->GetActualSize().y });
        m_textbox->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 25, UIObjectSize::PIXEL }));
    }
}

Material::ParameterTable ConsoleUI::GetMaterialParameters() const
{
    return UIObject::GetMaterialParameters();
}

#pragma endregion ConsoleUI

} // namespace hyperion
