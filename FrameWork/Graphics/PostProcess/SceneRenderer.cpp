#include "pch.h"
#include "SceneRenderer.hpp"

#include"Debug/spdlog/Logger.hpp"
#include"Graphics/DirectX/DirectX12.hpp"
#include"Graphics/GraphicsDescriptorHeap/GraphicsDescriptorHeapManager.hpp"
#include"Graphics/Renderer/Renderer.hpp"
#include"Graphics/Texture/Texture.hpp"
#include"System/Camera/Camera.hpp"
#include"Engine/LaunchOptions.hpp"

#include<d3dcompiler.h>
#include<cmath>

namespace
{
	struct Float4
	{
		float x, y, z, w;
	};

	//	HLSL の SceneBuffer(b1) と同じ並び
	struct SceneConstants
	{
		Math::Matrix LightViewProj;
		Float4 CameraPosition;
		Float4 LightDirection;
		Float4 LightColor;
		Float4 AmbientSky;
		Float4 AmbientGround;
		Float4 ShadowParams;
		Float4 RimColor;
	};

	//	HLSL の PostConstants(b1) と同じ並び
	struct PostConstants
	{
		Math::Matrix InvProjection;
		Math::Matrix InvViewProjection;
		Math::Matrix Projection;
		Float4 ScreenSize;
		Float4 CameraPosition;
		Float4 FogColor;
		Float4 FogParams;
		Float4 BloomParams;
		Float4 ToneParams;
		Float4 AOParams;
		Float4 GradeParams;
		Float4 SunDirection;
	};

	mw::BlobPtr CompileShader(const wchar_t* File, const char* Entry, const char* Model)
	{
#ifdef _DEBUG
		const UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		const UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
		mw::BlobPtr shader;
		mw::BlobPtr error;
		const HRESULT hr = D3DCompileFromFile(File, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			Entry, Model, flags, 0, shader.GetAddressOf(), error.GetAddressOf());
		if (FAILED(hr))
		{
			if (error != nullptr)
			{
				LOG_ERROR("Shader compile error ({}): {}", Entry, static_cast<const char*>(error->GetBufferPointer()));
			}
			else
			{
				LOG_ERROR("Shader file not found: {}", Entry);
			}
			return nullptr;
		}
		return shader;
	}

	D3D12_STATIC_SAMPLER_DESC MakeSampler(UINT Register, D3D12_FILTER Filter, D3D12_TEXTURE_ADDRESS_MODE Address)
	{
		D3D12_STATIC_SAMPLER_DESC desc = {};
		desc.Filter = Filter;
		desc.AddressU = Address;
		desc.AddressV = Address;
		desc.AddressW = Address;
		desc.MaxAnisotropy = 1;
		desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		desc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
		desc.MinLOD = 0.0f;
		desc.MaxLOD = D3D12_FLOAT32_MAX;
		desc.ShaderRegister = Register;
		desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		return desc;
	}

	bool SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC& Desc, mw::RootSignaturePtr& Out)
	{
		mw::BlobPtr blob;
		mw::BlobPtr error;
		HRESULT hr = D3D12SerializeRootSignature(&Desc, D3D_ROOT_SIGNATURE_VERSION_1_0, blob.GetAddressOf(), error.GetAddressOf());
		if (FAILED(hr))
		{
			if (error != nullptr)
			{
				LOG_ERROR("RootSignature error: {}", static_cast<const char*>(error->GetBufferPointer()));
			}
			return false;
		}
		ID3D12Device* device = Engine::Graphics::DirectX::GetInstance()->GetDevice();
		hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&Out));
		return SUCCEEDED(hr);
	}

	Float4 ToFloat4(const Engine::Graphics::Color& c, float w)
	{
		return { c.r, c.g, c.b, w };
	}

	//	DirectXMath の XMMatrixOrthographicOffCenterLH と同じ
	Math::Matrix OrthoOffCenterLH(float l, float r, float b, float t, float n, float f)
	{
		Math::Matrix m = Math::Matrix::identity;
		m._11 = 2.0f / (r - l);
		m._22 = 2.0f / (t - b);
		m._33 = 1.0f / (f - n);
		m._41 = (l + r) / (l - r);
		m._42 = (t + b) / (b - t);
		m._43 = n / (n - f);
		m._44 = 1.0f;
		return m;
	}
}

namespace Engine::Graphics
{
	void SceneRenderer::OnCreate()
	{
	}

