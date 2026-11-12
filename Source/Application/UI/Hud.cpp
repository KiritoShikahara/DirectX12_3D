#include "pch.h"
#include "Hud.hpp"

#include"Application/Combat/CombatComponents.hpp"
#include"Application/Combat/CombatEvents.hpp"
#include"Application/Components/Status/StatusComponent.hpp"
#include"Application/Components/Tag/TagComponent.hpp"
#include"Application/Components/Skill/SkillComponent.hpp"
#include"System/Conponent/Transform/TransformConponent.hpp"
#include"System/Camera/Camera.hpp"
#include"Application/Components/CameraWork/CameraControl.hpp"

#include<algorithm>
#include<cmath>
#include<vector>

namespace
{
	using namespace Engine;
	using namespace Engine::System;
	using Combat::CombatEventType;

	struct FloatingText
	{
		std::string Text;
		Math::Vector3 World;
		float Age = 0.0f;
		float Life = 0.9f;
		ImU32 Color = IM_COL32_WHITE;
		float Size = 30.0f;
		float Drift = 0.0f;
	};

	struct Banner
	{
		std::string Text;
		std::string Sub;
		ImU32 Color = IM_COL32_WHITE;
		float Age = 99.0f;
		float Life = 1.0f;
	};

	struct Marker
	{
		Math::Vector3 World;
		bool Parryable = true;
		float Age = 0.0f;
	};

	struct WorldLabel
	{
		Math::Vector3 World;
		std::string Text;
		bool Highlight;
	};
	std::vector<WorldLabel> sLabels;

	bool sEnabled = false;
	const UI::Objective* sObjective = nullptr;
	std::vector<FloatingText> sFloating;
	std::vector<Marker> sMarkers;
	Banner sBanner;
	float sPlayerHpTrail = 1.0f;
	float sEnemyHpTrail = 1.0f;
	entt::entity sTrackedEnemy = entt::null;
	float sTime = 0.0f;

	float Scale()
	{
		return ImGui::GetIO().DisplaySize.y / 1080.0f;
	}

	ImU32 Col(float r, float g, float b, float a = 1.0f)
	{
		return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
	}

	ImU32 WithAlpha(ImU32 c, float a)
	{
		ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
		v.w *= a;
		return ImGui::ColorConvertFloat4ToU32(v);
	}

	ImVec2 TextSize(float Size, const char* Text)
	{
		return ImGui::GetFont()->CalcTextSizeA(Size, FLT_MAX, 0.0f, Text);
	}

	/// <summary>
	/// 影付きの文字。Align: 0=左 0.5=中央 1=右
	/// </summary>
	void Text(ImDrawList* dl, ImVec2 Pos, float Size, ImU32 Color, const char* Str, float Align = 0.0f)
	{
		const ImVec2 ts = TextSize(Size, Str);
		Pos.x -= ts.x * Align;
		const float a = ImGui::ColorConvertU32ToFloat4(Color).w;
		dl->AddText(ImGui::GetFont(), Size, ImVec2(Pos.x + 2, Pos.y + 2), Col(0, 0, 0, 0.75f * a), Str);
		dl->AddText(ImGui::GetFont(), Size, Pos, Color, Str);
	}

