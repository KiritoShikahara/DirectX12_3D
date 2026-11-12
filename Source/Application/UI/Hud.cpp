#include "pch.h"
#include "Hud.hpp"

#include"Application/UI/UiTheme.hpp"
#include"Application/UI/InputGlyphs.hpp"
#include"Application/Combat/CombatComponents.hpp"
#include"Application/Combat/CombatEvents.hpp"
#include"Application/Components/Status/StatusComponent.hpp"
#include"Application/Components/Tag/TagComponent.hpp"
#include"Application/Components/Skill/SkillComponent.hpp"
#include"Application/Components/CameraWork/CameraControl.hpp"
#include"System/Conponent/Transform/TransformConponent.hpp"
#include"System/Camera/Camera.hpp"
#include"System/Input/Manager/InputManager.hpp"

#include<algorithm>
#include<cmath>
#include<vector>

// =====================================================================
//  戦闘HUD
//  配置: 自分の状態=左上 / 敵の体力と体幹=下中央 / スキル=右下 / 操作=左下(必要な時だけ)
//  見た目の部品は UiTheme にまとめている
// =====================================================================
namespace
{
	using namespace Engine;
	using namespace Engine::System;
	using namespace Engine::UI::Theme;
	using Engine::UI::eAction;
	using Engine::UI::KeyLabel;
	using Combat::CombatEventType;

	constexpr float ControlsShowSeconds = 10.0f;	//	操作ガイドを出しておく時間
	constexpr float ControlsFadeSeconds = 1.5f;

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
		eFont Font = eFont::Display;
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
	float sControlsTimer = 0.0f;
	bool sLastWasPad = false;

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

	void ShowBanner(const char* Text, const char* Sub, eFont Font, ImU32 Color, float Life)
	{
		sBanner = { Text, Sub, Font, Color, 0.0f, Life };
	}

	entt::entity FindPlayer(entt::registry& Reg)
	{
		auto view = Reg.view<PlayerTag>();
		return view.empty() ? entt::null : view.front();
	}

	/// <summary>
	/// 表示する敵(ロックオン中の敵、無ければ一番近い生存敵)
	/// </summary>
	entt::entity FindFocusEnemy(entt::registry& Reg)
	{
		const entt::entity player = FindPlayer(Reg);
		Math::Vector3 origin = { 0, 0, 0 };
		if (player != entt::null)
		{
			if (const auto* camera = Reg.try_get<CameraControlComponent>(player);
				camera != nullptr && camera->LockTarget != entt::null && Reg.valid(camera->LockTarget))
			{
				return camera->LockTarget;
			}
			if (Reg.all_of<Transform3D>(player))
			{
				origin = Reg.get<Transform3D>(player).Position;
			}
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

	float EaseOutCubic(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
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
		sControlsTimer = 0.0f;
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
			if (label.Highlight)
			{
				//	「[キー] 名前」を金の細線で受ける
				const float h = 34 * s;
				const std::string key = KeyLabel(eAction::Interact);
				const float w = PromptWidth(h, key.c_str(), label.Text.c_str());
				Band(dl, p.y + h * 0.5f, h * 1.1f, 0.7f);
				Prompt(dl, ImVec2(p.x - w * 0.5f, p.y), h, key.c_str(), label.Text.c_str());
				FadeLine(dl, ImVec2(p.x, p.y + h + 8 * s), w * 0.65f, Gold(0.8f), 1.0f);
			}
			else
			{
				Diamond(dl, ImVec2(p.x, p.y - 12 * s), 4 * s, Gold(0.8f), true);
				Text(dl, eFont::Mincho, p, 22 * s, Ivory(0.8f), label.Text.c_str(), 0.5f, 2 * s);
			}
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
				t.Color = e.Extra != 0 ? Gold() : Ivory();
				t.Size = e.Extra != 0 ? 44.0f : 30.0f;
				t.Drift = (static_cast<float>(sFloating.size() % 5) - 2.0f) * 16.0f;
				sFloating.push_back(t);
				break;
			}
			case CombatEventType::DamageToPlayer:
				if (e.Value > 0.0f)
				{
					FloatingText t;
					t.Text = std::to_string(static_cast<int>(std::round(e.Value)));
					t.World = e.Position;
					t.Color = CrimsonLight();
					t.Size = 26.0f;
					sFloating.push_back(t);
				}
				break;
			case CombatEventType::Parry:
				ShowBanner("PARRY", "弾 き", eFont::Display, Moon(), 0.9f);
				break;
			case CombatEventType::PostureBreak:
				ShowBanner("BREAK", "体 幹 崩 し", eFont::Display, Gold(), 1.4f);
				break;
			case CombatEventType::GuardBreak:
				ShowBanner("GUARD BROKEN", "防 御 を 崩 さ れ た", eFont::Display, Vermilion(), 1.1f);
				break;
			case CombatEventType::EnemyKilled:
				ShowBanner("敵を討ち果たした", "ENEMY  FELLED", eFont::Mincho, Gold(), 2.6f);
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
		const float s = Scale();
		const entt::entity player = FindPlayer(Reg);
		if (player == entt::null)
		{
			return;
		}

		//	被弾・瀕死で画面の縁を赤く
		float red = 0.0f;
		if (const auto* defense = Reg.try_get<DefenseComponent>(player))
		{
			red = (std::max)(red, defense->DamageFlashTimer / 0.5f * 0.5f);
		}
		if (auto* status = Reg.try_get<StatusComponet>(player))
		{
			const float ratio = status->GetHpRatio();
			if (ratio < 0.3f && ratio > 0.0f)
			{
				red = (std::max)(red, 0.16f + 0.1f * std::sin(sTime * 4.0f));
			}
		}
		if (red > 0.001f)
		{
			const float edge = display.y * 0.24f;
			const ImU32 c = Rgba(0.45f, 0.0f, 0.02f, red);
			const ImU32 z = Rgba(0.45f, 0.0f, 0.02f, 0.0f);
			dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(display.x, edge), c, c, z, z);
			dl->AddRectFilledMultiColor(ImVec2(0, display.y - edge), ImVec2(display.x, display.y), z, z, c, c);
			dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(edge, display.y), c, z, z, c);
			dl->AddRectFilledMultiColor(ImVec2(display.x - edge, 0), ImVec2(display.x, display.y), z, c, c, z);
		}

