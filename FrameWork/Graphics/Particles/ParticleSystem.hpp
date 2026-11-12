#pragma once
#include"System/Singleton/Singleton.hpp"
#include"Graphics/DirectX/TypeAlias/DirectX12TypeAlias.hpp"
#include"Graphics/Color/Color.hpp"
#include"Math/Vector3/Vector3.h"
#include<vector>
#include<deque>
#include<cstdint>

namespace Engine::Graphics
{
	/// <summary>
	/// パーティクルの形(シェーダーで生成するのでテクスチャ不要)
	/// </summary>
	enum class ParticleShape : uint32_t
	{
		SoftDot = 0,	//	丸い光
		Spark = 1,		//	速度方向に伸びる火花
		Ring = 2,		//	カメラを向く輪
		Flare = 3,		//	十字の閃光
		Smoke = 4,		//	煙(通常合成)
		GroundRing = 5,	//	地面に水平な輪(衝撃波)
	};

	struct Particle
	{
		Math::Vector3 Position;
		Math::Vector3 Velocity;
		float Life = 1.0f;
		float Age = 0.0f;
		float SizeStart = 1.0f;
		float SizeEnd = 1.0f;
		Color ColorStart = { 1, 1, 1, 1 };	//	HDR(1より大きい値で発光)
		Color ColorEnd = { 1, 1, 1, 0 };
		float Drag = 0.0f;			//	毎秒の減速率
		float Gravity = 0.0f;		//	下向き加速度
		float Stretch = 0.0f;		//	速度による伸び
		float Rotation = 0.0f;
		float RotationSpeed = 0.0f;
		ParticleShape Shape = ParticleShape::SoftDot;
		bool Additive = true;
	};

	/// <summary>
	/// 武器の軌跡(リボン)
	/// </summary>
	struct TrailHandle
	{
		int Index = -1;
		bool IsValid()const { return Index >= 0; }
	};

	class ParticleSystem : public Singleton<ParticleSystem>
	{
		GENERATE_SINGLETON_BODY(ParticleSystem);
		void OnCreate()override;
		void OnDestory()override;
	public:
		bool Initialize();

		void Spawn(const Particle& P);

		/// <summary>
		/// 軌跡の作成。Lifetime 秒前までの点を描く
		/// </summary>
		TrailHandle CreateTrail(const Color& TrailColor, float Lifetime);

		/// <summary>
		/// 軌跡の点(根本・先端)を追加。Emitting=false の間は点を足さずに消えていく
		/// </summary>
		void UpdateTrail(TrailHandle Handle, const Math::Vector3& Base, const Math::Vector3& Tip, bool Emitting);
		void SetTrailColor(TrailHandle Handle, const Color& TrailColor);

		void Update(float DeltaTime);

		/// <summary>
		/// HDRシーンパス中に呼ぶ
		/// </summary>
		void Render();

		/// <summary>
		/// シーン切り替え時に全消去
		/// </summary>
		void Clear();

		size_t GetAliveCount()const { return mParticles.size(); }

	private:
		bool CreatePipelines();

		struct TrailPoint
		{
			Math::Vector3 Base;
			Math::Vector3 Tip;
			float Time;
		};
		struct Trail
		{
			std::deque<TrailPoint> Points;
			Color TrailColor;
			float Lifetime = 0.15f;
			bool InUse = false;
		};

		struct GpuParticle
		{
			float Position[3];
			float Size;
			float Color[4];
			float Velocity[3];
			float Stretch;
			float Rotation;
			uint32_t Shape;
			float Age01;
			float Additive;
		};
		struct TrailVertex
		{
			float Position[3];
			float Color[4];
			float UV[2];
		};

		static constexpr size_t MAX_PARTICLES = 8192;
		static constexpr size_t MAX_TRAIL_VERTICES = 8192;
		static constexpr size_t MAX_TRAILS = 16;
		static constexpr UINT FRAME_COUNT = 2;

		std::vector<Particle> mParticles;
		std::vector<Trail> mTrails;
		float mTime = 0.0f;

		mw::RootSignaturePtr mRootSignature;
		mw::PipelineStatePtr mParticlePipeline;
		mw::PipelineStatePtr mTrailPipeline;
		mw::ResourcePtr mUpload;
		uint8_t* mMapped = nullptr;
		bool mInitialized = false;
	};
}
