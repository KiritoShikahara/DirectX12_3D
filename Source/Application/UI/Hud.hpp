#pragma once
#include<Plugin/entt/entt.hpp>
#include<string>
#include"Math/Vector3/Vector3.h"

namespace Engine::UI
{
	/// <summary>
	/// チュートリアルの課題表示
	/// </summary>
	struct Objective
	{
		std::string Title;		//	例: 「パリィ」
		std::string Body;		//	説明
		std::string KeyHint;	//	例: 「右クリック / L2」
		int Step = 0;			//	1始まり
		int StepCount = 0;
		int Progress = 0;
		int ProgressMax = 0;
		float CompleteFlash = 0.0f;	//	達成時の光
	};

	/// <summary>
	/// 戦闘HUD(ImGuiの前面描画リストに描く)
	/// </summary>
	class Hud
	{
	public:
		static void SetEnabled(bool Enabled);
		static void SetObjective(const Objective* Current);

		/// <summary>
		/// ワールド上の案内ラベル(毎フレーム追加する。HUDが無効でも表示)
		/// </summary>
		static void AddWorldLabel(const Math::Vector3& World, const std::string& Text, bool Highlight);

		/// <summary>
		/// 毎フレーム呼ぶ(ImGuiのフレーム中)
		/// </summary>
		static void Draw(entt::registry& Reg, float DeltaTime);

	private:
		static void DrawPlayer(entt::registry& Reg, float DeltaTime);
		static void DrawEnemy(entt::registry& Reg, float DeltaTime);
		static void DrawSkills(entt::registry& Reg);
		static void DrawControls();
		static void DrawObjective();
		static void DrawFloatingTexts(float DeltaTime);
		static void DrawScreenEffects(entt::registry& Reg, float DeltaTime);
		static void ConsumeEvents(entt::registry& Reg);
		static void DrawWorldLabels();
	};
}
