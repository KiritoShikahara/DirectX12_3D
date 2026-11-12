#pragma once
#include"System/Scene/IScene.hpp"
#include"Graphics/SkyBox/SkyBox.hpp"
#include"Application/UI/Hud.hpp"
#include<Plugin/entt/entt.hpp>
#include<memory>
#include<vector>
#include<functional>

namespace Engine::Scene
{
	/// <summary>
	/// チュートリアル: 修練の木人を相手に操作を1つずつ覚える
	/// 課題の内容は Document/GameDesign.md の「チュートリアルステージ」
	/// </summary>
	class Tutorial : public Engine::System::IScene
	{
	public:
		bool Initialize()override;
		//	戦闘イベント(DamageSystemのPostUpdateで出る)を読むのでPostUpdateで判定する
		void PostUpdate(double DeltaTime)override;
		void Render()override;
		void Release()override;
		std::string GetSceneName()override
		{
			return "Tutorial";
		}

	private:
		struct Step
		{
			const char* Title;
			const char* Body;
			const char* KeyHint;
			int Goal;
			std::function<void()> OnEnter;
		};

		void BuildSteps();
		void EnterStep(int Index);
		int CountProgress(float DeltaTime);

		std::unique_ptr<Graphics::SkyBox> mSkyBox;
		std::vector<Step> mSteps;
		int mCurrent = 0;
		int mProgress = 0;
		float mStepTime = 0.0f;
		float mAccumulator = 0.0f;
		float mFinishTimer = -1.0f;
		bool mUsedSkill[2] = { false, false };
		entt::entity mDummy = entt::null;
		UI::Objective mObjective;
	};
}
