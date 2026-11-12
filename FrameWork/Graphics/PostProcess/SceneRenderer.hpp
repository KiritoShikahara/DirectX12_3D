#pragma once
#include"System/Singleton/Singleton.hpp"
#include"Graphics/DirectX/TypeAlias/DirectX12TypeAlias.hpp"
#include"Graphics/GraphicsDescriptorHeap/GraphicsDescriptorHeapInfo.hpp"
#include"Graphics/Color/Color.hpp"
#include"Math/Vector3/Vector3.h"
#include"Math/Matrix/Matrix.h"
#include<array>

namespace Engine::Graphics
{
	class Texture;

	/// <summary>
	/// シーンのライティング・ポストエフェクト設定(シーンごとに調整可能)
	/// </summary>
	struct SceneLighting
	{
		Math::Vector3 SunDirection = { -0.5f, 0.6f, -0.6f };	//	光源へ向かう方向
		Color SunColor = { 1.0f, 0.93f, 0.85f, 1.0f };
		float SunIntensity = 4.6f;
		Color AmbientSky = { 0.16f, 0.19f, 0.32f, 1.0f };
		Color AmbientGround = { 0.06f, 0.05f, 0.05f, 1.0f };
		float IBLIntensity = 0.9f;
		Color RimColor = { 0.45f, 0.65f, 1.4f, 1.0f };

		bool ShadowEnabled = true;
		float ShadowRadius = 26.0f;		//	影を落とす範囲(半径)
		float ShadowStrength = 0.92f;
		float ShadowBias = 0.0012f;

		Color FogColor = { 0.10f, 0.13f, 0.26f, 1.0f };
		float FogDensity = 0.012f;
		float FogStart = 12.0f;
		float FogHeightFalloff = 0.05f;
		float FogBaseHeight = 0.0f;
		float FogMaxOpacity = 0.85f;
		float FogSunScatter = 1.5f;

		bool AOEnabled = true;
		float AORadius = 0.9f;
		float AOIntensity = 1.1f;

		float BloomThreshold = 1.0f;
		float BloomKnee = 0.6f;
		float BloomIntensity = 0.6f;

		float Exposure = 1.2f;
		float Saturation = 1.08f;
		float Contrast = 1.06f;
		float Vignette = 0.35f;
		Color Gain = { 1.0f, 0.99f, 1.02f, 1.0f };
		float Grain = 0.012f;
	};

	/// <summary>
	/// 3D描画フレームの管理
	///  1. シャドウマップ  2. HDRシーン  3. ポストエフェクト(SSAO/Bloom/Fog/ACES/FXAA) -> バックバッファ
	/// </summary>
	class SceneRenderer : public Singleton<SceneRenderer>
	{
		GENERATE_SINGLETON_BODY(SceneRenderer);
		void OnCreate()override;
		void OnDestory()override;
	public:
		bool Initialize(UINT Width, UINT Height);

		/// <summary>
		/// フレーム開始。カメラ・ライトから定数を更新
		/// </summary>
		void BeginFrame(double Time);

		void BeginShadowPass();
		void BeginScenePass();

		/// <summary>
		/// ポストエフェクトを掛けてバックバッファへ出力し、UI用にバックバッファを設定する
		/// </summary>
		void EndScenePass();

		/// <summary>
		/// FBX描画用のシーンリソースをルートパラメータ 2,3,4 に設定
		/// </summary>
		void BindFbxSceneResources();

		/// <summary>
		/// 法線マップ/ARMテクスチャ(ルートパラメータ5)を設定。無い場合はnullを設定
		/// </summary>
		void BindSurfaceTextures(const DescriptorHeapInfo* Table);

		/// <summary>
		/// テクスチャのSRVをテーブルの指定スロットへ書き込む(全ミップ)
		/// </summary>
		static void WriteTextureSrv(const DescriptorHeapInfo& Table, UINT Slot, ID3D12Resource* Resource);

		/// <summary>
		/// シャドウ描画パイプラインの設定
		/// </summary>
		void BindShadowPipeline();

		/// <summary>
		/// 環境マップ(スカイボックス)の登録。nullptrで解除
		/// </summary>
		void SetEnvironment(Texture* CubeMap);

		/// <summary>
		/// 影を落とす範囲の中心(通常はプレイヤー)
		/// </summary>
		void SetShadowFocus(const Math::Vector3& Center);

