#include "pch.h"
#include "UI_Viewport_Utils.h"

namespace UIEditor::ViewportUtils
{
    namespace
    {
        vector<string> Split_TextLines(const string& text)
        {
            vector<string> lines;
            size_t lineStart = 0;

            for (size_t i = 0; i <= text.size(); ++i)
            {
                if (i == text.size() || text[i] == '\n')
                {
                    size_t lineEnd = i;
                    if (lineEnd > lineStart && text[lineEnd - 1] == '\r')
                        --lineEnd;

                    lines.push_back(text.substr(lineStart, lineEnd - lineStart));
                    lineStart = i + 1;
                }
            }

            if (lines.empty())
                lines.push_back("");

            return lines;
        }

        vector<string> Wrap_Paragraph(ImFont* font, const string& paragraph, float fontSize, float maxWidth)
        {
            vector<string> wrappedLines;
            if (nullptr == font || paragraph.empty() || maxWidth <= 1.f)
            {
                wrappedLines.push_back(paragraph);
                return wrappedLines;
            }

            string currentLine;
            size_t wordStart = 0;

            auto pushWord = [&](const string& word)
            {
                if (word.empty())
                    return;

                const string candidate = currentLine.empty() ? word : currentLine + " " + word;
                const float candidateWidth = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, candidate.c_str()).x;
                if (candidateWidth <= maxWidth || currentLine.empty())
                {
                    currentLine = candidate;
                    return;
                }

                wrappedLines.push_back(currentLine);
                currentLine = word;
            };

            for (size_t i = 0; i <= paragraph.size(); ++i)
            {
                if (i == paragraph.size() || paragraph[i] == ' ' || paragraph[i] == '\t')
                {
                    pushWord(paragraph.substr(wordStart, i - wordStart));
                    wordStart = i + 1;
                }
            }

            if (!currentLine.empty())
                wrappedLines.push_back(currentLine);

            if (wrappedLines.empty())
                wrappedLines.push_back("");

            return wrappedLines;
        }

        vector<string> Build_TextPreviewLines(ImFont* font, const string& text, float fontSize, float maxWidth, bool wrapText)
        {
            vector<string> lines;
            const vector<string> rawLines = Split_TextLines(text);
            for (const string& rawLine : rawLines)
            {
                if (!wrapText)
                {
                    lines.push_back(rawLine);
                    continue;
                }

                vector<string> wrappedLines = Wrap_Paragraph(font, rawLine, fontSize, maxWidth);
                lines.insert(lines.end(), wrappedLines.begin(), wrappedLines.end());
            }

            if (lines.empty())
                lines.push_back("");

            return lines;
        }

