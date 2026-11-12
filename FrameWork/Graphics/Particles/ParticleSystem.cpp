#include "pch.h"
#include "ParticleSystem.hpp"

#include"Debug/spdlog/Logger.hpp"
#include"Graphics/DirectX/DirectX12.hpp"
#include"Graphics/Renderer/Renderer.hpp"
#include"System/Camera/Camera.hpp"
#include"Math/Matrix/Matrix.h"

#include<d3dcompiler.h>
#include<algorithm>

namespace
{
	mw::BlobPtr CompileParticleShader(const char* Entry, const char* Model)
	{
#ifdef _DEBUG
		const UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		const UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
		mw::BlobPtr shader;
		mw::BlobPtr error;
		const HRESULT hr = D3DCompileFromFile(L"Assets/Shader/Particles.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			Entry, Model, flags, 0, shader.GetAddressOf(), error.GetAddressOf());
		if (FAILED(hr))
		{
			LOG_ERROR("Particle shader error ({}): {}", Entry, error != nullptr ? static_cast<const char*>(error->GetBufferPointer()) : "file not found");
			return nullptr;
		}
		return shader;
	}

	Engine::Graphics::Color LerpColor(const Engine::Graphics::Color& a, const Engine::Graphics::Color& b, float t)
	{
		return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t };
	}

	struct CameraConstants
	{
		Math::Matrix ViewProj;
		float Right[4];
		float Up[4];
		float Position[4];
	};
}

namespace Engine::Graphics
{
	void ParticleSystem::OnCreate()
	{
	}

	void ParticleSystem::OnDestory()
	{
		if (mUpload != nullptr)
		{
			mUpload->Unmap(0, nullptr);
			mMapped = nullptr;
		}
	}