		//	バナー: 帯 → 文字が字間を詰めながら現れる → 消える
		if (sBanner.Age < sBanner.Life)
		{
			sBanner.Age += DeltaTime;
			const float t = sBanner.Age / sBanner.Life;
			const float in = EaseOutCubic(sBanner.Age / 0.25f);
			const float alpha = (t < 0.7f ? 1.0f : 1.0f - (t - 0.7f) / 0.3f) * in;
			const float cy = display.y * 0.40f;
			const bool display_ = sBanner.Font == eFont::Display;
			const float size = (display_ ? 88.0f : 64.0f) * s;
			const float tracking = (display_ ? 14.0f : 6.0f) * s + (1.0f - in) * 30.0f * s;

			Band(dl, cy, size * 0.95f, alpha);
			const ImVec4 c = ImGui::ColorConvertU32ToFloat4(sBanner.Color);
			GlowText(dl, sBanner.Font, ImVec2(display.x * 0.5f, cy - size * 0.62f), size,
				Rgba(c.x, c.y, c.z, alpha), Rgba(c.x, c.y, c.z, 0.12f * alpha), sBanner.Text.c_str(), 0.5f, tracking);
			FadeLine(dl, ImVec2(display.x * 0.5f, cy + size * 0.52f), 360 * s * in, Rgba(c.x, c.y, c.z, 0.7f * alpha), 1.2f);
			Text(dl, display_ ? eFont::Mincho : eFont::Display, ImVec2(display.x * 0.5f, cy + size * 0.6f), 22 * s,
				Ivory(0.9f * alpha), sBanner.Sub.c_str(), 0.5f, 3 * s);
		}

