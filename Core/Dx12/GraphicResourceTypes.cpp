#include "pch.h"
#include "GraphicResourceTypes.h"

#include "Dx12RenderDevice.h"

#include "ResourceStateTracker.h"

using namespace Core;

Core::Dx12Texture::Dx12Texture(const Dx12Texture& copy)
    : Dx12Resrouce(copy)
{
    this->CreateViews();
}

Core::Dx12Texture::Dx12Texture(Dx12Texture&& copy)
    : Dx12Resrouce(copy)
{
    this->CreateViews();
}

Dx12Texture& Core::Dx12Texture::operator=(const Dx12Texture& other)
{
    Dx12Resrouce::operator=(other);
    this->CreateViews();
    return *this;
}

Dx12Texture& Core::Dx12Texture::operator=(Dx12Texture&& other)
{
    Dx12Resrouce::operator=(other);
    this->CreateViews();
    return *this;
}

void Core::Dx12Texture::CreateViews()
{
    if (this->m_d3dResouce)
    {
        auto device = this->m_renderDevice->GetD3DDevice();

        CD3DX12_RESOURCE_DESC desc(this->m_d3dResouce->GetDesc());

        if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0)
        {
            this->m_renderTargetView = this->m_renderDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            device->CreateRenderTargetView(
                this->m_d3dResouce.Get(),
                nullptr,
                this->m_renderTargetView.GetDescriptorHandle());
        }
        if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0)
        {
            this->m_depthStencilView = this->m_renderDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            device->CreateDepthStencilView(
                this->m_d3dResouce.Get(),
                nullptr,
                this->m_depthStencilView.GetDescriptorHandle());
        }
    }

    std::lock_guard<std::mutex> lock(this->m_shaderResourceViewsMutex);
    std::lock_guard<std::mutex> guard(this->m_unorderedAccessViewsMutex);

    // SRVs and UAVs will be created as needed.
    this->m_shaderResourceViews.clear();
    this->m_unorderedAccessViews.clear();
}

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

D3D12_CPU_DESCRIPTOR_HANDLE Core::Dx12Texture::GetRenderTargetView() const
{
    return this->m_renderTargetView.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE Core::Dx12Texture::GetDepthStencilView() const
{
    return this->m_depthStencilView.GetDescriptorHandle();
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

Core::Dx12Resrouce::Dx12Resrouce(
    std::shared_ptr<Dx12RenderDevice> renderDevice,
    D3D12_RESOURCE_DESC const& resourceDesc,
    const D3D12_CLEAR_VALUE* clearValue)
    : m_d3dResouce(nullptr)
    , m_renderDevice(renderDevice)
{
	auto device = this->m_renderDevice->GetD3DDevice();
	ThrowIfFailed(
		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			clearValue,
			IID_PPV_ARGS(&this->m_d3dResouce)
		));

	ResourceStateTracker::AddGlobalResourceState(this->m_d3dResouce.Get(), D3D12_RESOURCE_STATE_COMMON);

}
