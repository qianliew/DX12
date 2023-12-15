#pragma once
#include "AbstractRenderPass.h"

class DeferredLightingPass : public AbstractRenderPass
{
public:
	DeferredLightingPass(shared_ptr<D3D12Device>&, shared_ptr<SceneManager>&, shared_ptr<ViewManager>&);

	virtual void Setup(D3D12CommandList*, ComPtr<ID3D12RootSignature>&) override;
	virtual void Execute(D3D12CommandList*) override;
};