		//	攻撃の予告: 弾ける=白い菱形の閃光 / 弾けない=朱の「危」
		for (auto& m : sMarkers)
		{
			m.Age += DeltaTime;
		}
		sMarkers.erase(std::remove_if(sMarkers.begin(), sMarkers.end(), [](const Marker& m) { return m.Age > 0.75f; }), sMarkers.end());
		for (const auto& m : sMarkers)
		{
			ImVec2 p;
			if (WorldToScreen(m.World, p) == false)
			{
				continue;
			}
			const float a = 1.0f - m.Age / 0.75f;
			const float pop = 1.0f + 0.6f * std::exp(-m.Age * 12.0f);
			if (m.Parryable)
			{
				const float r = 14.0f * s * pop;
				Diamond(dl, p, r * 2.2f, Ivory(0.12f * a), true);
				Diamond(dl, p, r, Ivory(0.95f * a), true);
				Diamond(dl, p, r * 1.6f, Moon(0.8f * a), false, 2.0f * s);
				dl->AddLine(ImVec2(p.x - r * 3.5f, p.y), ImVec2(p.x + r * 3.5f, p.y), Ivory(0.6f * a), 1.5f * s);
			}
			else
			{
				const float size = 64.0f * s * pop;
				GlowText(dl, eFont::Mincho, ImVec2(p.x, p.y - size * 0.6f), size, Vermilion(a), Vermilion(0.25f * a), "危", 0.5f);
			}
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
			const float pop = 1.0f + 0.45f * std::exp(-t.Age * 20.0f);
			p.y -= 60.0f * s * EaseOutCubic(k);
			p.x += t.Drift * s;
			const float alpha = k < 0.55f ? 1.0f : 1.0f - (k - 0.55f) / 0.45f;
			const ImVec4 c = ImGui::ColorConvertU32ToFloat4(t.Color);
			Text(dl, eFont::Display, p, t.Size * s * pop, Rgba(c.x, c.y, c.z, alpha), t.Text.c_str(), 0.5f);
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
		const float s = Scale();
		const ImVec2 origin = ImVec2(70 * s, 58 * s);

		if (auto* status = Reg.try_get<StatusComponet>(player))
		{
			const float ratio = status->GetHpRatio();
			sPlayerHpTrail = (std::max)(ratio, sPlayerHpTrail - DeltaTime * 0.3f);
			Gauge(dl, origin, ImVec2(460 * s, 11 * s), ratio, sPlayerHpTrail, Crimson(), CrimsonLight());
			char buf[32];
			snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::ceil(status->Hp)));
			Text(dl, eFont::Display, ImVec2(origin.x + 482 * s, origin.y - 8 * s), 22 * s, Ivory(0.9f), buf);
		}
		if (auto* stamina = Reg.try_get<StaminaComponent>(player))
		{
			const bool denied = stamina->DeniedFlashTimer > 0.0f && std::fmod(sTime, 0.16f) < 0.08f;
			const bool low = stamina->Value.Value < Combat::Tuning::DodgeStaminaCost;
			const ImU32 fill = denied ? Vermilion() : (low ? Rgba(0.45f, 0.32f, 0.12f, 1) : Rgba(0.42f, 0.46f, 0.22f, 1));
			const ImU32 bright = denied ? Vermilion() : (low ? Gold() : Rgba(0.78f, 0.82f, 0.45f, 1));
			Gauge(dl, ImVec2(origin.x, origin.y + 24 * s), ImVec2(320 * s, 6 * s), stamina->Value.Ratio(), 0.0f, fill, bright, false);
		}
		if (auto* defense = Reg.try_get<DefenseComponent>(player))
		{
			const ImVec2 p = ImVec2(origin.x, origin.y + 40 * s);
			if (defense->StunTimer > 0.0f)
			{
				Text(dl, eFont::Mincho, p, 20 * s, Vermilion(), "体勢を崩された", 0.0f, 2 * s);
			}
			else if (defense->Holding)
			{
				Diamond(dl, ImVec2(p.x + 6 * s, p.y + 11 * s), 5 * s, Moon(0.9f), false, 1.5f);
				Text(dl, eFont::Mincho, ImVec2(p.x + 18 * s, p.y), 20 * s, Moon(0.95f), "構え", 0.0f, 2 * s);
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
		const float width = 900 * s;
		const float cx = display.x * 0.5f;
		const float baseY = display.y - 120 * s;

		//	ロックオンの印(小さな菱形と輪)
		const entt::entity player = FindPlayer(Reg);
		if (player != entt::null)
		{
			if (const auto* camera = Reg.try_get<CameraControlComponent>(player); camera != nullptr && camera->LockTarget == enemy)
			{
				ImVec2 p;
				if (WorldToScreen(Reg.get<Transform3D>(enemy).Position + Math::Vector3(0.0f, 4.5f, 0.0f), p))
				{
					const float r = (11.0f + 2.0f * std::sin(sTime * 5.0f)) * s;
					Diamond(dl, p, r, Ivory(0.85f), false, 1.5f * s);
					Diamond(dl, p, 3.0f * s, Ivory(0.95f), true);
				}
			}
		}

		//	体幹(中央から左右に伸びる)
		if (const auto* posture = Reg.try_get<PostureComponent>(enemy))
		{
			const float pr = posture->Value.Ratio();
			const bool broken = posture->Value.IsBroken();
			const float half = 230 * s;
			const float y = baseY - 92 * s;
			const float h = 6 * s;
			dl->AddRectFilled(ImVec2(cx - half, y), ImVec2(cx + half, y + h), Ink(0.7f));
			const float danger = std::clamp((pr - 0.55f) / 0.45f, 0.0f, 1.0f);
			const float fill = half * (broken ? 1.0f : pr);
			const ImU32 inner = broken ? Gold(0.7f + 0.3f * std::sin(sTime * 18.0f)) : Rgba(0.95f, 0.75f - 0.45f * danger, 0.3f - 0.2f * danger, 1.0f);
			const ImU32 outer = broken ? Gold() : Rgba(0.75f, 0.5f - 0.3f * danger, 0.15f, 1.0f);
			dl->AddRectFilledMultiColor(ImVec2(cx - fill, y), ImVec2(cx, y + h), outer, inner, inner, outer);
			dl->AddRectFilledMultiColor(ImVec2(cx, y), ImVec2(cx + fill, y + h), inner, outer, outer, inner);
			dl->AddRect(ImVec2(cx - half - 1, y - 1), ImVec2(cx + half + 1, y + h + 1), GoldDim(0.7f));
			Diamond(dl, ImVec2(cx, y + h * 0.5f), 6 * s, broken ? Gold() : Ink(0.95f), true);
			Diamond(dl, ImVec2(cx, y + h * 0.5f), 6 * s, Gold(0.9f), false, 1.2f);
		}

		//	名前と体力
		const auto* combat = Reg.try_get<EnemyCombatComponent>(enemy);
		const char* name = combat != nullptr ? combat->DisplayName.c_str() : "";
		Text(dl, eFont::Mincho, ImVec2(cx - width * 0.5f, baseY - 52 * s), 30 * s, Ivory(0.95f), name, 0.0f, 4 * s);

		auto& status = Reg.get<StatusComponet>(enemy);
		const float ratio = status.GetHpRatio();
		sEnemyHpTrail = (std::max)(ratio, sEnemyHpTrail - DeltaTime * 0.3f);
		Gauge(dl, ImVec2(cx - width * 0.5f, baseY), ImVec2(width, 10 * s), ratio, sEnemyHpTrail, Crimson(), CrimsonLight());
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
		const float r = 40 * s;
		const eAction keys[2] = { eAction::Skill1, eAction::Skill2 };
		const char* glyph[2] = { "風", "炎" };
		const ImU32 tint[2] = { Rgba(0.55f, 0.95f, 0.8f, 1), Rgba(1.0f, 0.55f, 0.3f, 1) };

		for (int i = 0; i < 2; i++)
		{
			const entt::entity skillEntity = slots->SkillSlots[i];
			if (Reg.valid(skillEntity) == false || Reg.all_of<SkillComponent>(skillEntity) == false)
			{
				continue;
			}
			const auto& skill = Reg.get<SkillComponent>(skillEntity);
			const ImVec2 c = ImVec2(display.x - (250 - i * 110) * s, display.y - 125 * s);
			const bool ready = skill.CooldownTimer <= 0.0f;
			const ImVec4 t = ImGui::ColorConvertU32ToFloat4(tint[i]);

			Diamond(dl, c, r, Ink(0.8f), true);
			if (ready)
			{
				const float pulse = 0.5f + 0.5f * std::sin(sTime * 3.0f + i);
				Diamond(dl, c, r * (1.12f + 0.04f * pulse), Rgba(t.x, t.y, t.z, 0.25f + 0.35f * pulse), false, 2.0f * s);
				GlowText(dl, eFont::Mincho, ImVec2(c.x, c.y - 19 * s), 36 * s, Rgba(t.x, t.y, t.z, 1), Rgba(t.x, t.y, t.z, 0.15f), glyph[i], 0.5f);
			}
			else
			{
				const float remain = std::clamp(skill.CooldownTimer / (std::max)(skill.MaxCooldown, 0.01f), 0.0f, 1.0f);
				RadialFill(dl, c, r * 0.62f, remain, Ink(0.55f));
				char cd[16];
				snprintf(cd, sizeof(cd), "%.0f", std::ceil(skill.CooldownTimer));
				Text(dl, eFont::Display, ImVec2(c.x, c.y - 14 * s), 26 * s, Ivory(0.95f), cd, 0.5f);
			}
			Diamond(dl, c, r, ready ? Gold(0.95f) : GoldDim(0.8f), false, 1.5f * s);

			const char* key = KeyLabel(keys[i]);
			const float h = 24 * s;
			const float kw = PromptWidth(h, key, "") - h * 0.35f;
			KeyChip(dl, ImVec2(c.x - kw * 0.5f, c.y + r + 10 * s), h, key);
			Text(dl, eFont::Mincho, ImVec2(c.x, c.y + r + 40 * s), 18 * s, Ivory(0.75f), skill.DisplayName, 0.5f, 3 * s);
		}
	}

	void Hud::DrawControls()
	{
		//	開始直後と入力機器を持ち替えた時だけ出す(慣れたら邪魔にならないように)
		const bool pad = Input::InputManager::GetInstance()->IsUsingGamepad();
		if (pad != sLastWasPad)
		{
			sLastWasPad = pad;
			sControlsTimer = 0.0f;
		}
		sControlsTimer += ImGui::GetIO().DeltaTime;
		const float alpha = 1.0f - std::clamp((sControlsTimer - ControlsShowSeconds) / ControlsFadeSeconds, 0.0f, 1.0f);
		if (alpha <= 0.0f)
		{
			return;
		}

		ImDrawList* dl = ImGui::GetForegroundDrawList();
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		const float s = Scale();
		struct Row { eAction Key; const char* Action; };
		const Row rows[] = {
			{ eAction::Attack, "攻撃" },
			{ eAction::Parry, "弾き・長押しで構え" },
			{ eAction::Dodge, "回避" },
			{ eAction::LockOn, "ロックオン" },
		};
		const float h = 28 * s;
		float x = 70 * s;
		const float y = display.y - 72 * s;
		for (const auto& row : rows)
		{
			x += Prompt(dl, ImVec2(x, y), h, KeyLabel(row.Key), row.Action, alpha) + 30 * s;
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
		const ImVec2 p = ImVec2(70 * s, 150 * s);
		const float flash = std::clamp(o.CompleteFlash, 0.0f, 1.0f);

		//	左の縦線と見出し
		dl->AddRectFilledMultiColor(ImVec2(p.x - 18 * s, p.y), ImVec2(p.x - 16 * s, p.y + 150 * s), Gold(0.9f), Gold(0.9f), Gold(0.0f), Gold(0.0f));
		char head[64];
		snprintf(head, sizeof(head), "TRAINING   %d / %d", o.Step, o.StepCount);
		Text(dl, eFont::Display, p, 16 * s, Gold(0.9f), head, 0.0f, 3 * s);

		const ImU32 titleColor = flash > 0.0f ? Rgba(1.0f, 0.93f, 0.7f, 1.0f) : Ivory();
		GlowText(dl, eFont::Mincho, ImVec2(p.x, p.y + 26 * s), 36 * s, titleColor, Gold(0.08f + 0.3f * flash), o.Title.c_str(), 0.0f, 4 * s);
		Text(dl, eFont::Mincho, ImVec2(p.x, p.y + 76 * s), 20 * s, Ivory(0.85f), o.Body.c_str());

		if (o.KeyHint.empty() == false)
		{
			KeyChip(dl, ImVec2(p.x, p.y + 110 * s), 28 * s, o.KeyHint.c_str());
		}
		//	進み具合は菱形の列(色だけに頼らず塗り/枠で区別)
		if (o.ProgressMax > 0)
		{
			for (int i = 0; i < o.ProgressMax; i++)
			{
				const ImVec2 c = ImVec2(p.x + 8 * s + i * 22 * s, p.y + 160 * s);
				const bool done = i < o.Progress;
				Diamond(dl, c, 7 * s, done ? Gold() : Ink(0.6f), true);
				Diamond(dl, c, 7 * s, Gold(0.9f), false, 1.2f);
			}
		}
	}
}
