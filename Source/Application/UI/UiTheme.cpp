#include "pch.h"
#include "UiTheme.hpp"

#include<algorithm>
#include<cmath>
#include<cstring>

namespace
{
	ImFont* sFonts[3] = { nullptr, nullptr, nullptr };

	ImFont* GetFont(Engine::UI::Theme::eFont Font)
	{
		ImFont* f = sFonts[static_cast<int>(Font)];
		return f != nullptr ? f : ImGui::GetFont();
	}

	float AlphaOf(ImU32 c)
	{
		return static_cast<float>((c >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
	}

	/// <summary>
	/// 1文字ずつ字間を空けて描く(UTF-8)
	/// </summary>
	void DrawTracked(ImDrawList* Dl, ImFont* Font, float Size, ImVec2 Pos, ImU32 Color, const char* Str, float Tracking)
	{
		if (Tracking <= 0.0f)
		{
			Dl->AddText(Font, Size, Pos, Color, Str);
			return;
		}
		const char* p = Str;
		while (*p != '\0')
		{
			//	UTF-8 の先頭バイトから1文字の長さを求める
			const unsigned char lead = static_cast<unsigned char>(*p);
			const int bytes = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : 4;
			Dl->AddText(Font, Size, Pos, Color, p, p + bytes);
			Pos.x += Font->CalcTextSizeA(Size, FLT_MAX, 0.0f, p, p + bytes).x + Tracking;
			p += bytes;
		}
	}
}

namespace Engine::UI::Theme
{
	ImU32 Rgba(float R, float G, float B, float A)
	{
		return ImGui::ColorConvertFloat4ToU32(ImVec4(R, G, B, std::clamp(A, 0.0f, 1.0f)));
	}
	ImU32 Ivory(float Alpha) { return Rgba(0.94f, 0.90f, 0.81f, Alpha); }
	ImU32 Gold(float Alpha) { return Rgba(0.86f, 0.71f, 0.42f, Alpha); }
	ImU32 GoldDim(float Alpha) { return Rgba(0.52f, 0.43f, 0.27f, Alpha); }
	ImU32 Crimson(float Alpha) { return Rgba(0.45f, 0.06f, 0.07f, Alpha); }
	ImU32 CrimsonLight(float Alpha) { return Rgba(0.80f, 0.20f, 0.16f, Alpha); }
	ImU32 Moon(float Alpha) { return Rgba(0.66f, 0.79f, 1.00f, Alpha); }
	ImU32 Vermilion(float Alpha) { return Rgba(0.95f, 0.25f, 0.12f, Alpha); }
	ImU32 Ink(float Alpha) { return Rgba(0.015f, 0.018f, 0.03f, Alpha); }

	void Initialize()
	{
		ImGuiIO& io = ImGui::GetIO();
		sFonts[static_cast<int>(eFont::Display)] = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Cinzel.ttf", 32.0f);
		sFonts[static_cast<int>(eFont::Mincho)] = io.Fonts->AddFontFromFileTTF("Assets/Fonts/ShipporiMincho-Medium.ttf", 32.0f);
		sFonts[static_cast<int>(eFont::Sans)] = io.Fonts->AddFontFromFileTTF("Assets/Fonts/NotoSansJP-Regular.ttf", 20.0f);
	}

	float Scale()
	{
		return ImGui::GetIO().DisplaySize.y / 1080.0f;
	}

	ImVec2 TextSize(eFont Font, float Size, const char* Text, float Tracking)
	{
		ImFont* f = GetFont(Font);
		ImVec2 size = f->CalcTextSizeA(Size, FLT_MAX, 0.0f, Text);
		if (Tracking > 0.0f)
		{
			int count = 0;
			for (const char* p = Text; *p != '\0'; p++)
			{
				if ((static_cast<unsigned char>(*p) & 0xC0) != 0x80) count++;
			}
			size.x += Tracking * (std::max)(0, count - 1);
		}
		return size;
	}

	void Text(ImDrawList* Dl, eFont Font, ImVec2 Pos, float Size, ImU32 Color, const char* Str, float Align, float Tracking)
	{
		ImFont* f = GetFont(Font);
		Pos.x -= TextSize(Font, Size, Str, Tracking).x * Align;
		const float a = AlphaOf(Color);
		const float shadow = (std::max)(1.0f, Size * 0.06f);
		DrawTracked(Dl, f, Size, ImVec2(Pos.x + shadow, Pos.y + shadow), Ink(0.8f * a), Str, Tracking);
		DrawTracked(Dl, f, Size, Pos, Color, Str, Tracking);
	}

	void GlowText(ImDrawList* Dl, eFont Font, ImVec2 Pos, float Size, ImU32 Color, ImU32 Glow, const char* Str, float Align, float Tracking)
	{
		ImFont* f = GetFont(Font);
		Pos.x -= TextSize(Font, Size, Str, Tracking).x * Align;
		//	周りに薄く重ねて光をにじませる
		const float r = Size * 0.05f;
		for (int i = 0; i < 8; i++)
		{
			const float ang = i * 0.785398f;
			DrawTracked(Dl, f, Size, ImVec2(Pos.x + std::cos(ang) * r, Pos.y + std::sin(ang) * r), Glow, Str, Tracking);
		}
		DrawTracked(Dl, f, Size, Pos, Color, Str, Tracking);
	}

	void FadeLine(ImDrawList* Dl, ImVec2 Center, float HalfWidth, ImU32 Color, float Thickness)
	{
		ImVec4 c = ImGui::ColorConvertU32ToFloat4(Color);
		const ImU32 clear = ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, 0.0f));
		const float y0 = Center.y - Thickness * 0.5f;
		const float y1 = Center.y + Thickness * 0.5f;
		Dl->AddRectFilledMultiColor(ImVec2(Center.x - HalfWidth, y0), ImVec2(Center.x, y1), clear, Color, Color, clear);
		Dl->AddRectFilledMultiColor(ImVec2(Center.x, y0), ImVec2(Center.x + HalfWidth, y1), Color, clear, clear, Color);
	}

