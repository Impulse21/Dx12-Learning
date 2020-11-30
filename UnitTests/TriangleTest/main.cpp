
#include "Application.h"
#include "Dx12/CommandListHelpers.h"

#include "Asserts.h"
#include "File.h"

#include <iostream>
#include <filesystem>

#include "pch.h"

namespace fs = std::filesystem;


using namespace Core;

class TriangleTestApp : public Dx12Application
{
public:
    TriangleTestApp();

protected:
    void LoadContent() override;
    void Update(double deltaTime) override;
    void Render() override;

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_emptyRootSignature;
};

CREATE_APPLICATION(TriangleTestApp)

TriangleTestApp::TriangleTestApp()
{
}

void TriangleTestApp::LoadContent()
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


    PipelineStateBuilder builder;

    builder.SetRootSignature(m_emptyRootSignature.Get());
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

void TriangleTestApp::Update(double deltaTime)
{
}

void TriangleTestApp::Render()
{
    auto commandList = this->m_directQueue->GetCommandList();
    auto currentBackBuffer = this->m_swapChain->GetCurrentBackBuffer();
    auto rtv = this->m_rtvDescriptorHeap->GetCpuHandle(this->m_swapChain->GetCurrentBufferIndex());

    // Clear the render targets.
    {
        CommandListHelpers::TransitionResource(
            commandList,
            currentBackBuffer,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);


        CommandListHelpers::ClearRenderTarget(commandList, rtv, { 0.4f, 0.6f, 0.9f, 1.0f });
        // ClearDepth(commandList, dsv);
    }

    commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    static CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, this->m_window->GetWidth(), this->m_window->GetHeight());
    commandList->RSSetViewports(1, &viewport);

    static CD3DX12_RECT rect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
    commandList->RSSetScissorRects(1, &rect);

    // Set Render Target
    commandList->OMSetRenderTargets(1, &rtv, false, nullptr);

    commandList->SetPipelineState(this->m_pso.Get());

    commandList->DrawInstanced(3, 1, 0, 0);

    // Prepare Render target for present
    {
        CommandListHelpers::TransitionResource(
            commandList,
            currentBackBuffer,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

        uint64_t commandFence = this->m_directQueue->ExecuteCommandList(commandList);
        this->SetCurrentFrameFence(commandFence);
    }
}
