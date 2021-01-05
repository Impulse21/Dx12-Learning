
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

namespace PbrRootParameters
{
    enum
    {
        MatricesCB,
        CameraDataCB,
        MaterialCB,
        LightPropertiesCB,
        PointLightsSB,
        Textures,
        NumRootParameters,
    };
}

namespace SkyboxRootParammeters
{
    enum
    {
        MatricesCB,
        Textures,
        NumRootParameters,
    };
}

enum class SkyboxState
{
    Disable = 0,
    DrawCubemap,
    DrawIrrandanceMap,
};
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

struct PointLight
{
    XMFLOAT4 Position = { 0.0f, 0.0f, -5.0f, 1.0f };
    XMFLOAT4 Colour = { 1.0f, 1.0f, 1.0f, 1.0f };

    float AttenuationConstant = 1.0f;
    float AttenuationLinear = 0.09;
    float AttenuationQuadratic = 0.032f;
};

struct LightProperties
{
    uint32_t numPointLights;
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

class BRDFLightingIBLApp : public Dx12Application
{
public:
    BRDFLightingIBLApp();

protected:
    void LoadContent() override;
    void Update(double deltaTime) override;
    void RenderScene(Dx12Texture& sceneTexture) override;
    void RenderUI() override;

private:
    void CreatePipelineStateObjects();
    std::unique_ptr<RootSignature> CreateRootSignature(
        uint32_t numParameters,
        CD3DX12_ROOT_PARAMETER1* rootParameters,
        uint32_t numSamplers = 0,
        CD3DX12_STATIC_SAMPLER_DESC* samplers = nullptr);

    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePipelineStateObject(
        RootSignature const& rootSignature,
        std::string const& vertexShaderName,
        std::string const& pixelShanderName);

private:
    RenderTarget m_hdrRenderTarget;

    std::array<FLOAT, 4> m_clearValue = { 0.0f, 0.0f, 0.0f, 1.0f };
    Camera m_camera;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_lightModelPso;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_skyboxPso;

    std::unique_ptr<RootSignature> m_rootSignature;
    std::unique_ptr<RootSignature> m_skyboxSignature;

    std::unique_ptr<Mesh> m_skyboxMesh;
    std::unique_ptr<Mesh> m_sphereMesh;

    // Index buffer for the cube
    std::unique_ptr<Dx12Texture> m_cathedralTexture = nullptr;
    std::unique_ptr<Dx12Texture> m_cathedralCubeMap = nullptr;
    std::unique_ptr<Dx12Texture> m_cathedralIrradianceMap = nullptr;
    std::unique_ptr<Dx12Texture> m_cathedralSpecularMap = nullptr;
    std::unique_ptr<Dx12Texture> m_specularBrdfLut = nullptr;

    PbrMaterial m_material = PbrMaterial({1.0f, 0.0f, 0.0f, 1.0f});

    const size_t MaxLights = 4;
    std::vector<PointLight> m_pointLights;

    DirectionalLight m_sun;
};

CREATE_APPLICATION(BRDFLightingIBLApp)

BRDFLightingIBLApp::BRDFLightingIBLApp()
{
}

void BRDFLightingIBLApp::LoadContent()
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
        // TODOL HDR Format and tone mapping
        DXGI_FORMAT hdrFormat = DXGI_FORMAT_R8G8B8A8_UNORM; //DXGI_FORMAT_R16G16B16A16_FLOAT;

        // Create an off-screen render target with a single color buffer and a depth buffer.
        auto colorResourceDesc =
            CD3DX12_RESOURCE_DESC::Tex2D(
                hdrFormat,
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

        Dx12Texture hdrTexture(this->m_renderDevice, colorResourceDesc, &colorClearValue);
        hdrTexture.CreateViews();

        this->m_hdrRenderTarget.AttachTexture(AttachmentPoint::Color0, hdrTexture);
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

        this->m_hdrRenderTarget.AttachTexture(AttachmentPoint::DepthStencil, depthTexture);
    }

    {
        this->m_cathedralTexture = std::make_unique<Dx12Texture>(this->m_renderDevice);

        std::wstring baseAssetPath(L"D:\\Users\\C.DiPaolo\\Development\\DX12\\Dx12-Learning\\Assets\\Textures\\hdr\\");
        uploadCmdList->LoadTextureFromFile(*this->m_cathedralTexture, baseAssetPath + L"grace-new.hdr");

        // Create a cubemap for the HDR panorama.
        auto cubemapDesc = this->m_cathedralTexture->GetDx12Resource()->GetDesc();
        cubemapDesc.Width = cubemapDesc.Height = 1024;
        cubemapDesc.DepthOrArraySize = 6;
        cubemapDesc.MipLevels = 0;

        this->m_cathedralCubeMap = std::make_unique<Dx12Texture>(this->m_renderDevice, cubemapDesc);
        uploadCmdList->PanoToCubemap(*this->m_cathedralCubeMap, *this->m_cathedralTexture);

        // Generate Irradiance map
        this->m_cathedralIrradianceMap = std::make_unique<Dx12Texture>(this->m_renderDevice, cubemapDesc);
        uploadCmdList->GenerateIrradianceMap(*this->m_cathedralIrradianceMap, *this->m_cathedralCubeMap);

        // this->m_cathedralSpecularMap = std::make_unique<Dx12Texture>(this->m_renderDevice, cubemapDesc);
        // uploadCmdList->GenerateSpecularMap(*this->m_cathedralSpecularMap, *this->m_cathedralCubeMap);
    }