	/// <summary>
	/// 減った分が遅れて追いかける(白い残像)ゲージ
	/// </summary>
	void Bar(ImDrawList* dl, ImVec2 Pos, ImVec2 Size, float Ratio, float Trail, ImU32 Fill, ImU32 FillTop, ImU32 Background, float Rounding)
	{
		Ratio = std::clamp(Ratio, 0.0f, 1.0f);
		Trail = std::clamp(Trail, 0.0f, 1.0f);
		const ImVec2 end = ImVec2(Pos.x + Size.x, Pos.y + Size.y);
		dl->AddRectFilled(ImVec2(Pos.x - 2, Pos.y - 2), ImVec2(end.x + 2, end.y + 2), Col(0, 0, 0, 0.55f), Rounding + 2);
		dl->AddRectFilled(Pos, end, Background, Rounding);
		if (Trail > Ratio)
		{
			dl->AddRectFilled(Pos, ImVec2(Pos.x + Size.x * Trail, end.y), Col(1.0f, 0.95f, 0.85f, 0.85f), Rounding);
		}
		if (Ratio > 0.0f)
		{
			dl->AddRectFilledMultiColor(Pos, ImVec2(Pos.x + Size.x * Ratio, end.y), FillTop, FillTop, Fill, Fill);
		}
		dl->AddRect(ImVec2(Pos.x - 1, Pos.y - 1), ImVec2(end.x + 1, end.y + 1), Col(1, 1, 1, 0.25f), Rounding, 0, 1.0f);
	}

	bool WorldToScreen(const Math::Vector3& World, ImVec2& Out)
	{
		if (System::Camera::Main == nullptr)
		{
			return false;
		}
		const Math::Matrix m = System::Camera::Main->GetView() * System::Camera::Main->GetProjection();
		const float x = World.x * m._11 + World.y * m._21 + World.z * m._31 + m._41;
		const float y = World.x * m._12 + World.y * m._22 + World.z * m._32 + m._42;
		const float w = World.x * m._14 + World.y * m._24 + World.z * m._34 + m._44;
		if (w <= 0.01f)
		{
			return false;
		}
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		Out = ImVec2((x / w * 0.5f + 0.5f) * display.x, (-y / w * 0.5f + 0.5f) * display.y);
		return true;
	}

	void ShowBanner(const char* Text, const char* Sub, ImU32 Color, float Life)
	{
		sBanner.Text = Text;
		sBanner.Sub = Sub;
		sBanner.Color = Color;
		sBanner.Age = 0.0f;
		sBanner.Life = Life;
	}

	entt::entity FindPlayer(entt::registry& Reg)
	{
		auto view = Reg.view<PlayerTag>();
		return view.empty() ? entt::null : view.front();
	}

	/// <summary>
	/// 表示する敵(プレイヤーに一番近い生存敵)
	/// </summary>
	entt::entity FindFocusEnemy(entt::registry& Reg)
	{
		const entt::entity player = FindPlayer(Reg);
		Math::Vector3 origin = { 0, 0, 0 };
		if (player != entt::null && Reg.all_of<Transform3D>(player))
		{
			origin = Reg.get<Transform3D>(player).Position;
		}
		entt::entity best = entt::null;
		float bestDistance = 1e9f;
		auto view = Reg.view<EnemyTag, Transform3D, StatusComponet>(entt::exclude<DeadTag>);
		view.each([&](entt::entity e, Transform3D& t, StatusComponet&)
			{
				const float d = (t.Position - origin).SqrLength();
				if (d < bestDistance)
				{
					bestDistance = d;
					best = e;
				}
			});
		return best;
	}
}

namespace Engine::UI
{
	void Hud::SetEnabled(bool Enabled)
	{
		sEnabled = Enabled;
		sFloating.clear();
		sMarkers.clear();
		sBanner.Age = 99.0f;
		sPlayerHpTrail = 1.0f;
		sEnemyHpTrail = 1.0f;
		sTrackedEnemy = entt::null;
		if (Enabled == false)
		{
			sObjective = nullptr;
		}
	}

	void Hud::SetObjective(const Objective* Current)
	{
		sObjective = Current;
	}

	void Hud::AddWorldLabel(const Math::Vector3& World, const std::string& Text, bool Highlight)
	{
		sLabels.push_back({ World, Text, Highlight });
	}