	void Diamond(ImDrawList* Dl, ImVec2 C, float R, ImU32 Color, bool Filled, float Thickness)
	{
		const ImVec2 a(C.x, C.y - R), b(C.x + R, C.y), c(C.x, C.y + R), d(C.x - R, C.y);
		if (Filled)
		{
			Dl->AddQuadFilled(a, b, c, d, Color);
		}
		else
		{
			Dl->AddQuad(a, b, c, d, Color, Thickness);
		}
	}

	void Gauge(ImDrawList* Dl, ImVec2 Pos, ImVec2 Size, float Ratio, float Trail, ImU32 Fill, ImU32 FillBright, bool EndCaps)
	{
		Ratio = std::clamp(Ratio, 0.0f, 1.0f);
		Trail = std::clamp(Trail, 0.0f, 1.0f);
		const ImVec2 end(Pos.x + Size.x, Pos.y + Size.y);

		//	下地と枠(細い金線)
		Dl->AddRectFilled(ImVec2(Pos.x, Pos.y), end, Ink(0.72f));
		if (Trail > Ratio)
		{
			Dl->AddRectFilled(ImVec2(Pos.x + Size.x * Ratio, Pos.y), ImVec2(Pos.x + Size.x * Trail, end.y), Ivory(0.55f));
		}
		if (Ratio > 0.0f)
		{
			const float fx = Pos.x + Size.x * Ratio;
			Dl->AddRectFilledMultiColor(Pos, ImVec2(fx, end.y), Fill, FillBright, FillBright, Fill);
			//	上辺のハイライトで少し立体に
			Dl->AddRectFilled(Pos, ImVec2(fx, Pos.y + (std::max)(1.0f, Size.y * 0.22f)), Rgba(1, 1, 1, 0.12f));
		}
		Dl->AddRect(ImVec2(Pos.x - 1, Pos.y - 1), ImVec2(end.x + 1, end.y + 1), GoldDim(0.85f), 0.0f, 0, 1.0f);

		if (EndCaps)
		{
			const float r = Size.y * 0.5f + 3.0f;
			const float cy = Pos.y + Size.y * 0.5f;
			Diamond(Dl, ImVec2(Pos.x - r - 2, cy), r, Ink(0.9f), true);
			Diamond(Dl, ImVec2(Pos.x - r - 2, cy), r, Gold(0.9f), false, 1.2f);
			Diamond(Dl, ImVec2(end.x + r + 2, cy), r, Ink(0.9f), true);
			Diamond(Dl, ImVec2(end.x + r + 2, cy), r, Gold(0.9f), false, 1.2f);
		}
	}

