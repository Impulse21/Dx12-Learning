
#include "Application.h"
#include "Dx12/CommandListHelpers.h"

#include "Asserts.h"
#include "File.h"

#include <iostream>
#include <filesystem>

#include "pch.h"

#include <DirectXMath.h>

#include "Dx12/RootSignature.h"

namespace fs = std::filesystem;
using namespace Core;
using namespace DirectX;


struct VertexPosTex
{
    XMFLOAT3 Position;
    XMFLOAT2 Colour;
};

static std::vector<VertexPosTex> gVertices =
{
    { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
    { XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.5f, 0.0f) },
    { XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }
};

static std::vector<uint16_t> gIndices = { 0, 1, 2 };

namespace RootParameters
{
    enum
    {
        Textures,
        NumRootParameters,
    };
}
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
    std::unique_ptr<RootSignature> m_rootSignature;

    // Vertex buffer for the cube.
    std::unique_ptr<Dx12Buffer> m_vertexBuffer = nullptr;

    // Index buffer for the cube.
    std::unique_ptr<Dx12Buffer> m_indexBuffer = nullptr;

    // Index buffer for the cube.
    std::unique_ptr<Dx12Texture> m_texture = nullptr;
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

	{
		TextureDesc desc = {};
		desc.Binding = BIND_SHADER_RESOURCE;
        this->m_texture = std::make_unique<Dx12Texture>(this->m_renderDevice, desc);

		std::wstring baseAssetPath(L"D:\\Users\\C.DiPaolo\\Development\\DX12\\Dx12-Learning\\Assets\\");
		uploadCmdList->LoadTextureFromFile(*this->m_texture, baseAssetPath + L"UV_Test_Pattern.dds");
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

    commandList->SetGraphicsRootSignature(*this->m_rootSignature);
    commandList->SetPipelineState(this->m_pso);

    commandList->SetShaderResourceView(RootParameters::Textures, 0, *this->m_texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

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

	// Allow input layout and deny unnecessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRage(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
	rootParameters[RootParameters::Textures].InitAsDescriptorTable(1, &descriptorRage, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC linearRepeatSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(
		RootParameters::NumRootParameters,
		rootParameters,
		1,
		&linearRepeatSampler,
		rootSignatureFlags);

	this->m_rootSignature = std::make_unique<RootSignature>(
		this->m_renderDevice,
		rootSignatureDescription.Desc_1_1,
		featureData.HighestVersion);

    // Create the vertex input layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    PipelineStateBuilder builder;
    builder.SetInputElementDesc(inputLayout);
    builder.SetRootSignature(this->m_rootSignature->GetRootSignature().Get());
    std::string baseAssetPath(fs::current_path().u8string());

    std::vector<char> vertexByteData;
    bool success = BinaryReader::ReadFile(baseAssetPath + "\\" + "TexturedTriVS.cso", vertexByteData);
    CORE_FATAL_ASSERT(success, "Failed to read Vertex Shader");

    builder.SetVertexShader(vertexByteData);

    std::vector<char> pixelByteData;
    success = BinaryReader::ReadFile(baseAssetPath + "\\" + "TexturedTriPS.cso", pixelByteData);
    CORE_FATAL_ASSERT(success, "Failed to read Pixel Shader");
    builder.SetPixelShader(pixelByteData);

    builder.SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    builder.SetRenderTargetFormat(this->m_swapChain->GetFormat());

    this->m_pso = builder.Build(this->m_renderDevice->GetD3DDevice().Get(), L"Pipeline state");
}
