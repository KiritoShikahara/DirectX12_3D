#pragma once
#include<Plugin/ImGui/imgui.h>
#include<string>

// =====================================================================
//  UIの見た目(配色・書体・描画部品)を一か所で決める
//  世界観: 月夜の剣戟。象牙色の文字、金の細線、深紅の体力、菱形のモチーフ
// =====================================================================
namespace Engine::UI::Theme
{
	//	配色
	ImU32 Ivory(float Alpha = 1.0f);
	ImU32 Gold(float Alpha = 1.0f);
	ImU32 GoldDim(float Alpha = 1.0f);
	ImU32 Crimson(float Alpha = 1.0f);
	ImU32 CrimsonLight(float Alpha = 1.0f);
	ImU32 Moon(float Alpha = 1.0f);
	ImU32 Vermilion(float Alpha = 1.0f);	//	危険(弾けない攻撃・スタミナ切れ)
	ImU32 Ink(float Alpha = 1.0f);			//	下地
	ImU32 Rgba(float R, float G, float B, float A);

	enum class eFont
	{
		Display,	//	欧文見出し(Cinzel)
		Mincho,		//	和文見出し・本文(しっぽり明朝)
		Sans,		//	小さい補足(Noto Sans JP)
	};

	/// <summary>
	/// フォントの読み込み(Application の初期化で1回)
	/// </summary>
	void Initialize();

	/// <summary>
	/// 1080p 基準の拡大率
	/// </summary>
	float Scale();

	ImVec2 TextSize(eFont Font, float Size, const char* Text, float Tracking = 0.0f);

	/// <summary>
	/// 文字(影付き)。Align: 0=左 0.5=中央 1=右  Tracking: 字間(px)
	/// </summary>
	void Text(ImDrawList* Dl, eFont Font, ImVec2 Pos, float Size, ImU32 Color, const char* Str, float Align = 0.0f, float Tracking = 0.0f);

	/// <summary>
	/// 発光する文字(見出し・バナー用)
	/// </summary>
	void GlowText(ImDrawList* Dl, eFont Font, ImVec2 Pos, float Size, ImU32 Color, ImU32 Glow, const char* Str, float Align = 0.5f, float Tracking = 0.0f);

	/// <summary>
	/// 中央が濃く両端が消える金の細線
	/// </summary>
	void FadeLine(ImDrawList* Dl, ImVec2 Center, float HalfWidth, ImU32 Color, float Thickness = 1.0f);

	/// <summary>
	/// 菱形(塗り/枠)
	/// </summary>
	void Diamond(ImDrawList* Dl, ImVec2 Center, float Radius, ImU32 Color, bool Filled, float Thickness = 1.0f);

	/// <summary>
	/// 細身のゲージ。Trail は減った分の残像。両端に菱形の金具
	/// </summary>
	void Gauge(ImDrawList* Dl, ImVec2 Pos, ImVec2 Size, float Ratio, float Trail, ImU32 Fill, ImU32 FillBright, bool EndCaps = true);

	/// <summary>
	/// 左右に伸びる帯(バナー・タイトルの下敷き)
	/// </summary>
	void Band(ImDrawList* Dl, float CenterY, float HalfHeight, float Alpha);

	/// <summary>
	/// ボタン表記のチップ。1文字(A/B/X/Y/Q/E)は丸、それ以外は角丸
	/// </summary>
	/// <returns>描いた幅</returns>
	float KeyChip(ImDrawList* Dl, ImVec2 Pos, float Height, const char* Label, float Alpha = 1.0f);

	/// <summary>
	/// 「[キー] 説明」の並び。描いた幅を返す
	/// </summary>
	float Prompt(ImDrawList* Dl, ImVec2 Pos, float Height, const char* Key, const char* Action, float Alpha = 1.0f);
	float PromptWidth(float Height, const char* Key, const char* Action);

	/// <summary>
	/// 扇形の塗り(クールダウン表示)。Ratio=残り割合
	/// </summary>
	void RadialFill(ImDrawList* Dl, ImVec2 Center, float Radius, float Ratio, ImU32 Color);
}
