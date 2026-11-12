#include "pch.h"
#include "ImGuiManager.hpp"

#include"Graphics/DirectX/DirectX12.hpp"
#include"Graphics/GraphicsDescriptorHeap/GraphicsDescriptorHeapManager.hpp"

#include"Debug/spdlog/Logger.hpp"

#include<Plugin/ImGui/imgui_impl_dx12.h>
#include<Plugin/ImGui/imgui_impl_win32.h>

/// <summary>
/// ImGuiの初期化
/// </summary>
/// <param name="WindowHandle">ウィンドウのハンドル</param>
/// <param name="DirectX">DirectXクラスのポインタ</param>
/// <param name="GDHManager">ヒープ管理のポインタ</param>
/// <returns>true:成功</returns>
bool Engine::Debug::ImGuiManager::Initialize(const HWND WindowHandle,Graphics::DirectX * DirectX, Graphics::GraphicsDescriptorHeapManager* GDHManager)
{
    //  コンテキストの作成
    mContext = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

#ifdef _DEBUG
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#else
    io.IniFilename = nullptr;
#endif

    //  HUD用の日本語フォント(1.92は必要な文字だけ動的に生成される)
    if (io.Fonts->AddFontFromFileTTF("Assets/Fonts/NotoSansJP-Regular.ttf", 20.0f) == nullptr)
    {
        io.Fonts->AddFontDefault();
    }

    //  初期化
    ImGui::SetCurrentContext(mContext);
    ImGui_ImplWin32_Init(WindowHandle);

    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = DirectX->GetDevice();
    init_info.CommandQueue = DirectX->GetCommandQueue();
    init_info.NumFramesInFlight = DirectX->NUM_BACK_BAFFER;// APP_NUM_FRAMES_IN_FLIGHT;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;

    //  動的フォントはテクスチャを作り直すので、ディスクリプタをその都度割り当てる
    init_info.SrvDescriptorHeap = GDHManager->GetGraphicsHeap();
    init_info.UserData = GDHManager;
    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* Info, D3D12_CPU_DESCRIPTOR_HANDLE* OutCpu, D3D12_GPU_DESCRIPTOR_HANDLE* OutGpu)
        {
            auto gdh = static_cast<Graphics::GraphicsDescriptorHeapManager*>(Info->UserData);
            const Graphics::DescriptorHeapInfo heapInfo = gdh->Issuance(1);
            *OutCpu = gdh->GetCpuHandle(heapInfo);
            *OutGpu = gdh->GetGpuHandle(heapInfo);
        };
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* Info, D3D12_CPU_DESCRIPTOR_HANDLE Cpu, D3D12_GPU_DESCRIPTOR_HANDLE)
        {
            auto gdh = static_cast<Graphics::GraphicsDescriptorHeapManager*>(Info->UserData);
            const UINT increment = Graphics::DirectX::GetInstance()->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            const SIZE_T start = gdh->GetGraphicsHeap()->GetCPUDescriptorHandleForHeapStart().ptr;
            Graphics::DescriptorHeapInfo heapInfo = { static_cast<int>((Cpu.ptr - start) / increment), 1 };
            gdh->Discard(heapInfo);
        };
    ImGui_ImplDX12_Init(&init_info);

    return true;
}

/// <summary>
/// インスタンス解放
/// </summary>
void Engine::Debug::ImGuiManager::OnDestory()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(mContext);
    mContext = nullptr;
}

/// <summary>
/// 新規フレームの開始
/// </summary>
void Engine::Debug::ImGuiManager::NewFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

/// <summary>
/// フレームの終了
/// </summary>
void Engine::Debug::ImGuiManager::EndFrame(Graphics::DirectX* DirectX, Graphics::GraphicsDescriptorHeapManager* GDHManager)
{
    ImGui::Render();
    ID3D12GraphicsCommandList* cmdlist = DirectX->GetCommandList();
    ID3D12DescriptorHeap* heap = GDHManager->GetGraphicsHeap();
    cmdlist->SetDescriptorHeaps(1, &heap);
    ImGui_ImplDX12_RenderDrawData(
        ImGui::GetDrawData(),
        cmdlist);

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}
