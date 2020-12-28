
#include "Application.h"
#include "Dx12/CommandListHelpers.h"

#include "Asserts.h"
#include "File.h"

#include <iostream>
#include <filesystem>

#include "pch.h"

#include <DirectXMath.h>

#include "Dx12/RootSignature.h"

#include "imgui.h"

#include "Mesh.h"
#include "Camera.h"

#include "Material.h"

namespace fs = std::filesystem;
using namespace Core;
using namespace DirectX;

namespace LightingModel
{
    enum : uint32_t
    {
        Ambient,
        Diffuse,
        Specular,
        Combined_Phong,
        NumLightingTypes,
    };
}

namespace RootParameters
{
    enum
    {
        InstanceDataSB,
        MaterialCB,
        DirectionLightCB,
        NumRootParameters,
    };
}

struct InstanceData
{
    XMMATRIX ModelMatrix;
    XMMATRIX ModelViewMatrix;
    XMMATRIX InverseTransposeModelViewMatrix;
    XMMATRIX ModelViewProjectionMatrix;

    uint32_t LightingModel;
    float padding[3];
};

struct DirectionLight
{
    XMFLOAT4 AmbientColour = { 0.15f, 0.15f, 0.15f, 1.0f };
    XMFLOAT3 Direction = { 0.0f, -1.0f, 1.0f };
    float padding = 0.0f;
};

struct LightMaterial
{
    XMFLOAT4 ObjectColour = {1.0f, 0.0f, 0.0f, 1.0f};
    XMFLOAT4 DiffuseColour = { 1.0f, 0.0f, 0.0f, 1.0f };
};

void XM_CALLCONV ComputeMatrices(FXMMATRIX model, CXMMATRIX view, CXMMATRIX viewProjection, InstanceData& mat)
{
    mat.ModelMatrix = model;
    mat.ModelViewMatrix = model * view;
    mat.InverseTransposeModelViewMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, mat.ModelViewMatrix));
    mat.ModelViewProjectionMatrix = model * viewProjection;
}

class LightingApp : public Dx12Application
{
public:
    LightingApp();

protected:
    void LoadContent() override;
    void Update(double deltaTime) override;
    void RenderScene(Dx12Texture& sceneTexture) override;
    void RenderUI() override;

private:
    void CreatePipelineStateObjects();

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
    std::unique_ptr<RootSignature> m_rootSignature;

    std::unique_ptr<Mesh> m_cubeMesh;

    RenderTarget m_sceneRenderTarget;
    std::array<FLOAT, 4> m_clearValue = { 0.4f, 0.6f, 0.9f, 1.0f };
    Camera m_camera;

    LightMaterial m_material = {};

    DirectionLight m_directionLighting;
};

CREATE_APPLICATION(LightingApp)

LightingApp::LightingApp()
{
}

void LightingApp::LoadContent()
{
    // -- Set up camera data ---
    XMVECTOR cameraPos = XMVectorSet(0, 5, -5, 1);
    XMVECTOR cameraTarget = XMVectorSet(0, 0, 0, 1);
    XMVECTOR cameraUp = XMVectorSet(0, 1, 0, 0);

    this->m_camera.SetLookAt(cameraPos, cameraTarget, cameraUp);

    float aspectRatio = this->m_window->GetWidth() / static_cast<float>(this->m_window->GetHeight());
    this->m_camera.SetProjection(45.0f, aspectRatio, 0.1f, 100.0f);

    // -- Upload data to the gpu ---
    auto copyQueue = this->m_renderDevice->GetQueue(D3D12_COMMAND_LIST_TYPE_COPY);
    auto uploadCmdList = copyQueue->GetCommandList();

    // Setup Render Target
    {
        // Create an HDR intermediate render target.
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

        // Create an off-screen render target with a single color buffer and a depth buffer.
        auto colorResourceDesc =
            CD3DX12_RESOURCE_DESC::Tex2D(
                format,
                this->m_window->GetWidth(),
                this->m_window->GetHeight());

        colorResourceDesc.MipLevels = 1;

        colorResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE colorClearValue;
        colorClearValue.Format = colorResourceDesc.Format;
        colorClearValue.Color[0] = this->m_clearValue[0];
        colorClearValue.Color[1] = this->m_clearValue[1];
        colorClearValue.Color[2] = this->m_clearValue[2];
        colorClearValue.Color[3] = this->m_clearValue[3];

        Dx12Texture colourTexture(this->m_renderDevice, colorResourceDesc, &colorClearValue);
        colourTexture.CreateViews();

        this->m_sceneRenderTarget.AttachTexture(AttachmentPoint::Color0, colourTexture);
    }

    {
        DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;

        D3D12_CLEAR_VALUE optimizedClearValue = {};
        optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        optimizedClearValue.DepthStencil = { 1.0f, 0 };

        // Create a depth buffer for the HDR render target.
        auto depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            depthBufferFormat,
            this->m_window->GetWidth(),
            this->m_window->GetHeight());

        depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        Dx12Texture depthTexture(this->m_renderDevice, depthDesc, &optimizedClearValue);
        depthTexture.CreateViews();

        this->m_sceneRenderTarget.AttachTexture(AttachmentPoint::DepthStencil, depthTexture);
    }

    this->m_cubeMesh = MeshPrefabs::CreateCube(this->m_renderDevice, *uploadCmdList);

    uint64_t uploadFence = copyQueue->ExecuteCommandList(uploadCmdList);

    this->CreatePipelineStateObjects();

    copyQueue->WaitForFenceValue(uploadFence);
}

