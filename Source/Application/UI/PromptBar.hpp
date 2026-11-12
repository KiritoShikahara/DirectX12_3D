#pragma once
#include"Application/UI/UiTheme.hpp"
#include"Application/UI/InputGlyphs.hpp"
#include<initializer_list>

namespace Engine::UI
{
	struct PromptItem
	{
		eAction Action;
		const char* Text;
	};

	/// <summary>
	/// 画面下に「[キー] 説明」を右寄せで並べる(メニュー画面共通)
	/// </summary>
	inline void DrawPromptBar(std::initializer_list<PromptItem> Items, float Alpha = 1.0f)
	{
		ImDrawList* dl = ImGui::GetForegroundDrawList();
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		const float s = Theme::Scale();
		const float h = 30 * s;
		const float gap = 34 * s;

		float total = 0.0f;
		for (const auto& item : Items)
		{
			total += Theme::PromptWidth(h, KeyLabel(item.Action), item.Text) + gap;
		}
		float x = display.x - 70 * s - total + gap;
		const float y = display.y - 74 * s;
		Theme::FadeLine(dl, ImVec2(display.x - 70 * s - total * 0.5f, y - 14 * s), total * 0.6f, Theme::Gold(0.6f * Alpha), 1.0f);
		for (const auto& item : Items)
		{
			x += Theme::Prompt(dl, ImVec2(x, y), h, KeyLabel(item.Action), item.Text, Alpha) + gap;
		}
	}
}
