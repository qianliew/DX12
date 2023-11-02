#include "stdafx.h"
#include "ViewManager.h"
#include "Window.h"

ViewManager::ViewManager(shared_ptr<D3D12Device>& device, UINT inWidth, UINT inHeight) :
    pDevice(device),
    width(inWidth),
    height(inHeight)
{
    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FRAME_COUNT;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(pDevice->GetFactory()->CreateSwapChainForHwnd(
        pDevice->GetCommandQueue().Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(pDevice->GetFactory()->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(swapChain1.As(&pSwapChain));

    // Create frame resources.
    // Create a RTV for each frame.
    for (UINT n = 0; n < FRAME_COUNT; n++)
    {
        ThrowIfFailed(pSwapChain->GetBuffer(n, IID_PPV_ARGS(&pBackBuffers[n])));
        pDevice->GetDevice()->CreateRenderTargetView(pBackBuffers[n].Get(), nullptr,
            pDevice->GetDescriptorHeapManager()->GetHandle(RENDER_TARGET_VIEW, n));
    }

    // Create a render target buffer.
    pRenderTarget = new D3D12Texture(1, width, height);
    pRenderTarget->CreateTexture(D3D12TextureType::RenderTarget);

    D3D12_CLEAR_VALUE renderTargetClearValue = {};
    renderTargetClearValue.Color[0] = 0.0f;
    renderTargetClearValue.Color[1] = 0.2f;
    renderTargetClearValue.Color[2] = 0.4f;
    renderTargetClearValue.Color[3] = 1.0f;
    renderTargetClearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    pDevice->GetBufferManager()->AllocateDefaultBuffer(
        pRenderTarget->GetTextureBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &renderTargetClearValue);
    pRenderTarget->GetTextureBuffer()->CreateView(pDevice->GetDevice(),
        pDevice->GetDescriptorHeapManager()->GetHandle(RENDER_TARGET_VIEW, 2));

    // Create a depth stencil buffer.
    pDepthStencil = new D3D12Texture(-1, width, height);
    pDepthStencil->CreateTexture(D3D12TextureType::DepthStencil);

    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    pDevice->GetBufferManager()->AllocateDefaultBuffer(
        pDepthStencil->GetTextureBuffer(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthOptimizedClearValue);
    pDepthStencil->GetTextureBuffer()->CreateView(pDevice->GetDevice(),
        pDevice->GetDescriptorHeapManager()->GetHandle(DEPTH_STENCIL_VIEW, 0));
}

ViewManager::~ViewManager()
{
    delete pRenderTarget;
    delete pDepthStencil;
}

void ViewManager::EmplaceRenderTarget(D3D12CommandList*& pCommandList, D3D12TextureType type)
{
    D3D12_RESOURCE_STATES stateBefore = GetResourceState(pRenderTarget->GetType());
    UINT heapMapIndex = type == D3D12TextureType::ShaderResource ? SHADER_RESOURCE_VIEW : RENDER_TARGET_VIEW;

    pRenderTarget->CreateTexture(type);
    pRenderTarget->GetTextureBuffer()->CreateView(pDevice->GetDevice(),
        pDevice->GetDescriptorHeapManager()->GetHandle(heapMapIndex, 2));
    pCommandList->AddTransitionResourceBarriers(pRenderTarget->GetTextureBuffer()->GetResource(), stateBefore, GetResourceState(type));
    pCommandList->FlushResourceBarriers();
}

// Helper functions
D3D12_RESOURCE_STATES ViewManager::GetResourceState(D3D12TextureType type)
{
    switch (type)
    {
    case D3D12TextureType::RenderTarget:
        return D3D12_RESOURCE_STATE_RENDER_TARGET;
    case D3D12TextureType::ShaderResource:
        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
}