    {
        // Create a cubemap for the HDR panorama.
        CD3DX12_RESOURCE_DESC specularBrdfLut;
        specularBrdfLut.Width = specularBrdfLut.Height = 256;
        specularBrdfLut.DepthOrArraySize = 1;
        specularBrdfLut.Format = DXGI_FORMAT_R16G16_FLOAT;
        specularBrdfLut.MipLevels = 1;

        this->m_specularBrdfLut= std::make_unique<Dx12Texture>(this->m_renderDevice, specularBrdfLut);
        uploadCmdList->GenerateSpecularBrdfLut(*this->m_specularBrdfLut);
    }

    // Create an inverted (reverse winding order) cube so the insides are not clipped.
    this->m_skyboxMesh = MeshPrefabs::CreateCube(this->m_renderDevice, *uploadCmdList, 1.0f, true);
    this->m_sphereMesh = MeshPrefabs::CreateSphere(this->m_renderDevice, *uploadCmdList, 3.0f);

    uint64_t uploadFence = copyQueue->ExecuteCommandList(uploadCmdList);

    // Create a single light source
    this->m_pointLights.reserve(this->MaxLights);
    this->m_pointLights.emplace_back(PointLight());

    this->CreatePipelineStateObjects();

    copyQueue->WaitForFenceValue(uploadFence);
}

void BRDFLightingIBLApp::Update(double deltaTime)
{
}

