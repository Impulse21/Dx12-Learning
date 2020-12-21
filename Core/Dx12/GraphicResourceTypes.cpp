#include "pch.h"
#include "GraphicResourceTypes.h"

#include "Dx12RenderDevice.h"

using namespace Core;

D3D12_CPU_DESCRIPTOR_HANDLE Core::Dx12Texture::GetShaderResourceView(
	const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const
{
    std::size_t hash = 0;
    if (srvDesc)
    {
        hash = std::hash<D3D12_SHADER_RESOURCE_VIEW_DESC>{}(*srvDesc);
    }

    std::lock_guard<std::mutex> lock(this->m_shaderResourceViewsMutex);

    auto iter = this->m_shaderResourceViews.find(hash);
    if (iter == m_shaderResourceViews.end())
    {
        auto srv = this->CreateShaderResourceView(srvDesc);
        iter = this->m_shaderResourceViews.insert({ hash, std::move(srv) }).first;
    }

    return iter->second.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE Core::Dx12Texture::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const
{
    std::size_t hash = 0;
    if (uavDesc)
    {
        hash = std::hash<D3D12_UNORDERED_ACCESS_VIEW_DESC>{}(*uavDesc);
    }

    std::lock_guard<std::mutex> guard(this->m_unorderedAccessViewsMutex);

    auto iter = this->m_unorderedAccessViews.find(hash);
    if (iter == this->m_unorderedAccessViews.end())
    {
        auto uav = CreateUnorderedAccessView(uavDesc);
        iter = this->m_unorderedAccessViews.insert({ hash, std::move(uav) }).first;
    }

    return iter->second.GetDescriptorHandle();
}

DescriptorAllocation Core::Dx12Texture::CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const
{
    auto srv = this->m_renderDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    this->m_renderDevice->GetD3DDevice()
        ->CreateShaderResourceView(this->m_d3dResouce.Get(), srvDesc, srv.GetDescriptorHandle());

    return srv;
}

DescriptorAllocation Core::Dx12Texture::CreateUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const
{

    auto uav = this->m_renderDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    this->m_renderDevice->GetD3DDevice()
        ->CreateUnorderedAccessView(this->m_d3dResouce.Get(), nullptr, uavDesc, uav.GetDescriptorHandle());

    return uav;
}
