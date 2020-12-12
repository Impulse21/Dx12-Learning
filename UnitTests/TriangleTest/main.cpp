
#include "Application.h"
#include "Dx12/CommandListHelpers.h"

#include "Asserts.h"
#include "File.h"

#include <iostream>
#include <filesystem>

#include "pch.h"

#include <DirectXMath.h>

namespace fs = std::filesystem;
using namespace Core;
using namespace DirectX;


struct VertexPosColour
{
    XMFLOAT3 Position;
    XMFLOAT3 Colour;
};

static std::vector<VertexPosColour> gVertices =
{
    { XMFLOAT3(0.0f, 0.5f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
    { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
    { XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) }
};

static std::vector<uint16_t> gIndices = { 0, 2, 1 };

class TriangleTestApp : public Dx12Application
{
public:
    TriangleTestApp();

protected:
    void LoadContent() override;
    void Update(double deltaTime) override;
    void Render() override;

private:
    void CreatePipelineStateObjects();

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_emptyRootSignature;

    // Vertex buffer for the cube.
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};

    // Index buffer for the cube.
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
};

CREATE_APPLICATION(TriangleTestApp)

TriangleTestApp::TriangleTestApp()
{
}

void TriangleTestApp::LoadContent()
{
    auto uploadCmdList = this->m_copyQueue->GetCommandList();

    uploadCmdList->CopyBuffer(
        this->m_vertexBuffer,
        gVertices.size(),
        sizeof(VertexPosColour),
        gVertices.data());

    this->m_vertexBufferView.BufferLocation = this->m_vertexBuffer->GetGPUVirtualAddress();
    this->m_vertexBufferView.StrideInBytes = sizeof(VertexPosColour);
    this->m_vertexBufferView.SizeInBytes = sizeof(VertexPosColour) * gVertices.size();

    Microsoft::WRL::ComPtr<ID3D12Resource> indexUploadResource;

    uploadCmdList->CopyBuffer(
        this->m_indexBuffer,
        gIndices.size(),
        sizeof(uint16_t),
        gIndices.data());

    this->m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    this->m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    this->m_indexBufferView.SizeInBytes = sizeof(uint16_t) * gIndices.size();

    uint64_t uploadFence = this->m_copyQueue->ExecuteCommandList(uploadCmdList);

    this->CreatePipelineStateObjects();

    this->m_copyQueue->WaitForFenceValue(uploadFence);
}

void TriangleTestApp::Update(double deltaTime)
{
}

void TriangleTestApp::Render()
{
    auto commandList = this->m_directQueue->GetCommandList();
    auto currentBackBuffer = this->m_swapChain->GetCurrentBackBuffer();
    auto rtv = this->m_rtvDescriptorHeap->GetCpuHandle(this->m_swapChain->GetCurrentBufferIndex());

    commandList->ClearRenderTarget(currentBackBuffer, rtv, { 0.4f, 0.6f, 0.9f, 1.0f });

    static CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, this->m_window->GetWidth(), this->m_window->GetHeight());
    commandList->SetViewport(viewport);

    static CD3DX12_RECT rect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
    commandList->SetScissorRect(rect);

    // Set Render Target
    commandList->SetRenderTarget(currentBackBuffer, rtv);

    commandList->SetGraphicsRootSignature(this->m_emptyRootSignature);

    commandList->SetPipelineState(this->m_pso);

    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetVertexBuffer(this->m_vertexBuffer, this->m_vertexBufferView);
    commandList->SetIndexBuffer(this->m_indexBuffer, this->m_indexBufferView);

    commandList->DrawIndexed(gIndices.size(), 1, 0, 0, 0);

    // Prepare Render target for present
    {
        commandList->TransitionBarrier(currentBackBuffer, D3D12_RESOURCE_STATE_PRESENT);

        uint64_t commandFence = this->m_directQueue->ExecuteCommandList(commandList);
        this->SetCurrentFrameFence(commandFence);
    }
}

void TriangleTestApp::CreatePipelineStateObjects()
{
    // -- Create Root Signature ---
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(this->m_d3d12Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // Serialize the root signature.
    Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    ThrowIfFailed(
        D3DX12SerializeVersionedRootSignature(
            &rootSignatureDescription,
            featureData.HighestVersion,
            &rootSignatureBlob,
            &errorBlob));

    // Create the root signature.
    ThrowIfFailed(
        this->m_d3d12Device->CreateRootSignature(
            0,
            rootSignatureBlob->GetBufferPointer(),
            rootSignatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&this->m_emptyRootSignature)));


    // Create the vertex input layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOUR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    PipelineStateBuilder builder;
    builder.SetInputElementDesc(inputLayout);
    builder.SetRootSignature(this->m_emptyRootSignature.Get());
    std::string baseAssetPath(fs::current_path().u8string());

    std::vector<char> vertexByteData;
    bool success = BinaryReader::ReadFile(baseAssetPath + "\\" + "TriangleVS.cso", vertexByteData);
    CORE_FATAL_ASSERT(success, "Failed to read Vertex Shader");

    builder.SetVertexShader(vertexByteData);

    std::vector<char> pixelByteData;
    success = BinaryReader::ReadFile(baseAssetPath + "\\" + "TrianglePS.cso", pixelByteData);
    CORE_FATAL_ASSERT(success, "Failed to read Pixel Shader");
    builder.SetPixelShader(pixelByteData);

    builder.SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    builder.SetRenderTargetFormat(this->m_swapChain->GetFormat());


    this->m_pso = builder.Build(this->m_d3d12Device.Get(), L"Pipeline state");
}
