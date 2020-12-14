
#include "Application.h"
#include "Dx12/CommandListHelpers.h"

#include "Asserts.h"
#include "File.h"

#include <iostream>
#include <filesystem>

#include "pch.h"

#include <DirectXMath.h>

#include <math.h>       /* sin */

namespace fs = std::filesystem;
using namespace Core;
using namespace DirectX;


struct VertexPosColour
{
    XMFLOAT3 Position;
    XMFLOAT3 Colour;
};

__declspec(align(16)) struct TriangleOffsetCB
{
    XMFLOAT2 Offset;
};

static_assert((sizeof(TriangleOffsetCB) % (sizeof(float) * 4)) == 0, "Invalid struture size");

static std::vector<VertexPosColour> gVertices =
{
    { XMFLOAT3(0.0f, -0.5f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
    { XMFLOAT3(-0.5f, 0.5f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
    { XMFLOAT3(0.5f, 0.5f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) }
};

static std::vector<uint16_t> gIndices = { 0, 1, 2 };

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
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

    // Vertex buffer for the cube.
    std::unique_ptr<Dx12Buffer> m_vertexBuffer = nullptr;

    // Index buffer for the cube.
    std::unique_ptr<Dx12Buffer> m_indexBuffer = nullptr;

    TriangleOffsetCB m_triangleCB = {};
};

CREATE_APPLICATION(TriangleTestApp)

TriangleTestApp::TriangleTestApp()
{
}

void TriangleTestApp::LoadContent()
{
    this->m_triangleCB.Offset = { 0.0f, 0.0f };

    auto uploadCmdList = this->m_copyQueue->GetCommandList();

    {
        BufferDesc desc = {};
        desc.Usage = Usage::Static;
        desc.BindFlags = BIND_VERTEX_BUFFER;
        desc.ElementByteStride = sizeof(VertexPosColour);
        desc.NumElements = gVertices.size();

        this->m_vertexBuffer = std::make_unique<Dx12Buffer>(desc);

        uploadCmdList->CopyBuffer<VertexPosColour>(
            *this->m_vertexBuffer,
            gVertices);
    }

    {
        BufferDesc desc = {};
        desc.Usage = Usage::Static;
        desc.BindFlags = BIND_INDEX_BUFFER;
        desc.ElementByteStride = sizeof(uint16_t);
        desc.NumElements = gIndices.size();

        this->m_indexBuffer = std::make_unique<Dx12Buffer>(desc);

        uploadCmdList->CopyBuffer<uint16_t>(
            *this->m_indexBuffer,
            gIndices);
    }

    uint64_t uploadFence = this->m_copyQueue->ExecuteCommandList(uploadCmdList);

    this->CreatePipelineStateObjects();

    this->m_copyQueue->WaitForFenceValue(uploadFence);
}

void TriangleTestApp::Update(double deltaTime)
{
    static float updateTick = 0;
    m_triangleCB.Offset.x = sin(updateTick * 3.14159265/ 180);
    // m_triangleCB.Offset.y = sin(updateTick * 3.14159265 / 180);
    updateTick += 0.1;
   
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

    commandList->SetGraphicsRootSignature(this->m_rootSignature);
    commandList->SetGraphics32BitConstants<TriangleOffsetCB>(0, this->m_triangleCB);
    commandList->SetPipelineState(this->m_pso);


    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetVertexBuffer(*this->m_vertexBuffer);
    commandList->SetIndexBuffer(*this->m_indexBuffer);


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

    // A single 32-bit constant root parameter that is used by the vertex shader.
    CD3DX12_ROOT_PARAMETER1 rootParameters[1] = {};
    rootParameters[0].InitAsConstants(sizeof(TriangleOffsetCB) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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
            IID_PPV_ARGS(&this->m_rootSignature)));


    // Create the vertex input layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOUR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    PipelineStateBuilder builder;
    builder.SetInputElementDesc(inputLayout);
    builder.SetRootSignature(this->m_rootSignature.Get());
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