	void Hud::DrawWorldLabels()
	{
		ImDrawList* dl = ImGui::GetForegroundDrawList();
		const float s = Scale();
		for (const auto& label : sLabels)
		{
			ImVec2 p;
			if (WorldToScreen(label.World, p) == false)
			{
				continue;
			}
			const float size = (label.Highlight ? 30.0f : 22.0f) * s;
			const ImVec2 ts = TextSize(size, label.Text.c_str());
			const ImVec2 a = ImVec2(p.x - ts.x * 0.5f - 14 * s, p.y - 8 * s);
			const ImVec2 b = ImVec2(p.x + ts.x * 0.5f + 14 * s, p.y + ts.y + 8 * s);
			dl->AddRectFilled(a, b, Col(0.02f, 0.03f, 0.06f, label.Highlight ? 0.8f : 0.5f), 8 * s);
			if (label.Highlight)
			{
				dl->AddRect(a, b, Col(0.6f, 0.85f, 1.0f, 0.6f + 0.4f * std::sin(sTime * 5.0f)), 8 * s, 0, 2 * s);
			}
			Text(dl, p, size, label.Highlight ? Col(1, 1, 1) : Col(0.85f, 0.9f, 1.0f, 0.85f), label.Text.c_str(), 0.5f);
		}
		sLabels.clear();
	}

	void Hud::Draw(entt::registry& Reg, float DeltaTime)
	{
		sTime += DeltaTime;
		DrawWorldLabels();
		if (sEnabled == false)
		{
			return;
		}
		ConsumeEvents(Reg);
		DrawScreenEffects(Reg, DeltaTime);
		DrawFloatingTexts(DeltaTime);
		DrawEnemy(Reg, DeltaTime);
		DrawPlayer(Reg, DeltaTime);
		DrawSkills(Reg);
		DrawControls();
		DrawObjective();
	}

	void Hud::ConsumeEvents(entt::registry& Reg)
	{
		for (const auto& e : Combat::CombatEvents::Get())
		{
			switch (e.Type)
			{
			case CombatEventType::DamageToEnemy:
			{
				if (e.Value < 0.5f)
				{
					break;
				}
				FloatingText t;
				t.Text = std::to_string(static_cast<int>(std::round(e.Value)));
				t.World = e.Position;
				t.Color = e.Extra != 0 ? Col(1.0f, 0.82f, 0.3f) : Col(1.0f, 1.0f, 1.0f);
				t.Size = e.Extra != 0 ? 46.0f : 34.0f;
				t.Drift = (static_cast<float>(sFloating.size() % 5) - 2.0f) * 14.0f;
				sFloating.push_back(t);
				break;
			}
			case CombatEventType::DamageToPlayer:
				if (e.Value > 0.0f)
				{
					FloatingText t;
					t.Text = "-" + std::to_string(static_cast<int>(std::round(e.Value)));
					t.World = e.Position;
					t.Color = Col(1.0f, 0.35f, 0.3f);
					t.Size = 30.0f;
					sFloating.push_back(t);
				}
				break;
			case CombatEventType::Parry:
				ShowBanner("PARRY", "弾き", Col(0.65f, 0.85f, 1.0f), 0.9f);
				break;
			case CombatEventType::PostureBreak:
				ShowBanner("BREAK", "体幹崩し  ― 今が好機 ―", Col(1.0f, 0.8f, 0.35f), 1.4f);
				break;
			case CombatEventType::GuardBreak:
				ShowBanner("GUARD BREAK", "スタミナ切れ", Col(1.0f, 0.4f, 0.3f), 1.0f);
				break;
			case CombatEventType::EnemyKilled:
				ShowBanner("DEFEATED", "撃破", Col(1.0f, 0.9f, 0.7f), 2.0f);
				break;
			case CombatEventType::Telegraph:
				sMarkers.push_back({ e.Position, e.Extra != 0, 0.0f });
				break;
			default:
				break;
			}
		}
	}