	bool ParticleSystem::Initialize()
	{
		mParticles.reserve(MAX_PARTICLES);
		mTrails.resize(MAX_TRAILS);

		ID3D12Device* device = DirectX::GetInstance()->GetDevice();
		D3D12_HEAP_PROPERTIES heap = {};
		heap.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = static_cast<UINT64>(MAX_PARTICLES * sizeof(GpuParticle) + MAX_TRAIL_VERTICES * sizeof(TrailVertex)) * FRAME_COUNT;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mUpload))))
		{
			LOG_ERROR("Failed Create Particle Buffer");
			return false;
		}
		D3D12_RANGE none = {};
		mUpload->Map(0, &none, reinterpret_cast<void**>(&mMapped));

		mInitialized = CreatePipelines();
		return mInitialized;
	}

	bool ParticleSystem::CreatePipelines()
	{
		ID3D12Device* device = DirectX::GetInstance()->GetDevice();

		//	0: カメラ定数(28個)  1: パーティクル配列(SRV)
		std::array<D3D12_ROOT_PARAMETER, 2> params = {};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		params[0].Constants.ShaderRegister = 0;
		params[0].Constants.Num32BitValues = sizeof(CameraConstants) / 4;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		params[1].Descriptor.ShaderRegister = 0;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
		rsDesc.NumParameters = static_cast<UINT>(params.size());
		rsDesc.pParameters = params.data();
		rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		mw::BlobPtr blob;
		mw::BlobPtr error;
		if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, blob.GetAddressOf(), error.GetAddressOf())) ||
			FAILED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature))))
		{
			LOG_ERROR("Failed Create Particle RootSignature");
			return false;
		}

		auto vs = CompileParticleShader("VSParticle", "vs_5_0");
		auto ps = CompileParticleShader("PSParticle", "ps_5_0");
		auto trailVS = CompileParticleShader("VSTrail", "vs_5_0");
		auto trailPS = CompileParticleShader("PSTrail", "ps_5_0");
		if (vs == nullptr || ps == nullptr || trailVS == nullptr || trailPS == nullptr)
		{
			return false;
		}

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
		pso.pRootSignature = mRootSignature.Get();
		pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pso.RasterizerState.DepthClipEnable = TRUE;
		auto& rt = pso.BlendState.RenderTarget[0];
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_ONE;
		rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D12_BLEND_ZERO;
		rt.DestBlendAlpha = D3D12_BLEND_ONE;
		rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		pso.DepthStencilState.DepthEnable = TRUE;
		pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		pso.SampleMask = UINT_MAX;
		pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso.NumRenderTargets = 1;
		pso.RTVFormats[0] = DirectX::GetSceneFormat();
		pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		pso.SampleDesc.Count = 1;
		if (FAILED(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&mParticlePipeline))))
		{
			LOG_ERROR("Failed Create Particle Pipeline");
			return false;
		}

		std::array<D3D12_INPUT_ELEMENT_DESC, 3> layout = { {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		} };
		pso.VS = { trailVS->GetBufferPointer(), trailVS->GetBufferSize() };
		pso.PS = { trailPS->GetBufferPointer(), trailPS->GetBufferSize() };
		pso.InputLayout = { layout.data(), static_cast<UINT>(layout.size()) };
		if (FAILED(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&mTrailPipeline))))
		{
			LOG_ERROR("Failed Create Trail Pipeline");
			return false;
		}
		return true;
	}

	void ParticleSystem::Spawn(const Particle& P)
	{
		if (mParticles.size() >= MAX_PARTICLES)
		{
			return;
		}
		mParticles.push_back(P);
	}

	TrailHandle ParticleSystem::CreateTrail(const Color& TrailColor, float Lifetime)
	{
		for (int i = 0; i < static_cast<int>(mTrails.size()); i++)
		{
			if (mTrails[i].InUse == false)
			{
				mTrails[i].InUse = true;
				mTrails[i].Points.clear();
				mTrails[i].TrailColor = TrailColor;
				mTrails[i].Lifetime = Lifetime;
				return { i };
			}
		}
		return { -1 };
	}

	void ParticleSystem::UpdateTrail(TrailHandle Handle, const Math::Vector3& Base, const Math::Vector3& Tip, bool Emitting)
	{
		if (Handle.IsValid() == false || Handle.Index >= static_cast<int>(mTrails.size()))
		{
			return;
		}
		Trail& trail = mTrails[Handle.Index];
		if (Emitting)
		{
			trail.Points.push_front({ Base, Tip, mTime });
		}
	}

	void ParticleSystem::SetTrailColor(TrailHandle Handle, const Color& TrailColor)
	{
		if (Handle.IsValid() && Handle.Index < static_cast<int>(mTrails.size()))
		{
			mTrails[Handle.Index].TrailColor = TrailColor;
		}
	}

	void ParticleSystem::Update(float DeltaTime)
	{
		mTime += DeltaTime;
		for (auto& p : mParticles)
		{
			p.Age += DeltaTime;
			p.Velocity.y -= p.Gravity * DeltaTime;
			const float drag = (std::max)(0.0f, 1.0f - p.Drag * DeltaTime);
			p.Velocity = p.Velocity * drag;
			p.Position = p.Position + p.Velocity * DeltaTime;
			p.Rotation += p.RotationSpeed * DeltaTime;
		}
		mParticles.erase(std::remove_if(mParticles.begin(), mParticles.end(),
			[](const Particle& p) { return p.Age >= p.Life; }), mParticles.end());

		for (auto& trail : mTrails)
		{
			while (trail.Points.empty() == false && mTime - trail.Points.back().Time > trail.Lifetime)
			{
				trail.Points.pop_back();
			}
		}
	}

	void ParticleSystem::Clear()
	{
		mParticles.clear();
		for (auto& trail : mTrails)
		{
			trail.Points.clear();
			trail.InUse = false;
		}
	}

	void ParticleSystem::Render()
	{
		if (mInitialized == false || System::Camera::Main == nullptr)
		{
			return;
		}
		auto directX = DirectX::GetInstance();
		auto cmd = directX->GetCommandList();

		const UINT frame = static_cast<UINT>(directX->GetFrameIndex()) % FRAME_COUNT;
		const size_t particleBytes = MAX_PARTICLES * sizeof(GpuParticle);
		const size_t trailBytes = MAX_TRAIL_VERTICES * sizeof(TrailVertex);
		const size_t frameOffset = (particleBytes + trailBytes) * frame;
		GpuParticle* gpuParticles = reinterpret_cast<GpuParticle*>(mMapped + frameOffset);
		TrailVertex* gpuTrail = reinterpret_cast<TrailVertex*>(mMapped + frameOffset + particleBytes);
		const D3D12_GPU_VIRTUAL_ADDRESS gpuBase = mUpload->GetGPUVirtualAddress() + frameOffset;

		//	カメラ
		const Math::Matrix& view = System::Camera::Main->GetView();
		const Math::Matrix& proj = System::Camera::Main->GetProjection();
		CameraConstants camera = {};
		camera.ViewProj = Math::Matrix::Transpose(view * proj);
		camera.Right[0] = view._11; camera.Right[1] = view._21; camera.Right[2] = view._31; camera.Right[3] = mTime;
		camera.Up[0] = view._12; camera.Up[1] = view._22; camera.Up[2] = view._32;
		const Math::Vector3 camPos = System::Camera::Main->GetPosition();
		camera.Position[0] = camPos.x; camera.Position[1] = camPos.y; camera.Position[2] = camPos.z;

		//	パーティクルを書き込む
		UINT count = 0;
		for (const auto& p : mParticles)
		{
			const float t = (std::min)(1.0f, p.Age / (std::max)(p.Life, 1e-4f));
			GpuParticle& g = gpuParticles[count++];
			g.Position[0] = p.Position.x; g.Position[1] = p.Position.y; g.Position[2] = p.Position.z;
			g.Size = p.SizeStart + (p.SizeEnd - p.SizeStart) * t;
			const Color c = LerpColor(p.ColorStart, p.ColorEnd, t);
			g.Color[0] = c.r; g.Color[1] = c.g; g.Color[2] = c.b; g.Color[3] = c.a;
			g.Velocity[0] = p.Velocity.x; g.Velocity[1] = p.Velocity.y; g.Velocity[2] = p.Velocity.z;
			g.Stretch = p.Stretch;
			g.Rotation = p.Rotation;
			g.Shape = static_cast<uint32_t>(p.Shape);
			g.Age01 = t;
			g.Additive = p.Additive ? 1.0f : 0.0f;
		}

		//	軌跡を三角形リストにする
		UINT trailVertexCount = 0;
		for (const auto& trail : mTrails)
		{
			if (trail.InUse == false || trail.Points.size() < 2)
			{
				continue;
			}
			const size_t n = trail.Points.size();
			for (size_t i = 0; i + 1 < n; i++)
			{
				if (trailVertexCount + 6 > MAX_TRAIL_VERTICES)
				{
					break;
				}
				const TrailPoint& a = trail.Points[i];
				const TrailPoint& b = trail.Points[i + 1];
				const float ua = (mTime - a.Time) / trail.Lifetime;
				const float ub = (mTime - b.Time) / trail.Lifetime;
				auto put = [&](const Math::Vector3& pos, float u, float v)
					{
						TrailVertex& vtx = gpuTrail[trailVertexCount++];
						vtx.Position[0] = pos.x; vtx.Position[1] = pos.y; vtx.Position[2] = pos.z;
						vtx.Color[0] = trail.TrailColor.r; vtx.Color[1] = trail.TrailColor.g;
						vtx.Color[2] = trail.TrailColor.b; vtx.Color[3] = trail.TrailColor.a;
						vtx.UV[0] = u; vtx.UV[1] = v;
					};
				put(a.Base, ua, 0.0f); put(a.Tip, ua, 1.0f); put(b.Tip, ub, 1.0f);
				put(a.Base, ua, 0.0f); put(b.Tip, ub, 1.0f); put(b.Base, ub, 0.0f);
			}
		}

		if (count == 0 && trailVertexCount == 0)
		{
			return;
		}

		cmd->SetGraphicsRootSignature(mRootSignature.Get());
		cmd->SetGraphicsRoot32BitConstants(0, sizeof(CameraConstants) / 4, &camera, 0);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		if (trailVertexCount > 0)
		{
			cmd->SetPipelineState(mTrailPipeline.Get());
			cmd->SetGraphicsRootShaderResourceView(1, gpuBase);
			D3D12_VERTEX_BUFFER_VIEW vbv = {};
			vbv.BufferLocation = gpuBase + particleBytes;
			vbv.SizeInBytes = trailVertexCount * sizeof(TrailVertex);
			vbv.StrideInBytes = sizeof(TrailVertex);
			cmd->IASetVertexBuffers(0, 1, &vbv);
			cmd->DrawInstanced(trailVertexCount, 1, 0, 0);
		}
		if (count > 0)
		{
			cmd->SetPipelineState(mParticlePipeline.Get());
			cmd->SetGraphicsRootShaderResourceView(1, gpuBase);
			cmd->DrawInstanced(6, count, 0, 0);
		}

		Renderer::GetInstance()->InvalidatePipelineCache();
	}
}
