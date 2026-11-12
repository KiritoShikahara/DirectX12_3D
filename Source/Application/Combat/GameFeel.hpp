#pragma once
#include"Math/Vector3/Vector3.h"

namespace Engine::Combat
{
	/// <summary>
	/// 手応えの演出(ヒットストップ・スローモーション・カメラ揺れ)
	/// ゲーム時間の倍率を返し、Application がデルタタイムに掛ける
	/// </summary>
	class GameFeel
	{
	public:
		/// <summary>
		/// ヒットストップ(ほぼ停止)。長い方が優先
		/// </summary>
		static void HitStop(float Seconds);

		/// <summary>
		/// スローモーション(実時間で Seconds 秒)
		/// </summary>
		static void SlowMotion(float Scale, float Seconds);

		/// <summary>
		/// カメラ揺れ。強い方が優先
		/// </summary>
		static void Shake(float Strength, float Seconds);

		/// <summary>
		/// 実時間で更新し、ゲーム時間の倍率を返す
		/// </summary>
		static float Update(float RealDeltaTime);

		/// <summary>
		/// 現在のカメラ揺れのオフセット
		/// </summary>
		static Math::Vector3 GetShakeOffset();

		static void Reset();

	private:
		static inline float sHitStopTimer = 0.0f;
		static inline float sSlowTimer = 0.0f;
		static inline float sSlowScale = 1.0f;
		static inline float sShakeTimer = 0.0f;
		static inline float sShakeDuration = 0.0f;
		static inline float sShakeStrength = 0.0f;
		static inline float sTime = 0.0f;
	};
}
