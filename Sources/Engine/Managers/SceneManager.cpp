#include "stdafx.h"
#include "SceneManager.h"
#include "LitMaterial.h"
#include "SkyboxMaterial.h"

UINT SceneManager::sTextureID = 0;

SceneManager::SceneManager(shared_ptr<D3D12Device>& device) :
    pDevice(device),
    objectID(0)
{

}

SceneManager::~SceneManager()
{
    UnloadScene();

    delete pSkyboxMaterial;
    delete pSkyboxMesh;
    // delete pFullScreenMesh;
    delete pCamera;
}

void SceneManager::InitFBXImporter()
{
    pFBXImporter.release();
    pFBXImporter = std::make_unique<FBXImporter>();
    pFBXImporter->InitializeSdkObjects();
}

void SceneManager::ParseScene(D3D12CommandList*& pCommandList)
{
    LPCWSTR sceneName = L"scene";
    std::wifstream inFile(GetAssetPath(sceneName));

    // Parse textures from the scene file to materials.
    UINT numMaterials = 0;
    inFile >> numMaterials;

    for (UINT i = 0; i < numMaterials; i++)
    {
        WCHAR materialName[32];
        inFile >> materialName;
        LitMaterial* material = new LitMaterial(materialName);
        material->LoadTexture();

        LoadTextureBufferAndSampler(pCommandList, material->GetTexture());
        LoadTextureBufferAndSampler(pCommandList, material->GetMRATexture());
        LoadTextureBufferAndSampler(pCommandList, material->GetNormalTexture());
        pMaterialPool[EraseSuffix(materialName)] = material;
    }

    // Parse FBX from the scene file.
    UINT numModels = 0;
    inFile >> numModels;

    for (UINT i = 0; i < numModels; i++)
    {
        WCHAR fileName[32];
        inFile >> fileName;

        Model* model = new Model(objectID++, fileName);
        model->LoadModel(pFBXImporter);
        model->SetMaterial(pMaterialPool[EraseSuffix(fileName)]);

        AddObject(model);
        LoadObjectVertexBufferAndIndexBuffer(pCommandList, model);
    }

    inFile.close();
}

void SceneManager::LoadScene(D3D12CommandList*& pCommandList)
{
    // Parse the scene file.
    ParseScene(pCommandList);

    // Create static data.
    pFullScreenMesh = new Model(objectID++, L"plane.fbx");
    pFullScreenMesh->LoadModel(pFBXImporter);
    LoadObjectVertexBufferAndIndexBuffer(pCommandList, pFullScreenMesh);

    // Create assets of the skybox.
    std::wstring skyboxName = L"Skybox\\sky01";
    SkyboxMaterial* material = new SkyboxMaterial(skyboxName);
    material->LoadTexture();
    LoadTextureBufferAndSampler(pCommandList, material->GetTexture());
    pSkyboxMaterial = material;

    pSkyboxMesh = new Model(objectID++, L"Skybox\\skybox.fbx");
    pSkyboxMesh->LoadModel(pFBXImporter);
    LoadObjectVertexBufferAndIndexBuffer(pCommandList, pSkyboxMesh);

    // Create the global constant buffer.
    pDevice->GetBufferManager()->AllocateGlobalConstantBuffer();
    pDevice->GetBufferManager()->GetGlobalConstantBuffer()->CreateView(pDevice->GetDevice(),
        pDevice->GetDescriptorHeapManager()->GetHandle(CONSTANT_BUFFER_VIEW_GLOBAL, 0));
}

void SceneManager::UnloadScene()
{
    objectID = 0;
    sTextureID = 0;

    for (auto it = pObjects.begin(); it != pObjects.end(); it++)
    {
        delete* it;
    }
    pObjects.clear();

    for (auto it = pMaterialPool.begin(); it != pMaterialPool.end(); it++)
    {
        delete it->second;
    }
    pMaterialPool.clear();
}

void SceneManager::CreateCamera(UINT width, UINT height)
{
    pCamera = new Camera(0, static_cast<FLOAT>(width), static_cast<FLOAT>(height));
    pCamera->SetViewport(static_cast<FLOAT>(width), static_cast<FLOAT>(height));
    pCamera->SetScissorRect(static_cast<LONG>(width), static_cast<LONG>(height));
}

void SceneManager::AddObject(Model* object)
{
	pObjects.push_back(object);
}

