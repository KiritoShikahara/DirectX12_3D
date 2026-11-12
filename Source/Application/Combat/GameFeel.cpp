#include "pch.h"
#include "GameFeel.hpp"
#include <cmath>
#include <algorithm>

namespace Engine::Combat
{
	void GameFeel::HitStop(float Seconds)
	{
		sHitStopTimer = (std::max)(sHitStopTimer, Seconds);
	}

	void GameFeel::SlowMotion(float Scale, float Seconds)
	{
		if (sSlowTimer <= 0.0f || Scale < sSlowScale)
		{
			sSlowScale = Scale;
		}
		sSlowTimer = (std::max)(sSlowTimer, Seconds);
	}

	void GameFeel::Shake(float Strength, float Seconds)
	{
		const float current = (sShakeDuration > 0.0f) ? sShakeStrength * (sShakeTimer / sShakeDuration) : 0.0f;
		if (Strength >= current)
		{
			sShakeStrength = Strength;
			sShakeDuration = Seconds;
			sShakeTimer = Seconds;
		}
	}

	float GameFeel::Update(float RealDeltaTime)
	{
		sTime += RealDeltaTime;
		if (sShakeTimer > 0.0f)
		{
			sShakeTimer = (std::max)(0.0f, sShakeTimer - RealDeltaTime);
		}

		if (sHitStopTimer > 0.0f)
		{
			sHitStopTimer -= RealDeltaTime;
			//	完全に止めるとアニメーションの補間が固まって見えるので僅かに進める
			return 0.02f;
		}
		if (sSlowTimer > 0.0f)
		{
			sSlowTimer -= RealDeltaTime;
			//	終わり際は元の速度へ滑らかに戻す
			const float blend = (std::min)(1.0f, sSlowTimer / 0.15f);
			return 1.0f + (sSlowScale - 1.0f) * blend;
		}
		return 1.0f;
	}

	Math::Vector3 GameFeel::GetShakeOffset()
	{
		if (sShakeTimer <= 0.0f || sShakeDuration <= 0.0f)
		{
			return Math::Vector3(0.0f, 0.0f, 0.0f);
		}
		const float fade = sShakeTimer / sShakeDuration;
		const float s = sShakeStrength * fade * fade;
		//	周波数の違う sin を重ねた疑似ノイズ
		return Math::Vector3(
			s * (std::sin(sTime * 61.0f) * 0.6f + std::sin(sTime * 97.0f) * 0.4f),
			s * (std::sin(sTime * 73.0f + 1.3f) * 0.6f + std::sin(sTime * 113.0f) * 0.4f),
			s * 0.3f * std::sin(sTime * 89.0f + 2.1f));
	}

	void GameFeel::Reset()
	{
		sHitStopTimer = 0.0f;
		sSlowTimer = 0.0f;
		sSlowScale = 1.0f;
		sShakeTimer = 0.0f;
		sShakeDuration = 0.0f;
		sShakeStrength = 0.0f;
	}
}