        float Resolve_TextLineX(const ImVec2& objectMin, const ImVec2& objectMax, float lineWidth, UI_TextAlign textAlign)
        {
            constexpr float textPaddingX = 4.f;
            const float textWidth = max(0.f, objectMax.x - objectMin.x);

            switch (textAlign)
            {
            case UI_TextAlign::Center:
                return objectMin.x + max(textPaddingX, (textWidth - lineWidth) * 0.5f);
            case UI_TextAlign::Right:
                return objectMax.x - lineWidth - textPaddingX;
            case UI_TextAlign::Left:
            default:
                return objectMin.x + textPaddingX;
            }
        }
    }

    float Clamp_Zoom(float zoomFactor)
    {
        return min(max(zoomFactor, kMinViewportZoomFactor), kMaxViewportZoomFactor);
    }

    ImVec2 Snap_Position(const ImVec2& position, float gridSize)
    {
        const float safeGridSize = max(kMinGridSize, gridSize);
        return ImVec2(
            roundf(position.x / safeGridSize) * safeGridSize,
            roundf(position.y / safeGridSize) * safeGridSize);
    }

    float Snap_Value(float value, float gridSize)
    {
        const float safeGridSize = max(kMinGridSize, gridSize);
        return roundf(value / safeGridSize) * safeGridSize;
    }

    ImVec2 Get_ResizeHandleCenter(UI_ResizeHandle resizeHandle, const ImVec2& objectMin, const ImVec2& objectMax)
    {
        const float centerX = (objectMin.x + objectMax.x) * 0.5f;
        const float centerY = (objectMin.y + objectMax.y) * 0.5f;

        switch (resizeHandle)
        {
        case UI_ResizeHandle::North:
            return ImVec2(centerX, objectMin.y);
        case UI_ResizeHandle::NorthEast:
            return ImVec2(objectMax.x, objectMin.y);
        case UI_ResizeHandle::East:
            return ImVec2(objectMax.x, centerY);
        case UI_ResizeHandle::SouthEast:
            return ImVec2(objectMax.x, objectMax.y);
        case UI_ResizeHandle::South:
            return ImVec2(centerX, objectMax.y);
        case UI_ResizeHandle::SouthWest:
            return ImVec2(objectMin.x, objectMax.y);
        case UI_ResizeHandle::West:
            return ImVec2(objectMin.x, centerY);
        case UI_ResizeHandle::NorthWest:
            return ImVec2(objectMin.x, objectMin.y);
        case UI_ResizeHandle::None:
        default:
            return ImVec2(0.f, 0.f);
        }
    }

    UI_ResizeHandle Hit_TestResizeHandle(const ImVec2& mousePosition, const ImVec2& objectMin, const ImVec2& objectMax)
    {
        constexpr float hitSize = 10.f;
        const UI_ResizeHandle handles[] = {
            UI_ResizeHandle::NorthWest,
            UI_ResizeHandle::NorthEast,
            UI_ResizeHandle::SouthEast,
            UI_ResizeHandle::SouthWest,
            UI_ResizeHandle::North,
            UI_ResizeHandle::East,
            UI_ResizeHandle::South,
            UI_ResizeHandle::West,
        };

        for (UI_ResizeHandle handle : handles)
        {
            const ImVec2 center = Get_ResizeHandleCenter(handle, objectMin, objectMax);
            if (mousePosition.x >= center.x - hitSize * 0.5f && mousePosition.x <= center.x + hitSize * 0.5f &&
                mousePosition.y >= center.y - hitSize * 0.5f && mousePosition.y <= center.y + hitSize * 0.5f)
            {
                return handle;
            }
        }

        return UI_ResizeHandle::None;
    }

    void Draw_ResizeHandles(ImDrawList* drawList, const ImVec2& objectMin, const ImVec2& objectMax)
    {
        if (nullptr == drawList)
            return;

        constexpr float handleSize = 8.f;
        const UI_ResizeHandle handles[] = {
            UI_ResizeHandle::North,
            UI_ResizeHandle::NorthEast,
            UI_ResizeHandle::East,
            UI_ResizeHandle::SouthEast,
            UI_ResizeHandle::South,
            UI_ResizeHandle::SouthWest,
            UI_ResizeHandle::West,
            UI_ResizeHandle::NorthWest,
        };

        for (UI_ResizeHandle handle : handles)
        {
            const ImVec2 center = Get_ResizeHandleCenter(handle, objectMin, objectMax);
            const ImVec2 handleMin = ImVec2(center.x - handleSize * 0.5f, center.y - handleSize * 0.5f);
            const ImVec2 handleMax = ImVec2(center.x + handleSize * 0.5f, center.y + handleSize * 0.5f);
            drawList->AddRectFilled(handleMin, handleMax, IM_COL32(255, 217, 102, 255));
            drawList->AddRect(handleMin, handleMax, IM_COL32(36, 39, 46, 255));
        }
    }

    void Set_ResizeMouseCursor(UI_ResizeHandle resizeHandle)
    {
        switch (resizeHandle)
        {
        case UI_ResizeHandle::North:
        case UI_ResizeHandle::South:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            break;
        case UI_ResizeHandle::East:
        case UI_ResizeHandle::West:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            break;
        case UI_ResizeHandle::NorthEast:
        case UI_ResizeHandle::SouthWest:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
            break;
        case UI_ResizeHandle::SouthEast:
        case UI_ResizeHandle::NorthWest:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
            break;
        case UI_ResizeHandle::None:
        default:
            break;
        }
    }

    void Draw_ImGuiTextPreview(
        ImDrawList* drawList,
        const UI_Object& object,
        const ImVec2& objectMin,
        const ImVec2& objectMax,
        float displayScaleX)
    {
        if (nullptr == drawList || object.text.empty())
            return;

        ImFont* font = ImGui::GetFont();
        if (nullptr == font)
            return;

        const float fontPixelSize = max(4.f, object.fontSize) * max(0.01f, object.scale.x) * displayScaleX;
        const float maxTextWidth = max(1.f, objectMax.x - objectMin.x - 8.f);
        const float lineHeight = max(4.f, fontPixelSize * 1.2f);
        const ImU32 previewTextColor = ImGui::ColorConvertFloat4ToU32(object.color);
        const ImVec4 clipRect = ImVec4(objectMin.x, objectMin.y, objectMax.x, objectMax.y);
        const vector<string> lines = Build_TextPreviewLines(font, object.text, fontPixelSize, maxTextWidth, object.textWrap);

        float textY = objectMin.y + 3.f;
        for (const string& line : lines)
        {
            if (textY >= objectMax.y)
                break;

            const float lineWidth = font->CalcTextSizeA(fontPixelSize, FLT_MAX, 0.f, line.c_str()).x;
            const float textX = Resolve_TextLineX(objectMin, objectMax, lineWidth, object.textAlign);
            drawList->AddText(font, fontPixelSize, ImVec2(textX, textY), previewTextColor, line.c_str(), nullptr, 0.f, &clipRect);
            textY += lineHeight;
        }
    }
}