void SceneManager::DrawObjects(D3D12CommandList*& pCommandList)
{
    for (UINT i = 0; i < pObjects.size(); i++)
    {
        Model* model = pObjects[i];
        UINT id = pObjects[i]->GetObjectID();

        // Set the per object views.
        pCommandList->SetRootConstantBufferView(CONSTANT_BUFFER_VIEW_PEROBJECT,
            pDevice->GetBufferManager()->GetPerObjectConstantBufferAtIndex(id)->GetResource()->GetGPUVirtualAddress());

        // Set the material relating views.
        LitMaterial* litMaterial = dynamic_cast<LitMaterial*>(model->GetMaterial());

        pDevice->GetDescriptorHeapManager()->SetSRVs(pCommandList->GetCommandList(),
            litMaterial->GetTexture()->GetTextureID());
        pDevice->GetDescriptorHeapManager()->SetSamplers(pCommandList->GetCommandList(),
            litMaterial->GetTexture()->GetTextureID());

        // Set buffers and draw the instance.
        pCommandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCommandList->SetVertexBuffers(0, 1, &model->GetMesh()->VertexBuffer->VertexBufferView);
        pCommandList->SetIndexBuffer(&model->GetMesh()->IndexBuffer->IndexBufferView);
        pCommandList->DrawIndexedInstanced(model->GetMesh()->GetIndicesNum());
    }
}

void SceneManager::DrawSkybox(D3D12CommandList*& pCommandList)
{
    pCommandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Set views for the skybox.
    UINT id = pSkyboxMesh->GetObjectID();
    pCommandList->SetRootConstantBufferView(CONSTANT_BUFFER_VIEW_PEROBJECT,
        pDevice->GetBufferManager()->GetPerObjectConstantBufferAtIndex(id)->GetResource()->GetGPUVirtualAddress());
    pDevice->GetDescriptorHeapManager()->SetSRVs(pCommandList->GetCommandList(),
        pSkyboxMaterial->GetTexture()->GetTextureID());
    pDevice->GetDescriptorHeapManager()->SetSamplers(pCommandList->GetCommandList(),
        pSkyboxMaterial->GetTexture()->GetTextureID());

    pCommandList->SetVertexBuffers(0, 1, &pSkyboxMesh->GetMesh()->VertexBuffer->VertexBufferView);
    pCommandList->SetIndexBuffer(&pSkyboxMesh->GetMesh()->IndexBuffer->IndexBufferView);
    pCommandList->DrawIndexedInstanced(pSkyboxMesh->GetMesh()->GetIndicesNum());
}

void SceneManager::DrawFullScreenMesh(D3D12CommandList*& pCommandList)
{
    pCommandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    pCommandList->SetVertexBuffers(0, 1, &pFullScreenMesh->GetMesh()->VertexBuffer->VertexBufferView);
    pCommandList->SetIndexBuffer(&pFullScreenMesh->GetMesh()->IndexBuffer->IndexBufferView);
    pCommandList->DrawIndexedInstanced(pFullScreenMesh->GetMesh()->GetIndicesNum());
}

void SceneManager::UpdateTransforms()
{
    // Set the transform of the skybox.
    pCamera->SetObjectToWorldMatrix();
    pSkyboxMesh->CopyWorldPosition(*pCamera);
    pSkyboxMesh->SetObjectToWorldMatrix();
    pDevice->GetBufferManager()->GetPerObjectConstantBufferAtIndex(pSkyboxMesh->GetObjectID())
        ->CopyData(&pSkyboxMesh->GetTransformConstant(), sizeof(TransformConstant));

    // Set the transform of objects.
    for (UINT i = 0; i < pObjects.size(); i++)
    {
        pObjects[i]->SetObjectToWorldMatrix();
        pDevice->GetBufferManager()->GetPerObjectConstantBufferAtIndex(pObjects[i]->GetObjectID())
            ->CopyData(&pObjects[i]->GetTransformConstant(), sizeof(TransformConstant));
    }
}

void SceneManager::UpdateCamera()
{
    pCamera->UpdateCameraConstant();
    pDevice->GetBufferManager()->GetGlobalConstantBuffer()->CopyData(&pCamera->GetCameraConstant(), sizeof(CameraConstant));
}

void SceneManager::Release()
{
    for (auto it = pMaterialPool.begin(); it != pMaterialPool.end(); it++)
    {
        it->second->ReleaseTextureData();
    }
}

