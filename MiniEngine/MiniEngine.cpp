#include "stdafx.h"
#include "MiniEngine.h"

using namespace Microsoft::WRL;

MiniEngine::MiniEngine(UINT width, UINT height, std::wstring name) :
    Window(width, height, name),
    frameIndex(0)
{

}

MiniEngine::~MiniEngine()
{
    delete pCommandList;
}

void MiniEngine::OnInit()
{
    LoadPipeline();
    LoadAssets();
}

// Load the rendering pipeline dependencies.
void MiniEngine::LoadPipeline()
{
    // Create the device.
    pDevice = std::make_shared<D3D12Device>();
    pDevice->CreateDevice();
    pDevice->CreateDescriptorHeapManager();
    pDevice->CreateBufferManager();

    // Create and init the view manager.
    pViewManager = make_shared<ViewManager>(pDevice, width, height);
    frameIndex = pViewManager->GetSwapChain()->GetCurrentBackBufferIndex();

    // Create the command allocator.
    ThrowIfFailed(pDevice->GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));

    // Create the command list.
    pCommandList = new D3D12CommandList(pDevice, commandAllocator);
}

// Load the sample assets.
void MiniEngine::LoadAssets()
{
    // Create an empty root signature.
    {
        CD3DX12_DESCRIPTOR_RANGE descriptorTableRanges[4];
        descriptorTableRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
        descriptorTableRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 1, 0);
        descriptorTableRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        descriptorTableRanges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 3, 1, 0);

        CD3DX12_ROOT_PARAMETER rootParameters[6];
        rootParameters[CONSTANT_BUFFER_VIEW_GLOBAL].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters[CONSTANT_BUFFER_VIEW_PEROBJECT].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters[SHADER_RESOURCE_VIEW_GLOBAL].InitAsDescriptorTable(1, &descriptorTableRanges[0], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[SHADER_RESOURCE_VIEW].InitAsDescriptorTable(1, &descriptorTableRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[UNORDERED_ACCESS_VIEW].InitAsDescriptorTable(1, &descriptorTableRanges[2], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[SAMPLER].InitAsDescriptorTable(1, &descriptorTableRanges[3], D3D12_SHADER_VISIBILITY_PIXEL);

        // create a static sampler
        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = 0.0f;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &sampler,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            signature.GetAddressOf(),
            error.GetAddressOf()));
        ThrowIfFailed(pDevice->GetDevice()->CreateRootSignature(0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature)));
    }

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(pDevice->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }

    // Create scene objects.
    {
        // Create and init the scene manager.
        pSceneManager = make_shared<SceneManager>(pDevice);
        pSceneManager->InitFBXImporter();
        pSceneManager->LoadScene(pCommandList);
        pSceneManager->CreateCamera(width, height);
        pCommandList->ExecuteCommandList();
        WaitForGPU();

        // Create and init render passes.
        pCommandList->Reset(commandAllocator);
        pDrawObjectPass = make_shared<DrawObjectsPass>(pDevice, pSceneManager);
        pDrawObjectPass->Setup(pCommandList, rootSignature);

        pDrawSkyboxPass = make_shared<DrawSkyboxPass>(pDevice, pSceneManager);
        pDrawSkyboxPass->Setup(pCommandList, rootSignature);

        pBlitPass = make_shared<BlitPass>(pDevice, pSceneManager);
        pBlitPass->Setup(pCommandList, rootSignature);

        pRayTracingPass = make_shared<RayTracingPass>(pDevice, pSceneManager);
        pRayTracingPass->Setup(pCommandList, rootSignature);
        pRayTracingPass->BuildShaderTables();

        pCommandList->ExecuteCommandList();
        WaitForGPU();
    }
}