		SceneLighting& GetLighting() { return mLighting; }
		void ResetLighting() { mLighting = SceneLighting(); }
		bool IsInShadowPass()const { return mInShadowPass; }

		void ImGuiUpdate();

	private:
		struct RenderTarget
		{
			mw::ResourcePtr Resource;
			D3D12_CPU_DESCRIPTOR_HANDLE Rtv = {};
			D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			UINT Width = 0;
			UINT Height = 0;
			DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
		};

		bool CreateTarget(RenderTarget& Target, UINT Width, UINT Height, DXGI_FORMAT Format, UINT RtvIndex);
		bool CreateShadowMap();
		bool CreateRootSignatures();
		bool CreatePipelines();
		void CreateDescriptorTables();
		void WriteSrv(const DescriptorHeapInfo& Table, UINT Slot, ID3D12Resource* Resource, DXGI_FORMAT Format);
		void WriteNullSrvs(const DescriptorHeapInfo& Table);
		void Transition(ID3D12Resource* Resource, D3D12_RESOURCE_STATES& Current, D3D12_RESOURCE_STATES Next);
		void Transition(RenderTarget& Target, D3D12_RESOURCE_STATES Next);
		void SetViewport(UINT Width, UINT Height);
		void DrawFullscreen(ID3D12PipelineState* Pipeline, RenderTarget& Output, const DescriptorHeapInfo& Inputs, float P0, float P1, float P2, float P3);
		Math::Matrix ComputeLightViewProjection()const;

	private:
		static constexpr UINT SHADOW_MAP_SIZE = 2048;
		static constexpr UINT BLOOM_LEVELS = 6;
		static constexpr UINT FRAME_COUNT = 2;
		static constexpr UINT CB_SLOT_SIZE = 512;

		SceneLighting mLighting;
		Math::Vector3 mShadowFocus = { 0, 0, 0 };
		bool mHasShadowFocus = false;
		bool mInShadowPass = false;
		bool mInitialized = false;
		bool mPostEnabled = true;

		UINT mWidth = 0;
		UINT mHeight = 0;

		RenderTarget mHdr;
		RenderTarget mLdr;
		RenderTarget mAO;
		std::array<RenderTarget, BLOOM_LEVELS> mBloom;

		mw::ResourcePtr mShadowMap;
		D3D12_RESOURCE_STATES mShadowState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		D3D12_RESOURCE_STATES mDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

		mw::DescriptorHeapPtr mRtvHeap;
		mw::DescriptorHeapPtr mDsvHeap;

		//	SRVテーブル(各4スロット)
		DescriptorHeapInfo mAOInputs = { -1, -1 };
		DescriptorHeapInfo mPrefilterInputs = { -1, -1 };
		std::array<DescriptorHeapInfo, BLOOM_LEVELS> mDownInputs;
		std::array<DescriptorHeapInfo, BLOOM_LEVELS> mUpInputs;
		DescriptorHeapInfo mCompositeInputs = { -1, -1 };
		DescriptorHeapInfo mFxaaInputs = { -1, -1 };
		DescriptorHeapInfo mShadowSrv = { -1, -1 };
		DescriptorHeapInfo mEnvironmentSrv = { -1, -1 };
		DescriptorHeapInfo mDefaultSurfaceSrv = { -1, -1 };
		float mEnvironmentMaxMip = 0.0f;

		mw::RootSignaturePtr mPostRootSignature;
		mw::RootSignaturePtr mShadowRootSignature;
		mw::PipelineStatePtr mSSAOPipeline;
		mw::PipelineStatePtr mPrefilterPipeline;
		mw::PipelineStatePtr mDownsamplePipeline;
		mw::PipelineStatePtr mUpsamplePipeline;
		mw::PipelineStatePtr mCompositePipeline;
		mw::PipelineStatePtr mFxaaPipeline;
		mw::PipelineStatePtr mShadowPipeline;

		//	フレームごとの定数バッファ(Upload, 常時Map)
		mw::ResourcePtr mConstantUpload;
		uint8_t* mConstantMapped = nullptr;
		D3D12_GPU_VIRTUAL_ADDRESS mSceneCB = 0;
		D3D12_GPU_VIRTUAL_ADDRESS mPostCB = 0;
	};
}
