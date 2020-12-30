
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
        Phong,
        NumLightModels,
    };
}

namespace RootParameters
{
    enum
    {
        MatricesCB,
        MaterialCB,
        DirectionalLightCB,
        CameraDataCB,
        NumRootParameters,
    };
}

struct Matrices
{
    XMMATRIX ModelMatrix;
    XMMATRIX ModelViewMatrix;
    XMMATRIX InverseTransposeModelViewMatrix;
    XMMATRIX ModelViewProjectionMatrix;
};

struct DirectionalLight
{
    XMFLOAT4 AmbientColour = { 0.2f, 0.2f, 0.2f, 1.0f };
    XMFLOAT4 DiffuseColour = { 0.5f, 0.5f, 0.5f, 1.0f };
    XMFLOAT4 SpecularColour = { 1.0f, 1.0f, 1.0f, 1.0f };

    XMFLOAT3 Direction = { 0.0f, -1.0f, 1.0f };

    float padding = 0.0f;
};

struct CameraData
{
    XMVECTOR Position;
};

void XM_CALLCONV ComputeMatrices(FXMMATRIX model, CXMMATRIX view, CXMMATRIX viewProjection, Matrices& mat)
{
    mat.ModelMatrix = model;
    mat.ModelViewMatrix = model * view;
    mat.InverseTransposeModelViewMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, mat.ModelViewMatrix));
    mat.ModelViewProjectionMatrix = model * viewProjection;
}

class PhongLightingApp : public Dx12Application
{
public:
    PhongLightingApp();

protected:
    void LoadContent() override;
    void Update(double deltaTime) override;
    void RenderScene(Dx12Texture& sceneTexture) override;
    void RenderUI() override;

private:
    void CreateLightModelPSO();
    std::unique_ptr<RootSignature> CreateRootSignature(
        uint32_t numParameters,
        CD3DX12_ROOT_PARAMETER1* rootParameters,
        uint32_t numSamplers = 0,
        CD3DX12_STATIC_SAMPLER_DESC* samplers = nullptr);

    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePipelineStateObject(
        RootSignature const& rootSignature,
        std::string const& shaderName);

private:
    RenderTarget m_sceneRenderTarget;

    std::array<FLOAT, 4> m_clearValue = { 0.0f, 0.0f, 0.0f, 1.0f };
    Camera m_camera;


    std::vector<Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_lightModelPso;
    std::vector<std::unique_ptr<RootSignature>> m_rootSignature;

    std::unique_ptr<Mesh> m_sphereMesh;

    int m_selectedLightModel = LightingModel::Phong;

    Material m_material = Material::Red;

    DirectionalLight m_sun;
};

CREATE_APPLICATION(PhongLightingApp)

PhongLightingApp::PhongLightingApp()
{
}

void PhongLightingApp::LoadContent()
{
    // -- Set up camera data ---
    XMVECTOR cameraPos = XMVectorSet(0, 0, -5, 1);
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

    this->m_sphereMesh = MeshPrefabs::CreateSphere(this->m_renderDevice, *uploadCmdList, 3.0f);

    uint64_t uploadFence = copyQueue->ExecuteCommandList(uploadCmdList);

    
    this->CreateLightModelPSO();

    copyQueue->WaitForFenceValue(uploadFence);
}

void PhongLightingApp::Update(double deltaTime)
{
}

void PhongLightingApp::RenderScene(Dx12Texture& sceneTexture)
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

    // -- Set pipeline state ---
    commandList->SetGraphicsRootSignature(*this->m_rootSignature[m_selectedLightModel]);
    commandList->SetPipelineState(this->m_lightModelPso[m_selectedLightModel]);

    // Set Matrix data
    XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
    XMMATRIX rotationMatrix = XMMatrixIdentity();
    XMMATRIX scaleMatrix = XMMatrixScaling(1.0f, 1.0f, 1.0f);
    XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
    XMMATRIX viewMatrix = this->m_camera.GetViewMatrix();
    XMMATRIX viewProjectionMatrix = viewMatrix * this->m_camera.GetProjectionMatrix();

    Matrices matrices;
    ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

    // -- Set Pipeline state parameters ---
    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCB, matrices);
    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCB, this->m_material);
    commandList->SetGraphicsDynamicConstantBuffer(RootParameters::DirectionalLightCB, this->m_sun);

    CameraData cameraData = {};
    cameraData.Position = this->m_camera.GetTranslation();
    commandList->SetGraphics32BitConstants(RootParameters::CameraDataCB, cameraData);

    // -- Draw Ambient Mesh ---
    this->m_sphereMesh->Draw(*commandList);

    this->m_renderDevice->GetQueue()->ExecuteCommandList(commandList);
    sceneTexture.SetDx12Resource(this->m_sceneRenderTarget.GetTexture(Color0).GetDx12Resource());
}