void MiniEngine::OnKeyDown(UINT8 key)
{
    switch (key)
    {
    case 'A':
        pSceneManager->GetCamera()->MoveAlongX(1);
        break;
    case 'D':
        pSceneManager->GetCamera()->MoveAlongX(-1);
        break;
    case 'W':
        pSceneManager->GetCamera()->MoveAlongZ(1);
        break;
    case 'S':
        pSceneManager->GetCamera()->MoveAlongZ(-1);
        break;

    case 'Q':
        pSceneManager->GetCamera()->RotateAlongY(1);
        break;
    case 'E':
        pSceneManager->GetCamera()->RotateAlongY(-1);
        break;
    case 'Z':
        pSceneManager->GetCamera()->RotateAlongX(1);
        break;
    case 'X':
        pSceneManager->GetCamera()->RotateAlongX(-1);
        break;

    case 'C':
        pSceneManager->GetCamera()->ResetTransform();
        break;
    }
}

void MiniEngine::OnKeyUp(UINT8 key)
{

}

// Update frame-based values.
void MiniEngine::OnUpdate()
{
    // Update scene objects.
    pSceneManager->UpdateTransforms();
    pSceneManager->UpdateCamera();
}

// Render the scene.
void MiniEngine::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Present the frame.
    ThrowIfFailed(pViewManager->GetSwapChain()->Present(1, 0));

    WaitForPreviousFrame();
}

void MiniEngine::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(fenceEvent);
}

void MiniEngine::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    pCommandList->Reset(commandAllocator);
    //pCommandList->SetRootSignature(rootSignature);

    // Indicate that the back buffer will be used as a render target.
    pCommandList->AddTransitionResourceBarriers(pViewManager->GetBackBufferAt(frameIndex),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    pCommandList->FlushResourceBarriers();

    //pDrawObjectPass->Execute(pCommandList, frameIndex);
    //pDrawSkyboxPass->Execute(pCommandList, frameIndex);

    //pViewManager->EmplaceRenderTarget(pCommandList, D3D12TextureType::ShaderResource);
    //pBlitPass->Execute(pCommandList, frameIndex);
    //pViewManager->EmplaceRenderTarget(pCommandList, D3D12TextureType::RenderTarget);

    //// Indicate that the back buffer will now be used to present.
    //pCommandList->AddTransitionResourceBarriers(pViewManager->GetBackBufferAt(frameIndex),
    //    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    //pCommandList->FlushResourceBarriers();

    pRayTracingPass->Execute(pCommandList, frameIndex);

    pCommandList->AddTransitionResourceBarriers(pViewManager->GetBackBufferAt(frameIndex),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
    pCommandList->AddTransitionResourceBarriers(pViewManager->GetRayTracingOutput(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    pCommandList->FlushResourceBarriers();

    pCommandList->CopyResource(pViewManager->GetBackBufferAt(frameIndex), pViewManager->GetRayTracingOutput());

    pCommandList->AddTransitionResourceBarriers(pViewManager->GetBackBufferAt(frameIndex),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
    pCommandList->AddTransitionResourceBarriers(pViewManager->GetRayTracingOutput(),
        D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    pCommandList->FlushResourceBarriers();

    pCommandList->ExecuteCommandList();
}

void MiniEngine::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 value = UpdateFence();

    // Wait until the previous frame is finished.
    if (fence->GetCompletedValue() < value)
    {
        ThrowIfFailed(fence->SetEventOnCompletion(value, fenceEvent));
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    frameIndex = pViewManager->GetSwapChain()->GetCurrentBackBufferIndex();

    // Release upload buffers from last frame.
    pDevice->GetBufferManager()->ReleaseUploadBuffer();
    // TODO: Add a event system to handle event.
    pSceneManager->Release();
}

// Wait for pending GPU work to complete.
void MiniEngine::WaitForGPU()
{
    const UINT64 value = UpdateFence();

    // Wait until the previous frame is finished.
    if (fence->GetCompletedValue() < value)
    {
        ThrowIfFailed(fence->SetEventOnCompletion(value, fenceEvent));
        WaitForSingleObject(fenceEvent, INFINITE);
    }
}

UINT64 MiniEngine::UpdateFence()
{
    const UINT64 value = fenceValue;
    ThrowIfFailed(pDevice->GetCommandQueue()->Signal(fence.Get(), value));
    fenceValue++;

    return value;
}
