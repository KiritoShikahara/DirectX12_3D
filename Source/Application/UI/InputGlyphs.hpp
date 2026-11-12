#pragma once
#include"System/Input/Manager/InputManager.hpp"
#include<string>
#include<initializer_list>

namespace Engine::UI
{
	/// <summary>
	/// 画面に出す操作
	/// </summary>
	enum class eAction
	{
		Attack,
		Guard,
		Parry,
		Dodge,
		Skill1,
		Skill2,
		LockOn,
		Move,
		Camera,
		Interact,
		Select,		//	決定
		Back,		//	戻る
		Choose,		//	左右で選ぶ
	};

	/// <summary>
	/// 最後に使った入力機器に合わせたボタン表記
	/// パッドは Windows.Gaming.Input の Xbox 配置の名前
	/// </summary>
	inline const char* KeyLabel(eAction Action)
	{
		const bool pad = Input::InputManager::GetInstance()->IsUsingGamepad();
		switch (Action)
		{
		case eAction::Attack:   return pad ? "RT" : "左クリック";
		case eAction::Guard:    return pad ? "LT 長押し" : "右クリック 長押し";
		case eAction::Parry:    return pad ? "LT" : "右クリック";
		case eAction::Dodge:    return pad ? "A" : "Shift";
		case eAction::Skill1:   return pad ? "LB" : "Q";
		case eAction::Skill2:   return pad ? "RB" : "E";
		case eAction::LockOn:   return pad ? "右スティック押し込み" : "Tab";
		case eAction::Move:     return pad ? "左スティック" : "WASD";
		case eAction::Camera:   return pad ? "右スティック" : "マウス";
		case eAction::Interact: return pad ? "X" : "F";
		case eAction::Select:   return pad ? "A" : "Space";
		case eAction::Back:     return pad ? "B" : "Esc";
		case eAction::Choose:   return pad ? "十字キー" : "A / D";
		}
		return "";
	}

	/// <summary>
	/// 複数の操作を「 / 」でつなぐ
	/// </summary>
	inline std::string KeyLabels(std::initializer_list<eAction> Actions)
	{
		std::string text;
		for (eAction a : Actions)
		{
			if (text.empty() == false)
			{
				text += " / ";
			}
			text += KeyLabel(a);
		}
		return text;
	}
}