	void Hud::DrawScreenEffects(entt::registry& Reg, float DeltaTime)
	{
		ImDrawList* dl = ImGui::GetForegroundDrawList();
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		const entt::entity player = FindPlayer(Reg);
		if (player == entt::null)
		{
			return;
		}

		float red = 0.0f;
		if (const auto* defense = Reg.try_get<DefenseComponent>(player))
		{
			red = (std::max)(red, defense->DamageFlashTimer / 0.5f * 0.55f);
		}
		if (auto* status = Reg.try_get<StatusComponet>(player))
		{
			const float ratio = status->GetHpRatio();
			if (ratio < 0.3f && ratio > 0.0f)
			{
				red = (std::max)(red, 0.18f + 0.12f * std::sin(sTime * 5.0f));
			}
		}
		if (red > 0.001f)
		{
			//	画面の縁を赤く(内側は透明)
			const float edge = display.y * 0.22f;
			const ImU32 c = Col(0.7f, 0.0f, 0.0f, red);
			const ImU32 z = Col(0.7f, 0.0f, 0.0f, 0.0f);
			dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(display.x, edge), c, c, z, z);
			dl->AddRectFilledMultiColor(ImVec2(0, display.y - edge), ImVec2(display.x, display.y), z, z, c, c);
			dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(edge, display.y), c, z, z, c);
			dl->AddRectFilledMultiColor(ImVec2(display.x - edge, 0), ImVec2(display.x, display.y), z, c, c, z);
		}

		//	バナー(PARRY / BREAK ...)
		if (sBanner.Age < sBanner.Life)
		{
			sBanner.Age += DeltaTime;
			const float t = sBanner.Age / sBanner.Life;
			const float pop = 1.0f + 0.35f * std::exp(-sBanner.Age * 14.0f);
			const float alpha = t < 0.75f ? 1.0f : 1.0f - (t - 0.75f) / 0.25f;
			const float s = Scale();
			const ImVec2 center = ImVec2(display.x * 0.5f, display.y * 0.36f);
			const float size = 84.0f * s * pop;
			//	背後の帯
			dl->AddRectFilledMultiColor(ImVec2(0, center.y - 18 * s), ImVec2(display.x * 0.5f, center.y + size + 30 * s),
				Col(0, 0, 0, 0), Col(0, 0, 0, 0.45f * alpha), Col(0, 0, 0, 0.45f * alpha), Col(0, 0, 0, 0));
			dl->AddRectFilledMultiColor(ImVec2(display.x * 0.5f, center.y - 18 * s), ImVec2(display.x, center.y + size + 30 * s),
				Col(0, 0, 0, 0.45f * alpha), Col(0, 0, 0, 0), Col(0, 0, 0, 0), Col(0, 0, 0, 0.45f * alpha));
			Text(dl, center, size, WithAlpha(sBanner.Color, alpha), sBanner.Text.c_str(), 0.5f);
			Text(dl, ImVec2(center.x, center.y + size * 0.95f), 26.0f * s, WithAlpha(IM_COL32_WHITE, alpha), sBanner.Sub.c_str(), 0.5f);
		}

		//	予告マーク(弾ける: 白い「!」 弾けない: 赤い「危」)
		for (auto& m : sMarkers)
		{
			m.Age += DeltaTime;
		}
		sMarkers.erase(std::remove_if(sMarkers.begin(), sMarkers.end(), [](const Marker& m) { return m.Age > 0.7f; }), sMarkers.end());
		for (const auto& m : sMarkers)
		{
			ImVec2 p;
			if (WorldToScreen(m.World, p) == false)
			{
				continue;
			}
			const float s = Scale();
			const float a = 1.0f - m.Age / 0.7f;
			const float r = (34.0f + 30.0f * std::exp(-m.Age * 10.0f)) * s;
			const ImU32 c = m.Parryable ? Col(1, 1, 1, a) : Col(1.0f, 0.15f, 0.1f, a);
			dl->AddCircleFilled(p, r, Col(0, 0, 0, 0.45f * a), 32);
			dl->AddCircle(p, r, c, 32, 3.0f * s);
			Text(dl, ImVec2(p.x, p.y - 26 * s), 44 * s, c, m.Parryable ? "!" : "危", 0.5f);
		}
	}

	void Hud::DrawFloatingTexts(float DeltaTime)
	{
		ImDrawList* dl = ImGui::GetForegroundDrawList();
		const float s = Scale();
		for (auto& t : sFloating)
		{
			t.Age += DeltaTime;
		}
		sFloating.erase(std::remove_if(sFloating.begin(), sFloating.end(), [](const FloatingText& t) { return t.Age > t.Life; }), sFloating.end());
		for (const auto& t : sFloating)
		{
			ImVec2 p;
			if (WorldToScreen(t.World, p) == false)
			{
				continue;
			}
			const float k = t.Age / t.Life;
			const float pop = 1.0f + 0.5f * std::exp(-t.Age * 18.0f);
			p.y -= 70.0f * s * k;
			p.x += t.Drift * s;
			const float alpha = k < 0.6f ? 1.0f : 1.0f - (k - 0.6f) / 0.4f;
			Text(dl, p, t.Size * s * pop, WithAlpha(t.Color, alpha), t.Text.c_str(), 0.5f);
		}
	}

	void Hud::DrawPlayer(entt::registry& Reg, float DeltaTime)
	{
		const entt::entity player = FindPlayer(Reg);
		if (player == entt::null)
		{
			return;
		}
		ImDrawList* dl = ImGui::GetForegroundDrawList();
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		const float s = Scale();
		const ImVec2 origin = ImVec2(60 * s, display.y - 150 * s);

		Text(dl, origin, 22 * s, Col(0.85f, 0.9f, 1.0f, 0.9f), "剣士");

		if (auto* status = Reg.try_get<StatusComponet>(player))
		{
			const float ratio = status->GetHpRatio();
			sPlayerHpTrail = (std::max)(ratio, sPlayerHpTrail - DeltaTime * 0.35f);
			if (sPlayerHpTrail < ratio) sPlayerHpTrail = ratio;
			const ImVec2 pos = ImVec2(origin.x, origin.y + 32 * s);
			Bar(dl, pos, ImVec2(520 * s, 20 * s), ratio, sPlayerHpTrail, Col(0.65f, 0.08f, 0.08f), Col(0.95f, 0.3f, 0.25f), Col(0.08f, 0.04f, 0.05f, 0.8f), 3 * s);
			char buf[64];
			snprintf(buf, sizeof(buf), "%d / %d", static_cast<int>(std::ceil(status->Hp)), static_cast<int>(status->MaxHp.GetCurrent()));
			Text(dl, ImVec2(pos.x + 528 * s, pos.y - 3 * s), 20 * s, Col(1, 1, 1, 0.9f), buf);
		}
		if (auto* stamina = Reg.try_get<StaminaComponent>(player))
		{
			const ImVec2 pos = ImVec2(origin.x, origin.y + 62 * s);
			const bool denied = stamina->DeniedFlashTimer > 0.0f && std::fmod(sTime, 0.15f) < 0.075f;
			const bool low = stamina->Value.Value < Combat::Tuning::DodgeStaminaCost;
			const ImU32 fill = denied ? Col(0.9f, 0.1f, 0.1f) : (low ? Col(0.6f, 0.45f, 0.1f) : Col(0.55f, 0.6f, 0.15f));
			const ImU32 top = denied ? Col(1.0f, 0.4f, 0.3f) : (low ? Col(0.95f, 0.7f, 0.25f) : Col(0.9f, 0.95f, 0.4f));
			Bar(dl, pos, ImVec2(380 * s, 10 * s), stamina->Value.Ratio(), 0.0f, fill, top, Col(0.06f, 0.06f, 0.04f, 0.8f), 2 * s);
			Text(dl, ImVec2(pos.x + 388 * s, pos.y - 6 * s), 16 * s, Col(1, 1, 1, 0.7f), "スタミナ");
		}
		if (auto* defense = Reg.try_get<DefenseComponent>(player))
		{
			if (defense->StunTimer > 0.0f)
			{
				Text(dl, ImVec2(origin.x, origin.y - 30 * s), 22 * s, Col(1.0f, 0.4f, 0.3f), "体勢を崩された！");
			}
			else if (defense->Holding)
			{
				Text(dl, ImVec2(origin.x, origin.y - 30 * s), 22 * s, Col(0.6f, 0.8f, 1.0f), "ガード中");
			}
		}
	}

	void Hud::DrawEnemy(entt::registry& Reg, float DeltaTime)
	{
		const entt::entity enemy = FindFocusEnemy(Reg);
		if (enemy == entt::null)
		{
			return;
		}
		if (enemy != sTrackedEnemy)
		{
			sTrackedEnemy = enemy;
			sEnemyHpTrail = 1.0f;
		}
		ImDrawList* dl = ImGui::GetForegroundDrawList();
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		const float s = Scale();
		const float width = 760 * s;
		const ImVec2 origin = ImVec2((display.x - width) * 0.5f, 54 * s);

		const auto* combat = Reg.try_get<EnemyCombatComponent>(enemy);
		Text(dl, ImVec2(display.x * 0.5f, origin.y - 4 * s), 28 * s, Col(1, 0.95f, 0.88f), combat != nullptr ? combat->DisplayName.c_str() : "敵", 0.5f);

		//	ロックオンの印
		const entt::entity player = FindPlayer(Reg);
		if (player != entt::null)
		{
			if (const auto* camera = Reg.try_get<CameraControlComponent>(player); camera != nullptr && camera->LockTarget == enemy)
			{
				ImVec2 p;
				if (WorldToScreen(Reg.get<Transform3D>(enemy).Position + Math::Vector3(0.0f, 4.5f, 0.0f), p))
				{
					const float r = (16.0f + 3.0f * std::sin(sTime * 6.0f)) * s;
					const ImU32 c = Col(1.0f, 0.95f, 0.8f, 0.9f);
					dl->AddQuad(ImVec2(p.x, p.y - r), ImVec2(p.x + r, p.y), ImVec2(p.x, p.y + r), ImVec2(p.x - r, p.y), c, 2.5f * s);
					dl->AddCircleFilled(p, 3.0f * s, c);
				}
			}
		}

		auto& status = Reg.get<StatusComponet>(enemy);
		const float ratio = status.GetHpRatio();
		sEnemyHpTrail = (std::max)(ratio, sEnemyHpTrail - DeltaTime * 0.3f);
		Bar(dl, ImVec2(origin.x, origin.y + 34 * s), ImVec2(width, 14 * s), ratio, sEnemyHpTrail,
			Col(0.6f, 0.1f, 0.08f), Col(0.95f, 0.35f, 0.25f), Col(0.07f, 0.03f, 0.03f, 0.8f), 2 * s);

		if (const auto* posture = Reg.try_get<PostureComponent>(enemy))
		{
			//	体幹ゲージは中央から左右に伸びる
			const float pr = posture->Value.Ratio();
			const bool broken = posture->Value.IsBroken();
			const float pw = 420 * s;
			const ImVec2 center = ImVec2(display.x * 0.5f, origin.y + 62 * s);
			const float h = 8 * s;
			dl->AddRectFilled(ImVec2(center.x - pw * 0.5f - 2, center.y - 2), ImVec2(center.x + pw * 0.5f + 2, center.y + h + 2), Col(0, 0, 0, 0.55f), 3 * s);
			const float half = pw * 0.5f * (broken ? 1.0f : pr);
			const float danger = std::clamp((pr - 0.6f) / 0.4f, 0.0f, 1.0f);
			ImU32 c = broken ? Col(1.0f, 0.85f, 0.3f, 0.6f + 0.4f * std::sin(sTime * 20.0f)) : Col(0.95f, 0.55f + 0.25f * (1 - danger), 0.15f);
			dl->AddRectFilled(ImVec2(center.x - half, center.y), ImVec2(center.x + half, center.y + h), c, 2 * s);
			dl->AddLine(ImVec2(center.x, center.y - 3 * s), ImVec2(center.x, center.y + h + 3 * s), Col(1, 1, 1, 0.6f), 2 * s);
			Text(dl, ImVec2(center.x + pw * 0.5f + 12 * s, center.y - 7 * s), 16 * s, Col(1, 0.85f, 0.6f, 0.85f), broken ? "崩れ" : "体幹");
		}
	}

	void Hud::DrawSkills(entt::registry& Reg)
	{
		const entt::entity player = FindPlayer(Reg);
		if (player == entt::null)
		{
			return;
		}
		const auto* slots = Reg.try_get<SkillSlotComponent>(player);
		if (slots == nullptr)
		{
			return;
		}
		ImDrawList* dl = ImGui::GetForegroundDrawList();
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		const float s = Scale();
		const float box = 78 * s;
		const char* keys[2] = { "Q", "E" };
		const ImU32 tint[2] = { Col(0.45f, 1.0f, 0.75f), Col(1.0f, 0.55f, 0.25f) };

		for (int i = 0; i < 2; i++)
		{
			const entt::entity skillEntity = slots->SkillSlots[i];
			if (Reg.valid(skillEntity) == false || Reg.all_of<SkillComponent>(skillEntity) == false)
			{
				continue;
			}
			const auto& skill = Reg.get<SkillComponent>(skillEntity);
			const ImVec2 p = ImVec2(display.x * 0.5f + (i == 0 ? -box - 10 * s : 10 * s), display.y - box - 60 * s);
			const ImVec2 e = ImVec2(p.x + box, p.y + box);
			const bool ready = skill.CooldownTimer <= 0.0f;

			dl->AddRectFilled(p, e, Col(0.03f, 0.04f, 0.07f, 0.75f), 10 * s);
			if (ready)
			{
				const float pulse = 0.5f + 0.5f * std::sin(sTime * 3.0f);
				dl->AddRect(ImVec2(p.x - 2, p.y - 2), ImVec2(e.x + 2, e.y + 2), WithAlpha(tint[i], 0.4f + 0.4f * pulse), 12 * s, 0, 3 * s);
			}
			else
			{
				//	クールダウン(下から埋まる)
				const float r = std::clamp(skill.CooldownTimer / (std::max)(skill.MaxCooldown, 0.01f), 0.0f, 1.0f);
				dl->AddRectFilled(ImVec2(p.x, p.y), ImVec2(e.x, p.y + box * r), Col(0, 0, 0, 0.55f), 10 * s);
				char cd[16];
				snprintf(cd, sizeof(cd), "%.1f", skill.CooldownTimer);
				Text(dl, ImVec2(p.x + box * 0.5f, p.y + box * 0.28f), 28 * s, Col(1, 1, 1, 0.9f), cd, 0.5f);
			}
			dl->AddRect(p, e, WithAlpha(tint[i], 0.7f), 10 * s, 0, 1.5f * s);
			Text(dl, ImVec2(p.x + 8 * s, p.y + 4 * s), 20 * s, Col(1, 1, 1, 0.95f), keys[i]);
			Text(dl, ImVec2(p.x + box * 0.5f, e.y + 4 * s), 18 * s, WithAlpha(tint[i], 0.95f), skill.DisplayName, 0.5f);
			if (ready)
			{
				Text(dl, ImVec2(p.x + box * 0.5f, p.y + box * 0.32f), 24 * s, WithAlpha(tint[i], 0.95f), i == 0 ? "風" : "炎", 0.5f);
			}
		}
	}

	void Hud::DrawControls()
	{
		ImDrawList* dl = ImGui::GetForegroundDrawList();
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		const float s = Scale();
		struct Row { const char* Key; const char* Action; };
		static const Row rows[] = {
			{ "左クリック / R2", "攻撃" },
			{ "右クリック / L2", "パリィ(長押しでガード)" },
			{ "Shift / A", "回避" },
			{ "Q E / L1 R1", "スキル" },
			{ "Tab / R3", "ロックオン" },
			{ "WASD / 左スティック", "移動" },
		};
		constexpr int RowCount = static_cast<int>(sizeof(rows) / sizeof(rows[0]));
		const float lineH = 26 * s;
		const float w = 360 * s;
		const ImVec2 origin = ImVec2(display.x - w - 40 * s, display.y - lineH * RowCount - 50 * s);
		dl->AddRectFilled(ImVec2(origin.x - 14 * s, origin.y - 10 * s), ImVec2(origin.x + w, origin.y + lineH * RowCount + 6 * s), Col(0, 0, 0, 0.35f), 8 * s);
		for (int i = 0; i < RowCount; i++)
		{
			const ImVec2 p = ImVec2(origin.x, origin.y + lineH * i);
			Text(dl, p, 18 * s, Col(0.75f, 0.85f, 1.0f, 0.95f), rows[i].Key);
			Text(dl, ImVec2(origin.x + w - 14 * s, p.y), 18 * s, Col(1, 1, 1, 0.9f), rows[i].Action, 1.0f);
		}
	}

	void Hud::DrawObjective()
	{
		if (sObjective == nullptr)
		{
			return;
		}
		ImDrawList* dl = ImGui::GetForegroundDrawList();
		const float s = Scale();
		const Objective& o = *sObjective;
		const ImVec2 p = ImVec2(40 * s, 120 * s);
		const float w = 520 * s;
		const float h = 168 * s;
		const float flash = std::clamp(o.CompleteFlash, 0.0f, 1.0f);

		dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), Col(0.02f, 0.03f, 0.06f, 0.72f), 10 * s);
		dl->AddRectFilled(p, ImVec2(p.x + 6 * s, p.y + h), Col(0.45f, 0.7f, 1.0f, 0.9f), 10 * s);
		if (flash > 0.0f)
		{
			dl->AddRect(p, ImVec2(p.x + w, p.y + h), Col(0.6f, 1.0f, 0.7f, flash), 10 * s, 0, 4 * s);
		}

		char head[64];
		snprintf(head, sizeof(head), "チュートリアル  %d / %d", o.Step, o.StepCount);
		Text(dl, ImVec2(p.x + 22 * s, p.y + 12 * s), 18 * s, Col(0.6f, 0.75f, 1.0f, 0.9f), head);
		Text(dl, ImVec2(p.x + 22 * s, p.y + 38 * s), 32 * s, flash > 0.0f ? Col(0.6f, 1.0f, 0.7f) : Col(1, 1, 1), o.Title.c_str());
		Text(dl, ImVec2(p.x + 22 * s, p.y + 82 * s), 19 * s, Col(0.9f, 0.9f, 0.9f, 0.95f), o.Body.c_str());

		if (o.KeyHint.empty() == false)
		{
			const ImVec2 kp = ImVec2(p.x + 22 * s, p.y + 122 * s);
			const ImVec2 ts = TextSize(20 * s, o.KeyHint.c_str());
			dl->AddRectFilled(ImVec2(kp.x - 8 * s, kp.y - 4 * s), ImVec2(kp.x + ts.x + 8 * s, kp.y + ts.y + 4 * s), Col(0.45f, 0.7f, 1.0f, 0.25f), 6 * s);
			Text(dl, kp, 20 * s, Col(0.75f, 0.9f, 1.0f), o.KeyHint.c_str());
		}
		if (o.ProgressMax > 0)
		{
			char prog[32];
			snprintf(prog, sizeof(prog), "%d / %d", (std::min)(o.Progress, o.ProgressMax), o.ProgressMax);
			Text(dl, ImVec2(p.x + w - 20 * s, p.y + 122 * s), 24 * s, Col(1, 1, 1), prog, 1.0f);
		}
	}
}