// Helper functions.
void SceneManager::LoadObjectVertexBufferAndIndexBuffer(D3D12CommandList*& pCommandList, Model* object)
{
    // Create the perObject constant buffer and its view.
    UINT id = object->GetObjectID();
    pDevice->GetBufferManager()->AllocatePerObjectConstantBuffers(id);
    pDevice->GetBufferManager()->GetPerObjectConstantBufferAtIndex(id)->CreateView(pDevice->GetDevice(),
        pDevice->GetDescriptorHeapManager()->GetHandle(CONSTANT_BUFFER_VIEW_PEROBJECT, id));

    // Create the vertex buffer and index buffer and their view.
    D3D12UploadBuffer* tempVertexBuffer = new D3D12UploadBuffer();
    pDevice->GetBufferManager()->AllocateUploadBuffer(tempVertexBuffer, object->GetMesh()->GetVerticesSize());
    pDevice->GetBufferManager()->AllocateDefaultBuffer(object->GetMesh()->VertexBuffer.get());
    tempVertexBuffer->CopyData(object->GetMesh()->GetVerticesData(), object->GetMesh()->GetVerticesSize());

    D3D12UploadBuffer* tempIndexBuffer = new D3D12UploadBuffer();
    pDevice->GetBufferManager()->AllocateUploadBuffer(tempIndexBuffer, object->GetMesh()->GetIndicesSize());
    pDevice->GetBufferManager()->AllocateDefaultBuffer(object->GetMesh()->IndexBuffer.get());
    tempIndexBuffer->CopyData(object->GetMesh()->GetIndicesData(), object->GetMesh()->GetIndicesSize());

    object->GetMesh()->CreateView();
    pCommandList->CopyBufferRegion(object->GetMesh()->VertexBuffer->GetResource(),
        tempVertexBuffer->ResourceLocation.Resource.Get(),
        object->GetMesh()->GetVerticesSize());
    pCommandList->CopyBufferRegion(object->GetMesh()->IndexBuffer->GetResource(),
        tempIndexBuffer->ResourceLocation.Resource.Get(),
        object->GetMesh()->GetIndicesSize());

    // Setup transition barriers.
    pCommandList->AddTransitionResourceBarriers(object->GetMesh()->VertexBuffer->GetResource(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    pCommandList->AddTransitionResourceBarriers(object->GetMesh()->IndexBuffer->GetResource(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    pCommandList->FlushResourceBarriers();
}

void SceneManager::LoadTextureBufferAndSampler(D3D12CommandList*& pCommandList, D3D12Texture* texture)
{
    UINT id = texture->GetTextureID();

    // Create the texture buffer.
    pDevice->GetBufferManager()->AllocateDefaultBuffer(texture->GetTextureBuffer());
    texture->GetTextureBuffer()->CreateView(pDevice->GetDevice(),
        pDevice->GetDescriptorHeapManager()->GetHandle(SHADER_RESOURCE_VIEW, id));

    // Init texture data.
    for (UINT i = 0; i < texture->GetSubresourceNum(); i++)
    {
        D3D12_SUBRESOURCE_DATA textureData;
        UINT64 rowSizeInBytes, totalBytes;
        pDevice->GetDevice()->GetCopyableFootprints(&texture->GetTextureBuffer()->GetResourceDesc(),
            i, 1, 0, nullptr, nullptr, &rowSizeInBytes, &totalBytes);
        textureData.pData = texture->GetTextureDataAt(i);
        textureData.RowPitch = rowSizeInBytes;
        textureData.SlicePitch = totalBytes;

        D3D12UploadBuffer* tempBuffer = new D3D12UploadBuffer();
        pDevice->GetBufferManager()->AllocateUploadBuffer(tempBuffer, totalBytes);

        // Update texture data from upload buffer to gpu buffer.
        pCommandList->CopyTextureBuffer(texture->GetTextureBuffer()->GetResource(),
            tempBuffer->ResourceLocation.Resource.Get(), 0, i, 1, &textureData);
    }

    pCommandList->AddTransitionResourceBarriers(texture->GetTextureBuffer()->GetResource(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    pCommandList->FlushResourceBarriers();

    // Create the sampler and its view.
    texture->CreateSampler();
    pDevice->GetDescriptorHeapManager()->GetSamplerHandle(texture->TextureSampler.get(), id);
    pDevice->GetDevice()->CreateSampler(&texture->TextureSampler->SamplerDesc,
        texture->TextureSampler->CPUHandle);
}
