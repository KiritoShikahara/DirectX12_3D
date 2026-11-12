#include"pch.h"
#include"Engine/Engine.hpp"

// framework
#include"Graphics/DirectX/DirectX12.hpp"
#include"System/Window/Window.hpp"
#include"System/EventBus/Manager/EventManager.hpp"
#include"System/Entity/Manager/EntityManager.hpp"

//	graphics
#include"Graphics/GraphicsDescriptorHeap/GraphicsDescriptorHeapManager.hpp"
#include"Graphics/Renderer/Renderer.hpp"
#include"Graphics/PostProcess/SceneRenderer.hpp"
#include"Graphics/Particles/ParticleSystem.hpp"
#include<chrono>
#include"Graphics/Effect/Manager/EffectManager.hpp"

// debug
#include"Debug/ImGui/ImGuiManager.hpp"
#include"Debug/spdlog/Logger.hpp"

//	input
#include"System/Input/Manager/InputManager.hpp"

//  resource
#include"Graphics/FbxMesh/Resource/Manager/FbxResourceManager.hpp"
#include"Graphics/Texture/Manager/TextureManager.hpp"
#include"Graphics/SkyBox/Resource/SkyBoxResourceManager.hpp"

//	audio
#include"Audio/Resource/Manager/AudioResourceManager.hpp"
#include"Audio/Manager/AudioManager.hpp"
#include"Audio/Device/AudioDevice.hpp"

/// <summary>
/// フレームワークの初期化
/// </summary>
/// <returns>true:成功</returns>
bool Engine::Core::Engine::Initialize()
{
	bool ret = false;

#if _DEBUG
	//	メモリ解放漏れチェック用
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	Debug::Logger::Initialize();

	//	ウィンドウ
	System::Window::Create();
	mWindow = System::Window::GetInstance();
	ret = mWindow->Initialize("DirectX12 Engine", 0, 0,{1280,720,true});
	if (ret == false)
	{
		return false;
	}


	//	DirectX
	Graphics::DirectX::Create();
	mDirectX = Graphics::DirectX::GetInstance();
	ret = mDirectX->Initialize(
		mWindow->GetWindowHandle(),
		static_cast<UINT>(mWindow->GetWidth()),
		static_cast<UINT>(mWindow->GetHeight()));

	if (ret == false)
	{
		return false;
	}


	mDirectX->SetBackColor({0.5,0.5,0.5,1.0});

	//	描画用ヒープの初期化
	Graphics::GraphicsDescriptorHeapManager::Create();
	mGDHManager = Graphics::GraphicsDescriptorHeapManager::GetInstance();
	ret = mGDHManager->Initialize();
	if (ret == false)
	{
		return false;
	}

	//	表示
	Graphics::Renderer::Create();
	mRenderer = Graphics::Renderer::GetInstance();

	//	シャドウ・HDR・ポストエフェクト
	Graphics::SceneRenderer::Create();
	ret = Graphics::SceneRenderer::GetInstance()->Initialize(
		static_cast<UINT>(mWindow->GetWidth()),
		static_cast<UINT>(mWindow->GetHeight()));
	if (ret == false)
	{
		return false;
	}

	//	エフェクト
	Graphics::EffectManager::Create();
	mEffect = Graphics::EffectManager::GetInstance();
	mEffect->Initialize();

	//	自作パーティクル
	Graphics::ParticleSystem::Create();
	if (Graphics::ParticleSystem::GetInstance()->Initialize() == false)
	{
		return false;
	}

	//	ImGui
	Debug::ImGuiManager::Create();
	mImgui = Debug::ImGuiManager::GetInstance();
	ret = mImgui->Initialize(
		mWindow->GetWindowHandle(),
		mDirectX,
		mGDHManager);

	if (ret == false)
	{
		return false;
	}

	//	ログの初期化


	//	オーディオ初期化
	//	リソース管理
	Audio::AudioResourceManager::Create();
	//	再生管理
	Audio::AudioManager::Create();
	auto AudioManager = Audio::AudioManager::GetInstance();
	ret = AudioManager->Initialize(Audio::AudioResourceManager::GetInstance());
	if (ret == false)
	{
		return false;
	}	
	//	オーディオのデバイス
	Audio::AudioDevice::Create();
	ret = Audio::AudioDevice::GetInstance()->Initialize(AudioManager);
	if (ret == false)
	{
		return false;
	}

	//	入力初期化
	Input::InputManager::Create();
	Input::InputManager::GetInstance()->Initialize();

	//	イベント管理の作成
	System::EventManager::Create();

	//	オブジェクト管理の作成
	System::EntityManager::Create();

	//	リソース管理
	Graphics::FbxResourceManager::Create();
	Graphics::TextureManager::Create();
	Graphics::SkyBoxResourceManager::Create();

	return true;
}

/// <summary>
/// フレームワークの解放
/// </summary>
void Engine::Core::Engine::OnDestory()
{
	Debug::ImGuiManager::Release();
	Graphics::SceneRenderer::Release();
	Graphics::ParticleSystem::Release();
	Graphics::GraphicsDescriptorHeapManager::Release();
	Graphics::DirectX::Release();
	System::Window::Release();
}

/// <summary>
/// フレームワークの解放
/// </summary>
void Engine::Core::Engine::NewFrame()
{
	//	ImGui(ReleaseでもHUD描画に使う)
	mImgui->NewFrame();
}

/// <summary>
/// フレームの開始
/// </summary>
void Engine::Core::Engine::BegineRendering()
{
	mDirectX->BegineRendering();
	mDirectX->SetViewPort(0,0, mWindow->GetWidth(), mWindow->GetHeight());

	mRenderer->NewRenderingFrame();

	static const auto startTime = std::chrono::steady_clock::now();
	const double time = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
	Graphics::SceneRenderer::GetInstance()->BeginFrame(time);
}

/// <summary>
/// フレームの終了
/// </summary>
void Engine::Core::Engine::EndFrame()
{
	mImgui->EndFrame(mDirectX, mGDHManager);

	mDirectX->Flip();
}