void LightingApp::Update(double deltaTime)
{
}

void LightingApp::RenderScene(Dx12Texture& sceneTexture)
{
    auto commandList = this->m_renderDevice->GetQueue()->GetCommandList();

    commandList->ClearRenderTarget(
        this->m_sceneRenderTarget.GetTexture(Color0),
        this->m_clearValue);

    commandList->ClearDepthStencilTexture(
            this->m_sceneRenderTarget.GetTexture(AttachmentPoint::DepthStencil),
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);

    static CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, this->m_window->GetWidth(), this->m_window->GetHeight());
    commandList->SetViewport(viewport);

    static CD3DX12_RECT rect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
    commandList->SetScissorRect(rect);

    // Set Render Target
    commandList->SetRenderTarget(this->m_sceneRenderTarget);

    commandList->SetGraphicsRootSignature(*this->m_rootSignature);
    commandList->SetPipelineState(this->m_pso);

    XMMATRIX rotationMatrix = XMMatrixRotationZ(10.0f);
    XMMATRIX scaleMatrix = XMMatrixScaling(1.0f, 1.0f, 1.0f);
    XMMATRIX viewMatrix = this->m_camera.GetViewMatrix();
    XMMATRIX viewProjectionMatrix = viewMatrix * this->m_camera.GetProjectionMatrix();

    std::vector<InstanceData> instanceData(LightingModel::NumLightingTypes);
    for (int i = 0; i < LightingModel::NumLightingTypes; i++)
    {
        instanceData[i].LightingModel = i;

        // Set Matrix data
        XMMATRIX translationMatrix = XMMatrixTranslation(-3.0f + i * 2, 0.0f, 0.0f);
        XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
        ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, instanceData[i]);

    }
    
    commandList->SetGraphicsDynamicStructuredBuffer(RootParameters::InstanceDataSB, instanceData);
    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, this->m_material);
    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::DirectionLightCB, this->m_directionLighting);

    // -- Draw Ambient Mesh ---
    this->m_cubeMesh->Draw(*commandList, LightingModel::NumLightingTypes);

    this->m_renderDevice->GetQueue()->ExecuteCommandList(commandList);
    sceneTexture.SetDx12Resource(this->m_sceneRenderTarget.GetTexture(Color0).GetDx12Resource());
}

void LightingApp::RenderUI()
{
    static bool showWindow = true;
    ImGui::Begin("Shader Parameters", &showWindow, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::CollapsingHeader("Direction Lighting");

    ImGui::ColorEdit3("Colour", reinterpret_cast<float*>(&this->m_directionLighting.AmbientColour));

    ImGui::DragFloat3("Direction", reinterpret_cast<float*>(&this->m_directionLighting.Direction), 0.01f, -1.0f, 1.0f);
    
    ImGui::NewLine();

    ImGui::CollapsingHeader("Material Info");

    // color picker
    ImGui::ColorEdit3("Diffuse Colour", reinterpret_cast<float*>(&this->m_material.DiffuseColour));

    ImGui::End();

}

void LightingApp::CreatePipelineStateObjects()
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

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRage(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

	CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
    rootParameters[RootParameters::InstanceDataSB].InitAsShaderResourceView(0);
    rootParameters[RootParameters::MaterialCB].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[RootParameters::DirectionLightCB].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

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

    // Setup the pipeline state.
    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS VS;
        CD3DX12_PIPELINE_STATE_STREAM_PS PS;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
    } pipelineStateStream;


    std::string baseAssetPath(fs::current_path().u8string());
    std::vector<char> vertexByteData;
    bool success = BinaryReader::ReadFile(baseAssetPath + "\\" + "SimpleLightingVS.cso", vertexByteData);
    CORE_FATAL_ASSERT(success, "Failed to read Vertex Shader");


    std::vector<char> pixelByteData;
    success = BinaryReader::ReadFile(baseAssetPath + "\\" + "SimpleLightingPS.cso", pixelByteData);
    CORE_FATAL_ASSERT(success, "Failed to read Pixel Shader");

    pipelineStateStream.pRootSignature = this->m_rootSignature->GetRootSignature().Get();
    pipelineStateStream.InputLayout = { VertexPositionNormalTexture::InputElements, VertexPositionNormalTexture::InputElementCount };
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexByteData.data(), vertexByteData.size());
    pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelByteData.data(), pixelByteData.size());
    pipelineStateStream.DSVFormat = this->m_sceneRenderTarget.GetDepthStencilFormat();
    pipelineStateStream.RTVFormats = this->m_sceneRenderTarget.GetRenderTargetForamts();


    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc =
    {
        sizeof(PipelineStateStream), &pipelineStateStream
    };

    ThrowIfFailed(
        this->m_renderDevice->GetD3DDevice()->CreatePipelineState(
            &pipelineStateStreamDesc,
            IID_PPV_ARGS(&this->m_pso)));
}