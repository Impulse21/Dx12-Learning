#pragma once

#include "RootSignature.h"
#include "DescriptorAllocation.h"

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>


namespace Core
{
    // Struct used in the PanoToCubemap_CS compute shader.
    struct alignas(16) PanoToCubemapCB
    {
        // Size of the cubemap face in pixels at the current mipmap level.
        uint32_t CubemapSize;

        // The first mip level to generate.
        uint32_t FirstMip;

        // The number of mips to generate.
        uint32_t NumMips;
    };

    namespace PanoToCubemapRS
    {
        enum
        {
            PanoToCubemapCB,
            SrcTexture,
            DstMips,

            NumRootParameters
        };
    }

    class Dx12RenderDevice;
	class PanoToCubemapPso
	{
    public:
        PanoToCubemapPso(std::shared_ptr<Dx12RenderDevice> renderDevice);

        const RootSignature& GetRootSignature() const { return this->m_rootSignature; }

        Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState() const
        {
            return this->m_pipelineState;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE GetDefaultUAV() const { return this->m_defaultUAV.GetDescriptorHandle();  }

    private:
        std::shared_ptr<Dx12RenderDevice> m_renderDevice;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
        RootSignature m_rootSignature;

        // Default (no resource) UAV's to pad the unused UAV descriptors.
        // If generating less than 4 mip map levels, the unused mip maps
        // need to be padded with default UAVs (to keep the DX12 runtime happy).
        DescriptorAllocation m_defaultUAV;
	};
}