	void Band(ImDrawList* Dl, float CenterY, float HalfHeight, float Alpha)
	{
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		const ImU32 dark = Ink(0.62f * Alpha);
		const ImU32 clear = Ink(0.0f);
		const float mid = display.x * 0.5f;
		//	中央が濃く、左右と上下へ消える(頂点色の順: 左上,右上,右下,左下)
		Dl->AddRectFilledMultiColor(ImVec2(0, CenterY - HalfHeight), ImVec2(mid, CenterY), clear, clear, dark, clear);
		Dl->AddRectFilledMultiColor(ImVec2(mid, CenterY - HalfHeight), ImVec2(display.x, CenterY), clear, clear, clear, dark);
		Dl->AddRectFilledMultiColor(ImVec2(0, CenterY), ImVec2(mid, CenterY + HalfHeight), clear, dark, clear, clear);
		Dl->AddRectFilledMultiColor(ImVec2(mid, CenterY), ImVec2(display.x, CenterY + HalfHeight), dark, clear, clear, clear);
	}

	float KeyChip(ImDrawList* Dl, ImVec2 Pos, float Height, const char* Label, float Alpha)
	{
		const bool single = std::strlen(Label) == 1;
		const float textSize = Height * 0.62f;
		const ImVec2 ts = TextSize(eFont::Sans, textSize, Label);
		const float width = single ? Height : ts.x + Height * 0.7f;
		const ImVec2 end(Pos.x + width, Pos.y + Height);

		if (single)
		{
			const ImVec2 c(Pos.x + Height * 0.5f, Pos.y + Height * 0.5f);
			Dl->AddCircleFilled(c, Height * 0.5f, Ink(0.85f * Alpha), 24);
			Dl->AddCircle(c, Height * 0.5f, Ivory(0.9f * Alpha), 24, 1.5f);
		}
		else
		{
			Dl->AddRectFilled(Pos, end, Ink(0.85f * Alpha), Height * 0.25f);
			Dl->AddRect(Pos, end, Ivory(0.85f * Alpha), Height * 0.25f, 0, 1.3f);
		}
		Dl->AddText(GetFont(eFont::Sans), textSize, ImVec2(Pos.x + (width - ts.x) * 0.5f, Pos.y + (Height - ts.y) * 0.5f), Ivory(Alpha), Label);
		return width;
	}

	float PromptWidth(float Height, const char* Key, const char* Action)
	{
		const bool single = std::strlen(Key) == 1;
		const float keyWidth = single ? Height : TextSize(eFont::Sans, Height * 0.62f, Key).x + Height * 0.7f;
		return keyWidth + Height * 0.35f + TextSize(eFont::Mincho, Height * 0.72f, Action).x;
	}

	float Prompt(ImDrawList* Dl, ImVec2 Pos, float Height, const char* Key, const char* Action, float Alpha)
	{
		const float w = KeyChip(Dl, Pos, Height, Key, Alpha);
		const float textSize = Height * 0.72f;
		const float x = Pos.x + w + Height * 0.35f;
		Text(Dl, eFont::Mincho, ImVec2(x, Pos.y + (Height - textSize) * 0.5f - 1.0f), textSize, Ivory(0.95f * Alpha), Action);
		return w + Height * 0.35f + TextSize(eFont::Mincho, textSize, Action).x;
	}

	void RadialFill(ImDrawList* Dl, ImVec2 Center, float Radius, float Ratio, ImU32 Color)
	{
		Ratio = std::clamp(Ratio, 0.0f, 1.0f);
		if (Ratio <= 0.0f)
		{
			return;
		}
		//	180度を超えると凸形でなくなるので三角形の扇で塗る
		const float start = -1.5707963f;
		const int segments = (std::max)(2, static_cast<int>(48 * Ratio));
		const float step = Ratio * 6.2831853f / segments;
		for (int i = 0; i < segments; i++)
		{
			const float a0 = start + step * i;
			const float a1 = a0 + step;
			Dl->AddTriangleFilled(Center,
				ImVec2(Center.x + std::cos(a0) * Radius, Center.y + std::sin(a0) * Radius),
				ImVec2(Center.x + std::cos(a1) * Radius, Center.y + std::sin(a1) * Radius), Color);
		}
	}
}