void PhongLightingApp::RenderUI()
{
    static bool showWindow = true;
    ImGui::Begin("Options", &showWindow, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Combo("Lighting Mode", &this->m_selectedLightModel, "Phong");

    ImGui::NewLine();
    ImGui::CollapsingHeader("Material Parameters");

    switch (this->m_selectedLightModel)
    {
    case LightingModel::Phong:

        ImGui::ColorEdit3("Material Ambient Colour", reinterpret_cast<float*>(&this->m_material.Ambient));
        ImGui::ColorEdit3("Material Diffuse Colour", reinterpret_cast<float*>(&this->m_material.Diffuse));
        ImGui::ColorEdit3("Material Specular Colour", reinterpret_cast<float*>(&this->m_material.Specular));
        ImGui::DragFloat("Material Shininess", &this->m_material.Shininess, 0.1f, 0.0f, 256.0f);

        break;
    default:
        ImGui::Text("No parameters");
    }

    ImGui::NewLine();


    ImGui::CollapsingHeader("Sun Parameters");
    ImGui::ColorEdit3("Sun Ambient Colour", reinterpret_cast<float*>(&this->m_sun.AmbientColour));
    ImGui::ColorEdit3("Sun Diffuse Colour", reinterpret_cast<float*>(&this->m_sun.DiffuseColour));
    ImGui::ColorEdit3("Sun Specular Colour", reinterpret_cast<float*>(&this->m_sun.SpecularColour));
    ImGui::DragFloat3("Sun Direction", reinterpret_cast<float*>(&this->m_sun.Direction), 0.01f, -1.0f, 1.0f);
    ImGui::End();
}

void PhongLightingApp::CreateLightModelPSO()
{
    this->m_rootSignature.resize(LightingModel::NumLightModels);
    this->m_lightModelPso.resize(LightingModel::NumLightModels);

    for (int i = 0; i < LightingModel::NumLightModels; i++)
    {
        switch (i)
        {
        case LightingModel::Phong:

            CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
            rootParameters[RootParameters::MatricesCB].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
            rootParameters[RootParameters::MaterialCB].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
            rootParameters[RootParameters::DirectionalLightCB].InitAsConstantBufferView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
            rootParameters[RootParameters::CameraDataCB].InitAsConstants(sizeof(CameraData) / 4, 3, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);

            this->m_rootSignature[i] = this->CreateRootSignature(RootParameters::NumRootParameters, rootParameters);
            this->m_lightModelPso[i] = this->CreatePipelineStateObject(*this->m_rootSignature[i], "PhongLighting");
            break;

        default:
            LOG_CORE_WARN("Lighting model not implmented yet. Skipping");
            continue;
        }
    }
}

std::unique_ptr<RootSignature> PhongLightingApp::CreateRootSignature(
    uint32_t numParameters,
    CD3DX12_ROOT_PARAMETER1* rootParameters,
    uint32_t numSamplers,
    CD3DX12_STATIC_SAMPLER_DESC* samplers)
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

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(
        numParameters,
        rootParameters,
        numSamplers,
        samplers,
        rootSignatureFlags);

    return std::make_unique<RootSignature>(
        this->m_renderDevice,
        rootSignatureDescription.Desc_1_1,
        featureData.HighestVersion);
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> PhongLightingApp::CreatePipelineStateObject(
    RootSignature const& rootSignature,
    std::string const& shaderName)
{
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
    bool success = BinaryReader::ReadFile(baseAssetPath + "\\" + shaderName + "VS.cso", vertexByteData);
    CORE_FATAL_ASSERT(success, "Failed to read Vertex Shader");


    std::vector<char> pixelByteData;
    success = BinaryReader::ReadFile(baseAssetPath + "\\" + shaderName + "PS.cso", pixelByteData);
    CORE_FATAL_ASSERT(success, "Failed to read Pixel Shader");

    pipelineStateStream.pRootSignature = rootSignature.GetRootSignature().Get();
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

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
    ThrowIfFailed(
        this->m_renderDevice->GetD3DDevice()->CreatePipelineState(
            &pipelineStateStreamDesc,
            IID_PPV_ARGS(&pso)));

    return pso;
}