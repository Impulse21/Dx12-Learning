#include "pch.h"
#include "RootSignature.h"

#include "d3dx12.h"

#include "Dx12RenderDevice.h"

using namespace Core;

RootSignature::RootSignature(std::shared_ptr<Dx12RenderDevice> renderDevice, std::wstring debugName)
	: m_renderDevice(renderDevice)
	, m_rootSignatureDesc{}
	, m_numDescriptorsPerTable{ 0 }
	, m_samplerTableBitMask{ 0 }
	, m_descriptorTableBitMask{ 0 }
    , m_debugName(debugName)
{}


RootSignature::RootSignature(
    std::shared_ptr<Dx12RenderDevice> renderDevice,
	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc,
	D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion,
    std::wstring debugName)
	: m_renderDevice(renderDevice)
	, m_rootSignatureDesc{}
	, m_numDescriptorsPerTable{ 0 }
	, m_samplerTableBitMask{ 0 }
	, m_descriptorTableBitMask{ 0 }
    , m_debugName(debugName)
{
	this->SetRootSignatureDesc(
		rootSignatureDesc,
		rootSignatureVersion);
}

RootSignature::~RootSignature()
{
	this->Destory();
}

void Core::RootSignature::Destory()
{
    for (UINT i = 0; i < this->m_rootSignatureDesc.NumParameters; ++i)
    {
        const D3D12_ROOT_PARAMETER1& rootParameter = this->m_rootSignatureDesc.pParameters[i];
        if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            delete[] rootParameter.DescriptorTable.pDescriptorRanges;
        }
    }

    delete[] this->m_rootSignatureDesc.pParameters;
    this->m_rootSignatureDesc.pParameters = nullptr;
    this->m_rootSignatureDesc.NumParameters = 0;

    delete[] this->m_rootSignatureDesc.pStaticSamplers;
    this->m_rootSignatureDesc.pStaticSamplers = nullptr;
    this->m_rootSignatureDesc.NumStaticSamplers = 0;

    this->m_descriptorTableBitMask = 0;
    this->m_samplerTableBitMask = 0;

    memset(this->m_numDescriptorsPerTable, 0, sizeof(this->m_numDescriptorsPerTable));
}

void Core::RootSignature::SetRootSignatureDesc(
	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc,
	D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion)
{
	this->Destory();

	UINT numParameters = rootSignatureDesc.NumParameters;
	D3D12_ROOT_PARAMETER1* parameters = numParameters > 0 ? new D3D12_ROOT_PARAMETER1[numParameters] : nullptr;

	for (UINT i = 0; i < numParameters; i++)
	{
		const D3D12_ROOT_PARAMETER1& rootParameter = rootSignatureDesc.pParameters[i];
		parameters[i] = rootParameter;

        if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            UINT numDescriptorRanges = rootParameter.DescriptorTable.NumDescriptorRanges;
            D3D12_DESCRIPTOR_RANGE1* pDescriptorRanges = numDescriptorRanges > 0 ? new D3D12_DESCRIPTOR_RANGE1[numDescriptorRanges] : nullptr;

            std::memcpy(
                pDescriptorRanges, 
                rootParameter.DescriptorTable.pDescriptorRanges,
                sizeof(D3D12_DESCRIPTOR_RANGE1) * numDescriptorRanges);

            parameters[i].DescriptorTable.NumDescriptorRanges = numDescriptorRanges;
            parameters[i].DescriptorTable.pDescriptorRanges = pDescriptorRanges;

            // Set the bit mask depending on the type of descriptor table.
            if (numDescriptorRanges > 0)
            {
                switch (pDescriptorRanges[0].RangeType)
                {
                case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
                case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
                case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
                    this->m_descriptorTableBitMask |= (1 << i);
                    break;
                case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
                    this->m_samplerTableBitMask |= (1 << i);
                    break;
                }
            }

            // Count the number of descriptors in the descriptor table.
            for (UINT j = 0; j < numDescriptorRanges; ++j)
            {
                this->m_numDescriptorsPerTable[i] += pDescriptorRanges[j].NumDescriptors;
            }
        }

        this->m_rootSignatureDesc.NumParameters = numParameters;
        this->m_rootSignatureDesc.pParameters = parameters;

        UINT numStaticSamplers = rootSignatureDesc.NumStaticSamplers;
        D3D12_STATIC_SAMPLER_DESC* pStaticSamplers = numStaticSamplers > 0 ? new D3D12_STATIC_SAMPLER_DESC[numStaticSamplers] : nullptr;

        if (pStaticSamplers)
        {
            memcpy(pStaticSamplers, rootSignatureDesc.pStaticSamplers,
                sizeof(D3D12_STATIC_SAMPLER_DESC) * numStaticSamplers);
        }

        this->m_rootSignatureDesc.NumStaticSamplers = numStaticSamplers;
        this->m_rootSignatureDesc.pStaticSamplers = pStaticSamplers;

        D3D12_ROOT_SIGNATURE_FLAGS flags = rootSignatureDesc.Flags;
        this->m_rootSignatureDesc.Flags = flags;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionRootSignatureDesc;
        versionRootSignatureDesc.Init_1_1(numParameters, parameters, numStaticSamplers, pStaticSamplers, flags);

        // Serialize the root signature.
        Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
        ThrowIfFailed(
            D3DX12SerializeVersionedRootSignature(
                &versionRootSignatureDesc,
                rootSignatureVersion,
                &rootSignatureBlob,
                &errorBlob));

        // Create the root signature.
        ThrowIfFailed(
            this->m_renderDevice->GetD3DDevice()->CreateRootSignature(
                0,
                rootSignatureBlob->GetBufferPointer(),
                rootSignatureBlob->GetBufferSize(),
                IID_PPV_ARGS(&this->m_rootSignature)));

        if (this->m_debugName != L"")
        {
            ThrowIfFailed(
                this->m_rootSignature->SetName(this->m_debugName.c_str()));
        }
	}
}

uint32_t Core::RootSignature::GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType) const
{
    uint32_t descriptorTableBitMask = 0;
    switch (descriptorHeapType)
    {
    case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
        descriptorTableBitMask = this->m_descriptorTableBitMask;
        break;
    case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
        descriptorTableBitMask = this->m_samplerTableBitMask;
        break;
    }

    return descriptorTableBitMask;
}

uint32_t Core::RootSignature::GetNumDescriptors(uint32_t rootIndex) const
{
    LOG_CORE_ASSERT(rootIndex < 32, "Invalud Root index size");
    return this->m_numDescriptorsPerTable[rootIndex];
}