void BRDFLightingIBLApp::RenderScene(Dx12Texture& sceneTexture)
{
    auto commandList = this->m_renderDevice->GetQueue()->GetCommandList();

    commandList->ClearRenderTarget(
        this->m_hdrRenderTarget.GetTexture(Color0),
        this->m_clearValue);

    commandList->ClearDepthStencilTexture(
            this->m_hdrRenderTarget.GetTexture(AttachmentPoint::DepthStencil),
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);

    static CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, this->m_window->GetWidth(), this->m_window->GetHeight());
    commandList->SetViewport(viewport);

    static CD3DX12_RECT rect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
    commandList->SetScissorRect(rect);

    // Set Render Target
    commandList->SetRenderTarget(this->m_hdrRenderTarget);

    // Render Skybox
    {
        // -- Set pipeline state ---
        commandList->SetGraphicsRootSignature(*this->m_skyboxSignature);
        commandList->SetPipelineState(this->m_skyboxPso);

        XMMATRIX viewMatrix = XMMatrixTranspose(XMMatrixRotationQuaternion(this->m_camera.GetRotation()));
        XMMATRIX viewProjectionMatrix = viewMatrix * this->m_camera.GetProjectionMatrix();

        commandList->SetGraphicsDynamicConstantBuffer(0, viewProjectionMatrix);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = this->m_cathedralIrradianceMap->GetDx12Resource()->GetDesc().Format;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = (UINT)-1; // Use all mips.

        commandList->SetShaderResourceView(
            SkyboxRootParammeters::Textures,
            0,
            *this->m_cathedralIrradianceMap,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            0,
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &srvDesc);

        // this->m_skyboxMesh->Draw(*commandList);
    }

    // -- Set pipeline state ---
    commandList->SetGraphicsRootSignature(*this->m_rootSignature);
    commandList->SetPipelineState(this->m_lightModelPso);

    // -- Set Shader Parameters ---
    XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
    XMMATRIX rotationMatrix = XMMatrixIdentity();
    XMMATRIX scaleMatrix = XMMatrixScaling(1.0f, 1.0f, 1.0f);
    XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
    XMMATRIX viewMatrix = this->m_camera.GetViewMatrix();
    XMMATRIX viewProjectionMatrix = viewMatrix * this->m_camera.GetProjectionMatrix();

    Matrices matrices;
    ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

    // -- Set Pipeline state parameters ---
    commandList->SetGraphicsDynamicConstantBuffer(PbrRootParameters::MatricesCB, matrices);

    CameraData cameraData = {};
    cameraData.Position = this->m_camera.GetTranslation();
    commandList->SetGraphics32BitConstants(PbrRootParameters::CameraDataCB, cameraData);

    commandList->SetGraphicsDynamicConstantBuffer(PbrRootParameters::MaterialCB, this->m_material);
    LightProperties lightProperties = {};
    lightProperties.numPointLights = this->m_pointLights.size();
    commandList->SetGraphics32BitConstants(PbrRootParameters::LightPropertiesCB, lightProperties);

    commandList->SetGraphicsDynamicStructuredBuffer(PbrRootParameters::PointLightsSB, this->m_pointLights);

    /*
    commandList->SetShaderResourceView(
        PbrRootParameters::Textures,
        0,
        *this->m_cathedralSpecularMap,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        */
    commandList->SetShaderResourceView(
        PbrRootParameters::Textures,
        0,
        *this->m_cathedralIrradianceMap,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList->SetShaderResourceView(
        PbrRootParameters::Textures,
        1,
        *this->m_specularBrdfLut,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // -- Draw Ambient Mesh ---
    this->m_sphereMesh->Draw(*commandList);

    this->m_renderDevice->GetQueue()->ExecuteCommandList(commandList);
    sceneTexture.SetDx12Resource(this->m_hdrRenderTarget.GetTexture(Color0).GetDx12Resource());
}

void BRDFLightingIBLApp::RenderUI()
{
    static bool showWindow = true;
    ImGui::Begin("PBR Options", &showWindow, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::NewLine();
    ImGui::CollapsingHeader("Material Parameters");

    ImGui::ColorEdit3("Albedo", reinterpret_cast<float*>(&this->m_material.Albedo));
    ImGui::SliderFloat("Roughness", &this->m_material.Roughness, 0.0f, 1.0f);

    ImGui::SliderFloat("Matallic", &this->m_material.Metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Ambient Occlusion", &this->m_material.AmbientOcclusion, 0.0f, 1.0f);


    ImGui::NewLine();

    ImGui::CollapsingHeader("Lighting");

    ImGui::ColorEdit3("Colour", reinterpret_cast<float*>(&this->m_pointLights.front().Colour));
    ImGui::DragFloat3("Position", reinterpret_cast<float*>(&this->m_pointLights.front().Position), 0.01f);
    ImGui::Text("Attenuation Constant %f", this->m_pointLights.front().AttenuationConstant);
    ImGui::Text("Attenuation Linear %f", this->m_pointLights.front().AttenuationLinear);
    ImGui::Text("Attenuation Quadratic %f", this->m_pointLights.front().AttenuationQuadratic);
    ImGui::End();
}

void BRDFLightingIBLApp::CreatePipelineStateObjects()
{

    CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(
        0,
        D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    {
        CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);

        CD3DX12_ROOT_PARAMETER1 rootParameters[PbrRootParameters::NumRootParameters];
        rootParameters[PbrRootParameters::MatricesCB].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters[PbrRootParameters::CameraDataCB].InitAsConstants(sizeof(CameraData) / 4, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters[PbrRootParameters::MaterialCB].InitAsConstantBufferView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[PbrRootParameters::LightPropertiesCB].InitAsConstants(sizeof(LightProperties) / 4, 3, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[PbrRootParameters::PointLightsSB].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[PbrRootParameters::Textures].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

        CD3DX12_STATIC_SAMPLER_DESC spBRDF_SamplerDesc{ 1, D3D12_FILTER_MIN_MAG_MIP_LINEAR };
        spBRDF_SamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        spBRDF_SamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        spBRDF_SamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_STATIC_SAMPLER_DESC staticSamplers[2];
        staticSamplers[0] = linearClampSampler;
        staticSamplers[1] = spBRDF_SamplerDesc;

        this->m_rootSignature = this->CreateRootSignature(PbrRootParameters::NumRootParameters, rootParameters, 2, staticSamplers);
        this->m_lightModelPso = this->CreatePipelineStateObject(*this->m_rootSignature, "BRDFLighting", "BRDFLightingIbl");
    }

    {

        CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

        CD3DX12_ROOT_PARAMETER1 rootParameters[SkyboxRootParammeters::NumRootParameters];
        rootParameters[SkyboxRootParammeters::MatricesCB].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters[PbrRootParameters::Textures].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

        this->m_skyboxSignature = this->CreateRootSignature(
            SkyboxRootParammeters::NumRootParameters,
            rootParameters,
            1,
            &linearClampSampler);

        this->m_skyboxPso = this->CreatePipelineStateObject(*this->m_skyboxSignature, "Skybox", "Skybox");
    }
}

std::unique_ptr<RootSignature> BRDFLightingIBLApp::CreateRootSignature(
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

Microsoft::WRL::ComPtr<ID3D12PipelineState> BRDFLightingIBLApp::CreatePipelineStateObject(
    RootSignature const& rootSignature,
    std::string const& vertexShaderName,
    std::string const& pixelShanderName)
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
    bool success = BinaryReader::ReadFile(baseAssetPath + "\\" + vertexShaderName + "VS.cso", vertexByteData);
    CORE_FATAL_ASSERT(success, "Failed to read Vertex Shader");


    std::vector<char> pixelByteData;
    success = BinaryReader::ReadFile(baseAssetPath + "\\" + pixelShanderName + "PS.cso", pixelByteData);
    CORE_FATAL_ASSERT(success, "Failed to read Pixel Shader");

    pipelineStateStream.pRootSignature = rootSignature.GetRootSignature().Get();
    pipelineStateStream.InputLayout = { VertexPositionNormalTexture::InputElements, VertexPositionNormalTexture::InputElementCount };
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexByteData.data(), vertexByteData.size());
    pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelByteData.data(), pixelByteData.size());
    pipelineStateStream.DSVFormat = this->m_hdrRenderTarget.GetDepthStencilFormat();
    pipelineStateStream.RTVFormats = this->m_hdrRenderTarget.GetRenderTargetForamts();


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