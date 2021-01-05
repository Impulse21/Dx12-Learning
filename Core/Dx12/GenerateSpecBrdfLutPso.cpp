#include "pch.h"
#include "GenerateSpecBrdfLutPso.h"

#include "Shaders/Generated/SpecularBrdfLutCS_generated.h"

#include "d3dx12.h"

#include "Dx12RenderDevice.h"

using namespace Core;

GenerateSpecBrdfLutPso::GenerateSpecBrdfLutPso(std::shared_ptr<Dx12RenderDevice> renderDevice)
	: m_renderDevice(renderDevice)
	, m_rootSignature(renderDevice)
{
	auto device = this->m_renderDevice->GetD3DDevice();
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    CD3DX12_DESCRIPTOR_RANGE1 lut(
        D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
        1,
        0,
        0,
        D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

    CD3DX12_ROOT_PARAMETER1 rootParameters[GenerateSpecBrdfLutRS::NumRootParameters];
    rootParameters[GenerateSpecBrdfLutRS::LUT].InitAsDescriptorTable(1, &lut);

    CD3DX12_STATIC_SAMPLER_DESC linearRepeatSampler(0,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP
    );

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(GenerateSpecBrdfLutRS::NumRootParameters,
        rootParameters, 1, &linearRepeatSampler);

    this->m_rootSignature.SetRootSignatureDesc(rootSignatureDesc.Desc_1_1, featureData.HighestVersion);

    // Create the PSO for GenerateMips shader.
    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS CS;
    } pipelineStateStream;

    pipelineStateStream.pRootSignature = this->m_rootSignature.GetRootSignature().Get();
    pipelineStateStream.CS = { gSpecularBrdfLutCS, sizeof(gSpecularBrdfLutCS) };

    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
        sizeof(PipelineStateStream), &pipelineStateStream
    };

    ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&this->m_pipelineState)));

    // Alocate Descriptors
    this->m_defaultUAV = this->m_renderDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.Texture2DArray.FirstArraySlice = 0;
    uavDesc.Texture2DArray.MipSlice = 0;
    uavDesc.Texture2DArray.PlaneSlice = 0;

    device->CreateUnorderedAccessView(
        nullptr, nullptr, &uavDesc,
        this->m_defaultUAV.GetDescriptorHandle());
}