	void SceneRenderer::OnDestory()
	{
		if (mConstantUpload != nullptr)
		{
			mConstantUpload->Unmap(0, nullptr);
			mConstantMapped = nullptr;
		}
		auto gdh = GraphicsDescriptorHeapManager::GetInstance();
		if (gdh != nullptr)
		{
			gdh->Discard(mAOInputs);
			gdh->Discard(mPrefilterInputs);
			for (auto& t : mDownInputs) gdh->Discard(t);
			for (auto& t : mUpInputs) gdh->Discard(t);
			gdh->Discard(mCompositeInputs);
			gdh->Discard(mFxaaInputs);
			gdh->Discard(mShadowSrv);
			gdh->Discard(mEnvironmentSrv);
			gdh->Discard(mDefaultSurfaceSrv);
		}
	}

	bool SceneRenderer::Initialize(UINT Width, UINT Height)
	{
		mWidth = Width;
		mHeight = Height;
		ID3D12Device* device = DirectX::GetInstance()->GetDevice();

		for (auto& t : mDownInputs) t = { -1, -1 };
		for (auto& t : mUpInputs) t = { -1, -1 };

		D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
		rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvDesc.NumDescriptors = 3 + BLOOM_LEVELS;
		if (FAILED(device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&mRtvHeap))))
		{
			LOG_CRITICAL("Failed Create SceneRenderer RTV Heap");
			return false;
		}
		D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
		dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvDesc.NumDescriptors = 1;
		if (FAILED(device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&mDsvHeap))))
		{
			LOG_CRITICAL("Failed Create SceneRenderer DSV Heap");
			return false;
		}

		bool ok = CreateTarget(mHdr, Width, Height, DirectX::GetSceneFormat(), 0);
		ok &= CreateTarget(mLdr, Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM, 1);
		ok &= CreateTarget(mAO, (std::max)(1u, Width / 2), (std::max)(1u, Height / 2), DXGI_FORMAT_R8_UNORM, 2);
		for (UINT i = 0; i < BLOOM_LEVELS; i++)
		{
			ok &= CreateTarget(mBloom[i], (std::max)(1u, Width >> (i + 1)), (std::max)(1u, Height >> (i + 1)), DXGI_FORMAT_R11G11B10_FLOAT, 3 + i);
		}
		ok &= CreateShadowMap();
		if (ok == false)
		{
			LOG_CRITICAL("Failed Create SceneRenderer Targets");
			return false;
		}

		//	定数バッファ(フレーム数 x [Scene, Post])
		D3D12_HEAP_PROPERTIES upload = {};
		upload.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC buffer = {};
		buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffer.Width = static_cast<UINT64>(CB_SLOT_SIZE) * 2 * FRAME_COUNT;
		buffer.Height = 1;
		buffer.DepthOrArraySize = 1;
		buffer.MipLevels = 1;
		buffer.SampleDesc.Count = 1;
		buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		if (FAILED(device->CreateCommittedResource(&upload, D3D12_HEAP_FLAG_NONE, &buffer,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mConstantUpload))))
		{
			LOG_CRITICAL("Failed Create SceneRenderer ConstantBuffer");
			return false;
		}
		D3D12_RANGE none = {};
		mConstantUpload->Map(0, &none, reinterpret_cast<void**>(&mConstantMapped));

		if (CreateRootSignatures() == false || CreatePipelines() == false)
		{
			LOG_CRITICAL("Failed Create SceneRenderer Pipelines");
			return false;
		}
		CreateDescriptorTables();

		mInitialized = true;
		return true;
	}

	bool SceneRenderer::CreateTarget(RenderTarget& Target, UINT Width, UINT Height, DXGI_FORMAT Format, UINT RtvIndex)
	{
		ID3D12Device* device = DirectX::GetInstance()->GetDevice();
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = Width;
		desc.Height = Height;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = Format;
		desc.SampleDesc.Count = 1;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_HEAP_PROPERTIES heap = {};
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_CLEAR_VALUE clear = {};
		clear.Format = Format;

		if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear, IID_PPV_ARGS(&Target.Resource))))
		{
			return false;
		}

		Target.Rtv = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
		Target.Rtv.ptr += static_cast<SIZE_T>(RtvIndex) * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		device->CreateRenderTargetView(Target.Resource.Get(), nullptr, Target.Rtv);
		Target.State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		Target.Width = Width;
		Target.Height = Height;
		Target.Format = Format;
		return true;
	}

	bool SceneRenderer::CreateShadowMap()
	{
		ID3D12Device* device = DirectX::GetInstance()->GetDevice();
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = SHADOW_MAP_SIZE;
		desc.Height = SHADOW_MAP_SIZE;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_R32_TYPELESS;
		desc.SampleDesc.Count = 1;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_HEAP_PROPERTIES heap = {};
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_CLEAR_VALUE clear = {};
		clear.Format = DXGI_FORMAT_D32_FLOAT;
		clear.DepthStencil.Depth = 1.0f;

		if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear, IID_PPV_ARGS(&mShadowMap))))
		{
			return false;
		}
		mShadowState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = DXGI_FORMAT_D32_FLOAT;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		device->CreateDepthStencilView(mShadowMap.Get(), &dsv, mDsvHeap->GetCPUDescriptorHandleForHeapStart());
		return true;
	}

	bool SceneRenderer::CreateRootSignatures()
	{
		//	ポストエフェクト用  0: 定数(b0)  1: CBV(b1)  2: SRV x4(t0-t3)
		{
			std::array<D3D12_ROOT_PARAMETER, 3> params = {};
			params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			params[0].Constants.ShaderRegister = 0;
			params[0].Constants.Num32BitValues = 8;
			params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			params[1].Descriptor.ShaderRegister = 1;
			params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			D3D12_DESCRIPTOR_RANGE range = {};
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			range.NumDescriptors = 4;
			range.BaseShaderRegister = 0;
			range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[2].DescriptorTable.NumDescriptorRanges = 1;
			params[2].DescriptorTable.pDescriptorRanges = &range;
			params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			std::array<D3D12_STATIC_SAMPLER_DESC, 2> samplers = {
				MakeSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
				MakeSampler(1, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP)
			};

			D3D12_ROOT_SIGNATURE_DESC desc = {};
			desc.NumParameters = static_cast<UINT>(params.size());
			desc.pParameters = params.data();
			desc.NumStaticSamplers = static_cast<UINT>(samplers.size());
			desc.pStaticSamplers = samplers.data();
			if (SerializeRootSignature(desc, mPostRootSignature) == false)
			{
				return false;
			}
		}

		//	シャドウ用  0: モデルCBV(b0)  1: シーンCBV(b1)
		{
			std::array<D3D12_ROOT_PARAMETER, 2> params = {};
			for (UINT i = 0; i < 2; i++)
			{
				params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				params[i].Descriptor.ShaderRegister = i;
				params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			}
			D3D12_ROOT_SIGNATURE_DESC desc = {};
			desc.NumParameters = static_cast<UINT>(params.size());
			desc.pParameters = params.data();
			desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
			if (SerializeRootSignature(desc, mShadowRootSignature) == false)
			{
				return false;
			}
		}
		return true;
	}

	bool SceneRenderer::CreatePipelines()
	{
		ID3D12Device* device = DirectX::GetInstance()->GetDevice();
		const wchar_t* file = L"Assets/Shader/PostProcess.hlsl";

		auto vs = CompileShader(file, "VSFullscreen", "vs_5_0");
		if (vs == nullptr)
		{
			return false;
		}

		auto makeFullscreen = [&](const char* Entry, DXGI_FORMAT Format, bool Additive, mw::PipelineStatePtr& Out)
			{
				auto ps = CompileShader(file, Entry, "ps_5_0");
				if (ps == nullptr)
				{
					return false;
				}
				D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
				desc.pRootSignature = mPostRootSignature.Get();
				desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
				desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
				desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
				desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				desc.RasterizerState.DepthClipEnable = TRUE;
				auto& rt = desc.BlendState.RenderTarget[0];
				rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
				rt.BlendEnable = Additive ? TRUE : FALSE;
				rt.SrcBlend = D3D12_BLEND_ONE;
				rt.DestBlend = D3D12_BLEND_ONE;
				rt.BlendOp = D3D12_BLEND_OP_ADD;
				rt.SrcBlendAlpha = D3D12_BLEND_ONE;
				rt.DestBlendAlpha = D3D12_BLEND_ZERO;
				rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
				rt.LogicOp = D3D12_LOGIC_OP_NOOP;
				desc.DepthStencilState.DepthEnable = FALSE;
				desc.SampleMask = UINT_MAX;
				desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				desc.NumRenderTargets = 1;
				desc.RTVFormats[0] = Format;
				desc.SampleDesc.Count = 1;
				return SUCCEEDED(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&Out)));
			};

		bool ok = makeFullscreen("PSSSAO", DXGI_FORMAT_R8_UNORM, false, mSSAOPipeline);
		ok &= makeFullscreen("PSBloomPrefilter", DXGI_FORMAT_R11G11B10_FLOAT, false, mPrefilterPipeline);
		ok &= makeFullscreen("PSBloomDownsample", DXGI_FORMAT_R11G11B10_FLOAT, false, mDownsamplePipeline);
		ok &= makeFullscreen("PSBloomUpsample", DXGI_FORMAT_R11G11B10_FLOAT, true, mUpsamplePipeline);
		ok &= makeFullscreen("PSComposite", DXGI_FORMAT_R8G8B8A8_UNORM, false, mCompositePipeline);
		ok &= makeFullscreen("PSFXAA", DirectX::GetInstance()->GetFormat(), false, mFxaaPipeline);
		if (ok == false)
		{
			return false;
		}

		//	シャドウ(深度のみ)
		auto shadowVS = CompileShader(L"Assets/Shader/VS_Shadow.hlsl", "main", "vs_5_0");
		if (shadowVS == nullptr)
		{
			return false;
		}
		std::array<D3D12_INPUT_ELEMENT_DESC, 5> layout = { {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BONE_INDEX", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "WEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		} };

		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = mShadowRootSignature.Get();
		desc.VS = { shadowVS->GetBufferPointer(), shadowVS->GetBufferSize() };
		desc.InputLayout = { layout.data(), static_cast<UINT>(layout.size()) };
		desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.RasterizerState.DepthClipEnable = FALSE;	//	ライト手前のオブジェクトも影を落とす
		desc.RasterizerState.SlopeScaledDepthBias = 1.5f;
		desc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;
		desc.DepthStencilState.DepthEnable = TRUE;
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 0;
		desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		desc.SampleDesc.Count = 1;
		if (FAILED(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mShadowPipeline))))
		{
			LOG_ERROR("Failed Create Shadow Pipeline");
			return false;
		}
		return true;
	}

	void SceneRenderer::WriteSrv(const DescriptorHeapInfo& Table, UINT Slot, ID3D12Resource* Resource, DXGI_FORMAT Format)
	{
		auto gdh = GraphicsDescriptorHeapManager::GetInstance();
		ID3D12Device* device = DirectX::GetInstance()->GetDevice();
		D3D12_CPU_DESCRIPTOR_HANDLE handle = gdh->GetCpuHandle(Table);
		handle.ptr += static_cast<SIZE_T>(Slot) * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.Format = Format;
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(Resource, &srv, handle);
	}

	void SceneRenderer::WriteNullSrvs(const DescriptorHeapInfo& Table)
	{
		for (UINT i = 0; i < static_cast<UINT>(Table.size); i++)
		{
			WriteSrv(Table, i, nullptr, DXGI_FORMAT_R8G8B8A8_UNORM);
		}
	}

	void SceneRenderer::CreateDescriptorTables()
	{
		auto gdh = GraphicsDescriptorHeapManager::GetInstance();
		ID3D12Resource* depth = DirectX::GetInstance()->GetDepthBuffer();

		auto issue = [&](DescriptorHeapInfo& Table)
			{
				Table = gdh->Issuance(4);
				WriteNullSrvs(Table);
			};

		issue(mAOInputs);
		WriteSrv(mAOInputs, 0, depth, DXGI_FORMAT_R32_FLOAT);

		issue(mPrefilterInputs);
		WriteSrv(mPrefilterInputs, 0, mHdr.Resource.Get(), mHdr.Format);

		for (UINT i = 1; i < BLOOM_LEVELS; i++)
		{
			issue(mDownInputs[i]);
			WriteSrv(mDownInputs[i], 0, mBloom[i - 1].Resource.Get(), mBloom[i - 1].Format);
		}
		for (UINT i = 0; i + 1 < BLOOM_LEVELS; i++)
		{
			issue(mUpInputs[i]);
			WriteSrv(mUpInputs[i], 0, mBloom[i + 1].Resource.Get(), mBloom[i + 1].Format);
		}

		issue(mCompositeInputs);
		WriteSrv(mCompositeInputs, 0, mHdr.Resource.Get(), mHdr.Format);
		WriteSrv(mCompositeInputs, 1, mBloom[0].Resource.Get(), mBloom[0].Format);
		WriteSrv(mCompositeInputs, 2, mAO.Resource.Get(), mAO.Format);
		WriteSrv(mCompositeInputs, 3, depth, DXGI_FORMAT_R32_FLOAT);

		issue(mFxaaInputs);
		WriteSrv(mFxaaInputs, 0, mLdr.Resource.Get(), mLdr.Format);

		mShadowSrv = gdh->Issuance(1);
		WriteSrv(mShadowSrv, 0, mShadowMap.Get(), DXGI_FORMAT_R32_FLOAT);

		mEnvironmentSrv = gdh->Issuance(1);
		SetEnvironment(nullptr);

		mDefaultSurfaceSrv = gdh->Issuance(2);
		WriteNullSrvs(mDefaultSurfaceSrv);
	}

	void SceneRenderer::WriteTextureSrv(const DescriptorHeapInfo& Table, UINT Slot, ID3D12Resource* Resource)
	{
		auto gdh = GraphicsDescriptorHeapManager::GetInstance();
		ID3D12Device* device = DirectX::GetInstance()->GetDevice();
		D3D12_CPU_DESCRIPTOR_HANDLE handle = gdh->GetCpuHandle(Table);
		handle.ptr += static_cast<SIZE_T>(Slot) * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		if (Resource != nullptr)
		{
			const D3D12_RESOURCE_DESC desc = Resource->GetDesc();
			srv.Format = desc.Format;
			srv.Texture2D.MipLevels = desc.MipLevels;
		}
		else
		{
			srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			srv.Texture2D.MipLevels = 1;
		}
		device->CreateShaderResourceView(Resource, &srv, handle);
	}

	void SceneRenderer::BindSurfaceTextures(const DescriptorHeapInfo* Table)
	{
		const DescriptorHeapInfo& info = (Table != nullptr && Table->IsValid()) ? *Table : mDefaultSurfaceSrv;
		GraphicsDescriptorHeapManager::GetInstance()->Set(5, info);
	}

	void SceneRenderer::SetEnvironment(Texture* CubeMap)
	{
		if (mEnvironmentSrv.IsValid() == false)
		{
			return;
		}
		ID3D12Device* device = DirectX::GetInstance()->GetDevice();
		auto gdh = GraphicsDescriptorHeapManager::GetInstance();

		ID3D12Resource* resource = (CubeMap != nullptr) ? CubeMap->GetResource() : nullptr;
		D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		if (resource != nullptr)
		{
			const D3D12_RESOURCE_DESC desc = resource->GetDesc();
			srv.Format = desc.Format;
			srv.TextureCube.MipLevels = desc.MipLevels;
			mEnvironmentMaxMip = static_cast<float>(desc.MipLevels > 0 ? desc.MipLevels - 1 : 0);
		}
		else
		{
			srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			srv.TextureCube.MipLevels = 1;
			mEnvironmentMaxMip = 0.0f;
		}
		device->CreateShaderResourceView(resource, &srv, gdh->GetCpuHandle(mEnvironmentSrv));
	}

	void SceneRenderer::SetShadowFocus(const Math::Vector3& Center)
	{
		mShadowFocus = Center;
		mHasShadowFocus = true;
	}

	Math::Matrix SceneRenderer::ComputeLightViewProjection()const
	{
		const Math::Vector3 L = Math::Vector3::Normalize(mLighting.SunDirection);
		Math::Vector3 center = mShadowFocus;
		if (mHasShadowFocus == false && System::Camera::Main != nullptr)
		{
			center = System::Camera::Main->GetPosition() + System::Camera::Main->GetForward() * 10.0f;
		}

		const Math::Vector3 up = (std::fabs(L.y) > 0.99f) ? Math::Vector3(0, 0, 1) : Math::Vector3(0, 1, 0);
		const Math::Matrix view = Math::Matrix::LookAt(L * 200.0f, Math::Vector3(0, 0, 0), up);

		//	テクセル単位でスナップしてカメラ移動時の影のチラつきを防ぐ
		Math::Vector3 c = Math::Vector3::TransformCoord(center, view);
		const float radius = mLighting.ShadowRadius;
		const float texel = (radius * 2.0f) / static_cast<float>(SHADOW_MAP_SIZE);
		c.x = std::floor(c.x / texel) * texel;
		c.y = std::floor(c.y / texel) * texel;

		const Math::Matrix proj = OrthoOffCenterLH(c.x - radius, c.x + radius, c.y - radius, c.y + radius, c.z - 120.0f, c.z + 120.0f);
		return view * proj;
	}

	void SceneRenderer::BeginFrame(double Time)
	{
		if (mInitialized == false)
		{
			return;
		}
		const UINT frame = static_cast<UINT>(DirectX::GetInstance()->GetFrameIndex()) % FRAME_COUNT;
		uint8_t* base = mConstantMapped + static_cast<size_t>(frame) * CB_SLOT_SIZE * 2;
		const D3D12_GPU_VIRTUAL_ADDRESS gpuBase = mConstantUpload->GetGPUVirtualAddress() + static_cast<UINT64>(frame) * CB_SLOT_SIZE * 2;
		mSceneCB = gpuBase;
		mPostCB = gpuBase + CB_SLOT_SIZE;

		const auto& options = Core::LaunchOptions::Get();
		if (options.HasSun)
		{
			mLighting.SunDirection = { options.Sun[0], options.Sun[1], options.Sun[2] };
		}
		const SceneLighting& l = mLighting;
		const Math::Vector3 sun = Math::Vector3::Normalize(l.SunDirection);
		Math::Vector3 cameraPos = { 0, 0, 0 };
		Math::Matrix view = Math::Matrix::identity;
		Math::Matrix proj = Math::Matrix::identity;
		if (System::Camera::Main != nullptr)
		{
			cameraPos = System::Camera::Main->GetPosition();
			view = System::Camera::Main->GetView();
			proj = System::Camera::Main->GetProjection();
		}

		SceneConstants scene = {};
		scene.LightViewProj = Math::Matrix::Transpose(ComputeLightViewProjection());
		scene.CameraPosition = { cameraPos.x, cameraPos.y, cameraPos.z, static_cast<float>(Time) };
		scene.LightDirection = { sun.x, sun.y, sun.z, l.SunIntensity };
		scene.LightColor = ToFloat4(l.SunColor, 1.0f);
		scene.AmbientSky = ToFloat4(l.AmbientSky, l.IBLIntensity);
		scene.AmbientGround = ToFloat4(l.AmbientGround, mEnvironmentMaxMip);
		scene.ShadowParams = { 1.0f / SHADOW_MAP_SIZE, l.ShadowBias, l.ShadowStrength, l.ShadowEnabled ? 1.0f : 0.0f };
		scene.RimColor = ToFloat4(l.RimColor, 1.0f);
		memcpy(base, &scene, sizeof(scene));

		PostConstants post = {};
		const Math::Matrix viewProj = view * proj;
		post.InvProjection = Math::Matrix::Transpose(Math::Matrix::Inverse(proj));
		post.InvViewProjection = Math::Matrix::Transpose(Math::Matrix::Inverse(viewProj));
		post.Projection = Math::Matrix::Transpose(proj);
		post.ScreenSize = { static_cast<float>(mWidth), static_cast<float>(mHeight), 1.0f / mWidth, 1.0f / mHeight };
		post.CameraPosition = scene.CameraPosition;
		post.FogColor = ToFloat4(l.FogColor, l.FogDensity);
		post.FogParams = { l.FogStart, l.FogHeightFalloff, l.FogBaseHeight, l.FogMaxOpacity };
		post.BloomParams = { l.BloomThreshold, l.BloomKnee, l.BloomIntensity, 0.0f };
		post.ToneParams = { l.Exposure, l.Saturation, l.Contrast, l.Vignette };
		post.AOParams = { l.AORadius, l.AOIntensity, 0.02f, l.AOEnabled ? 1.0f : 0.0f };
		post.GradeParams = ToFloat4(l.Gain, l.Grain);
		post.SunDirection = { sun.x, sun.y, sun.z, l.FogSunScatter };
		memcpy(base + CB_SLOT_SIZE, &post, sizeof(post));
	}

	void SceneRenderer::Transition(ID3D12Resource* Resource, D3D12_RESOURCE_STATES& Current, D3D12_RESOURCE_STATES Next)
	{
		if (Current == Next)
		{
			return;
		}
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = Resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = Current;
		barrier.Transition.StateAfter = Next;
		DirectX::GetInstance()->GetCommandList()->ResourceBarrier(1, &barrier);
		Current = Next;
	}

	void SceneRenderer::Transition(RenderTarget& Target, D3D12_RESOURCE_STATES Next)
	{
		Transition(Target.Resource.Get(), Target.State, Next);
	}

	void SceneRenderer::SetViewport(UINT Width, UINT Height)
	{
		auto cmd = DirectX::GetInstance()->GetCommandList();
		D3D12_VIEWPORT vp = { 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f };
		D3D12_RECT rect = { 0, 0, static_cast<LONG>(Width), static_cast<LONG>(Height) };
		cmd->RSSetViewports(1, &vp);
		cmd->RSSetScissorRects(1, &rect);
	}

	void SceneRenderer::BeginShadowPass()
	{
		if (mInitialized == false)
		{
			return;
		}
		auto cmd = DirectX::GetInstance()->GetCommandList();
		mInShadowPass = true;
		Transition(mShadowMap.Get(), mShadowState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		const D3D12_CPU_DESCRIPTOR_HANDLE dsv = mDsvHeap->GetCPUDescriptorHandleForHeapStart();
		cmd->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
		cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		SetViewport(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
	}

	void SceneRenderer::BindShadowPipeline()
	{
		Renderer::GetInstance()->ApplyPipeline(mShadowRootSignature.Get(), mShadowPipeline.Get(), D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DirectX::GetInstance()->GetCommandList()->SetGraphicsRootConstantBufferView(1, mSceneCB);
	}

	void SceneRenderer::BindFbxSceneResources()
	{
		if (mInitialized == false)
		{
			return;
		}
		auto cmd = DirectX::GetInstance()->GetCommandList();
		auto gdh = GraphicsDescriptorHeapManager::GetInstance();
		cmd->SetGraphicsRootConstantBufferView(2, mSceneCB);
		gdh->Set(3, mShadowSrv);
		gdh->Set(4, mEnvironmentSrv);
	}

	void SceneRenderer::BeginScenePass()
	{
		auto directX = DirectX::GetInstance();
		auto cmd = directX->GetCommandList();
		mInShadowPass = false;
		if (mInitialized == false)
		{
			directX->BindBackBuffer(true);
			return;
		}
		Transition(mShadowMap.Get(), mShadowState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		Transition(mHdr, D3D12_RESOURCE_STATE_RENDER_TARGET);

		const D3D12_CPU_DESCRIPTOR_HANDLE dsv = directX->GetDepthDSV();
		cmd->OMSetRenderTargets(1, &mHdr.Rtv, FALSE, &dsv);
		const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		cmd->ClearRenderTargetView(mHdr.Rtv, clear, 0, nullptr);
		cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		SetViewport(mWidth, mHeight);
		Renderer::GetInstance()->InvalidatePipelineCache();
	}

	void SceneRenderer::DrawFullscreen(ID3D12PipelineState* Pipeline, RenderTarget& Output, const DescriptorHeapInfo& Inputs, float P0, float P1, float P2, float P3)
	{
		auto cmd = DirectX::GetInstance()->GetCommandList();
		Transition(Output, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmd->OMSetRenderTargets(1, &Output.Rtv, FALSE, nullptr);
		SetViewport(Output.Width, Output.Height);
		cmd->SetPipelineState(Pipeline);
		const float constants[8] = { P0, P1, P2, P3, 0, 0, 0, 0 };
		cmd->SetGraphicsRoot32BitConstants(0, 8, constants, 0);
		GraphicsDescriptorHeapManager::GetInstance()->Set(2, Inputs);
		cmd->DrawInstanced(3, 1, 0, 0);
		Transition(Output, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void SceneRenderer::EndScenePass()
	{
		auto directX = DirectX::GetInstance();
		auto cmd = directX->GetCommandList();
		if (mInitialized == false)
		{
			return;
		}

		Transition(mHdr, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		Transition(directX->GetDepthBuffer(), mDepthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		cmd->SetGraphicsRootSignature(mPostRootSignature.Get());
		cmd->SetGraphicsRootConstantBufferView(1, mPostCB);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//	SSAO
		if (mLighting.AOEnabled)
		{
			DrawFullscreen(mSSAOPipeline.Get(), mAO, mAOInputs, 0, 0, 0, 0);
		}
		else
		{
			Transition(mAO, D3D12_RESOURCE_STATE_RENDER_TARGET);
			const float white[4] = { 1, 1, 1, 1 };
			cmd->ClearRenderTargetView(mAO.Rtv, white, 0, nullptr);
			Transition(mAO, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		//	Bloom: 縮小
		DrawFullscreen(mPrefilterPipeline.Get(), mBloom[0], mPrefilterInputs, 1.0f / mWidth, 1.0f / mHeight, 0, 0);
		for (UINT i = 1; i < BLOOM_LEVELS; i++)
		{
			const RenderTarget& src = mBloom[i - 1];
			DrawFullscreen(mDownsamplePipeline.Get(), mBloom[i], mDownInputs[i], 1.0f / src.Width, 1.0f / src.Height, 0, 0);
		}
		//	Bloom: 拡大しながら加算
		for (int i = static_cast<int>(BLOOM_LEVELS) - 2; i >= 0; i--)
		{
			const RenderTarget& src = mBloom[i + 1];
			DrawFullscreen(mUpsamplePipeline.Get(), mBloom[i], mUpInputs[i], 1.0f / src.Width, 1.0f / src.Height, 1.0f, 0);
		}

		//	合成 (トーンマップ)
		DrawFullscreen(mCompositePipeline.Get(), mLdr, mCompositeInputs, 0, 0, 0, 0);

		//	FXAA -> バックバッファ
		const D3D12_CPU_DESCRIPTOR_HANDLE backBuffer = directX->GetBackBufferRTV();
		cmd->OMSetRenderTargets(1, &backBuffer, FALSE, nullptr);
		SetViewport(mWidth, mHeight);
		cmd->SetPipelineState(mFxaaPipeline.Get());
		GraphicsDescriptorHeapManager::GetInstance()->Set(2, mFxaaInputs);
		cmd->DrawInstanced(3, 1, 0, 0);

		//	UI はバックバッファへ。深度はデバッグ線などで使うので戻す
		Transition(directX->GetDepthBuffer(), mDepthState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		directX->BindBackBuffer(true);
		Renderer::GetInstance()->InvalidatePipelineCache();
	}

	void SceneRenderer::ImGuiUpdate()
	{
#ifdef _DEBUG
		if (ImGui::Begin("Rendering"))
		{
			SceneLighting& l = mLighting;
			ImGui::DragFloat3("Sun Direction", &l.SunDirection.x, 0.01f, -1.0f, 1.0f);
			ImGui::ColorEdit3("Sun Color", l.SunColor.value);
			ImGui::DragFloat("Sun Intensity", &l.SunIntensity, 0.05f, 0.0f, 20.0f);
			ImGui::ColorEdit3("Ambient Sky", l.AmbientSky.value);
			ImGui::ColorEdit3("Ambient Ground", l.AmbientGround.value);
			ImGui::DragFloat("IBL", &l.IBLIntensity, 0.01f, 0.0f, 4.0f);
			ImGui::ColorEdit3("Rim", l.RimColor.value, ImGuiColorEditFlags_HDR);
			ImGui::Separator();
			ImGui::Checkbox("Shadow", &l.ShadowEnabled);
			ImGui::DragFloat("Shadow Radius", &l.ShadowRadius, 0.1f, 5.0f, 100.0f);
			ImGui::DragFloat("Shadow Bias", &l.ShadowBias, 0.0001f, 0.0f, 0.01f, "%.4f");
			ImGui::Separator();
			ImGui::ColorEdit3("Fog Color", l.FogColor.value);
			ImGui::DragFloat("Fog Density", &l.FogDensity, 0.001f, 0.0f, 0.2f);
			ImGui::DragFloat("Fog Start", &l.FogStart, 0.1f, 0.0f, 200.0f);
			ImGui::DragFloat("Fog Height Falloff", &l.FogHeightFalloff, 0.005f, 0.0f, 2.0f);
			ImGui::Separator();
			ImGui::Checkbox("SSAO", &l.AOEnabled);
			ImGui::DragFloat("AO Radius", &l.AORadius, 0.01f, 0.05f, 5.0f);
			ImGui::DragFloat("AO Intensity", &l.AOIntensity, 0.01f, 0.0f, 3.0f);
			ImGui::Separator();
			ImGui::DragFloat("Bloom Threshold", &l.BloomThreshold, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("Bloom Intensity", &l.BloomIntensity, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("Exposure", &l.Exposure, 0.01f, 0.05f, 8.0f);
			ImGui::DragFloat("Saturation", &l.Saturation, 0.01f, 0.0f, 2.0f);
			ImGui::DragFloat("Contrast", &l.Contrast, 0.01f, 0.5f, 2.0f);
			ImGui::DragFloat("Vignette", &l.Vignette, 0.01f, 0.0f, 2.0f);
		}
		ImGui::End();
#endif // _DEBUG
	}
}
