
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


struct VertexPosTex
{
    XMFLOAT3 Position;
    XMFLOAT3 Colour;
};

static std::vector<VertexPosTex> gVertices =
{
    { XMFLOAT3(0.0f, 0.5f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
    { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
    { XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) }
};

static std::vector<uint16_t> gIndices = { 0, 2, 1 };

class TexturedTriangleTestApp : public Dx12Application
{
public:
    TexturedTriangleTestApp();

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
    std::unique_ptr<Dx12Buffer> m_vertexBuffer = nullptr;

    // Index buffer for the cube.
    std::unique_ptr<Dx12Buffer> m_indexBuffer = nullptr;
};

CREATE_APPLICATION(TexturedTriangleTestApp)

TexturedTriangleTestApp::TexturedTriangleTestApp()
{
}

void TexturedTriangleTestApp::LoadContent()
{
    auto copyQueue = this->m_renderDevice->GetQueue(D3D12_COMMAND_LIST_TYPE_COPY);
    auto uploadCmdList = copyQueue->GetCommandList();

    {
        BufferDesc desc = {};
        desc.Usage = BufferUsage::Static;
        desc.BindFlags = BIND_VERTEX_BUFFER;
        desc.ElementByteStride = sizeof(VertexPosTex);
        desc.NumElements = gVertices.size();

        this->m_vertexBuffer = std::make_unique<Dx12Buffer>(this->m_renderDevice, desc);

        uploadCmdList->CopyBuffer<VertexPosTex>(
            *this->m_vertexBuffer,
            gVertices);
    }

    {
        BufferDesc desc = {};
        desc.Usage = BufferUsage::Static;
        desc.BindFlags = BIND_INDEX_BUFFER;
        desc.ElementByteStride = sizeof(uint16_t);
        desc.NumElements = gIndices.size();

        this->m_indexBuffer = std::make_unique<Dx12Buffer>(this->m_renderDevice, desc);

        uploadCmdList->CopyBuffer<uint16_t>(
            *this->m_indexBuffer,
            gIndices);
    }

    uint64_t uploadFence = copyQueue->ExecuteCommandList(uploadCmdList);

    this->CreatePipelineStateObjects();

    copyQueue->WaitForFenceValue(uploadFence);
}

void TexturedTriangleTestApp::Update(double deltaTime)
{
}

void TexturedTriangleTestApp::Render()
{
    auto commandList = this->m_renderDevice->GetQueue()->GetCommandList();
    auto currentBackBuffer = this->m_swapChain->GetCurrentBackBuffer();
    auto rtv = this->m_swapChain->GetCurrentRenderTargetView();

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
    commandList->SetVertexBuffer(*this->m_vertexBuffer);
    commandList->SetIndexBuffer(*this->m_indexBuffer);

    commandList->DrawIndexed(gIndices.size(), 1, 0, 0, 0);

    // Prepare Render target for present
    {
        commandList->TransitionBarrier(currentBackBuffer, D3D12_RESOURCE_STATE_PRESENT);

        uint64_t commandFence = this->m_renderDevice->GetQueue()->ExecuteCommandList(commandList);
        this->SetCurrentFrameFence(commandFence);
    }
}

void TexturedTriangleTestApp::CreatePipelineStateObjects()
{
    // -- Create Root Signature ---
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(this->m_renderDevice->GetD3DDevice()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
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
        this->m_renderDevice->GetD3DDevice()->CreateRootSignature(
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

    this->m_pso = builder.Build(this->m_renderDevice->GetD3DDevice().Get(), L"Pipeline state");
}
