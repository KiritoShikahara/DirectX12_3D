#include "pch.h"
#include "TitleScene.hpp"

#include"System/Input/Manager/InputManager.hpp"
#include"System/Scene/Manager/SceneManager.hpp"
#include"System/Entity/Manager/EntityManager.hpp"

#include"Application/Scene/InGame/InGameScene.hpp"
#include"Factory/CreateTitle.hpp"
#include"Application/Scene/StageSelect/StageSelectScene.hpp"
#include"Audio/Manager/AudioManager.hpp"
#include"Application/UI/UiTheme.hpp"
#include"Application/UI/InputGlyphs.hpp"
#include<algorithm>
#include<cmath>

/// <summary>
///	初期化
/// </summary>
/// <returns></returns>
bool Engine::Scene::Title::Initialize()
{
    //  背景
    CreateTitleObject::CreateBG();

    Audio::AudioManager::GetInstance()->PlaySE("Assets/Title/Sound/BGM.aud",true,1.0f,false);

    return true;
}

/// <summary>
/// 画面の切り替え判定
/// </summary>
/// <param name="FixedDeltaTime"></param>
void Engine::Scene::Title::PostUpdate(double FixedDeltaTime)
{
    mTime += static_cast<float>(FixedDeltaTime);
    if (GET_INPUT_MANAGER->IsActionPressed("Select"))
    {
        Audio::AudioManager::GetInstance()->PlaySE("Assets/Sound/Select.aud", false, 1.0f);
        Engine::System::SceneManager::GetInstance()->ChangeSceneFade<StageSelect>();
    }
}

/// <summary>
/// 操作表記(入力機器に合わせるので画像ではなく描画する)
/// </summary>
void Engine::Scene::Title::Render()
{
    using namespace UI::Theme;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float s = Scale();

    //  ロゴが光り終わる頃に現れ、ゆっくり呼吸する
    const float appear = std::clamp((mTime - 1.2f) / 1.0f, 0.0f, 1.0f);
    const float breathe = 0.65f + 0.35f * std::sin(mTime * 2.2f);
    const float alpha = appear * breathe;
    const float cy = display.y * 0.745f;

    //  背景が明るいので下敷きを二重にして読みやすくする
    Band(dl, cy, 64 * s, appear * 1.4f);
    const float h = 38 * s;
    const char* key = UI::KeyLabel(UI::eAction::Select);
    const char* text = "はじめる";
    const float w = PromptWidth(h, key, text) + 8 * s * 3;
    const float x = display.x * 0.5f - w * 0.5f;
    const float chip = KeyChip(dl, ImVec2(x, cy - h * 0.5f), h, key, alpha);
    GlowText(dl, eFont::Mincho, ImVec2(x + chip + h * 0.4f, cy - h * 0.46f), h * 0.85f, Ivory(alpha), Ink(0.5f * appear), text, 0.0f, 8 * s);
    FadeLine(dl, ImVec2(display.x * 0.5f, cy + h * 0.85f), 260 * s, Gold(0.7f * appear), 1.0f);
    Text(dl, eFont::Display, ImVec2(display.x * 0.5f, cy + h * 1.05f), 16 * s, GoldDim(appear), "PRESS TO BEGIN", 0.5f, 6 * s);
}

/// <summary>
/// 終了処理
/// </summary>
void Engine::Scene::Title::Release()
{
    //  ローカルオブジェクトの解放
    Engine::System::EntityManager::GetInstance()->ClearLocalEntities();
}
