#include "pch.h"
#include "CommandList.h"

#include <filesystem>

#include "d3dx12.h"

#include "ResourceStateTracker.h"
#include "UploadBuffer.h"
#include "DynamicDescriptorHeap.h"

#include "DirectXTex/DirectXTex.h"

#include "Dx12RenderDevice.h"

#include "MathHelpers.h"

namespace fs = std::filesystem;

using namespace Core;

std::map<std::wstring, ID3D12Resource* > CommandList::ms_textureCache;
std::mutex CommandList::ms_textureCacheMutex;

Core::CommandList::CommandList(
	D3D12_COMMAND_LIST_TYPE type,
	std::shared_ptr<Dx12RenderDevice> renderDevice,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
	ID3D12CommandAllocator* allocator)
	: m_type(type)
	, m_renderDevice(renderDevice)
	, m_commandList(commandList)
	, m_allocator(allocator)
	, m_resourceStateTracker(std::make_unique<ResourceStateTracker>())
	, m_uploadBuffer(std::make_unique<UploadBuffer>(renderDevice->GetD3DDevice()))
{
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		this->m_dynamicDescriptorHeap[i] =
			std::make_unique<DynamicDescriptorHeap>(
				static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i),
				this->m_renderDevice->GetD3DDevice());

		this->m_descriptorHeaps[i] = nullptr;
	}
}

void Core::CommandList::Reset()
{
	ThrowIfFailed(this->m_allocator->Reset());

	ThrowIfFailed(
		this->m_commandList->Reset(this->m_allocator, nullptr));

	this->m_resourceStateTracker->Reset();
	this->m_uploadBuffer->Reset();
	this->m_trackedObjects.clear();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		this->m_dynamicDescriptorHeap[i]->Reset();
		this->m_descriptorHeaps[i] = nullptr;
	}

}

bool Core::CommandList::Close(CommandList& pendingCommandList)
{
	// Flush any remaining barriers.
	this->FlushResourceBarriers();
	this->m_commandList->Close();

	// Flush pending resource barriers.
	uint32_t numPendingBarriers = 
		this->m_resourceStateTracker->FlushPendingResourceBarriers(pendingCommandList);

	// Commit the final resource state to the global state.
	this->m_resourceStateTracker->CommitFinalResourceStates();

	return numPendingBarriers > 0;
}

void Core::CommandList::Close()
{
	this->FlushResourceBarriers();
	this->m_commandList->Close();
}

void Core::CommandList::FlushResourceBarriers()
{
	this->m_resourceStateTracker->FlushResourceBarriers(*this);
}

void Core::CommandList::TransitionBarrier(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES stateAfter, uint32_t subresource, bool flushBarriers)
{
	if (resource)
	{
		// The "before" state is not important. It will be resolved by the resource state tracker.
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			resource.Get(),
			D3D12_RESOURCE_STATE_COMMON,
			stateAfter,
			subresource);
		this->m_resourceStateTracker->ResourceBarrier(barrier);
	}

	if (flushBarriers)
	{
		this->FlushResourceBarriers();
	}
}

void Core::CommandList::LoadTextureFromFile(Dx12Texture& texture, std::wstring const& filename)
{
	fs::path filePath(filename);
	if (!fs::exists(filePath))
	{
		LOG_CORE_ERROR("Unable to locate texture filepath");
		throw std::exception("File not found.");
	}

	// Check cache
	std::lock_guard<std::mutex> lock(ms_textureCacheMutex);
	auto iter = ms_textureCache.find(filename);
	if (iter != ms_textureCache.end())
	{
		texture.SetDx12Resource(iter->second);
		texture.CreateViews();
		return;
	}

	// Create Texture
	DirectX::TexMetadata metadata;
	DirectX::ScratchImage scratchImage;

	if (filePath.extension() == ".dds")
	{
		// Load DDS
		ThrowIfFailed(
			DirectX::LoadFromDDSFile(
				filename.c_str(),
				DirectX::DDS_FLAGS_FORCE_RGB,
				&metadata,
				scratchImage));
	}
	else if (filePath.extension() == ".hdr")
	{
		// Load DDS
		ThrowIfFailed(
			DirectX::LoadFromHDRFile(
				filename.c_str(),
				&metadata,
				scratchImage));
	}
	else if (filePath.extension() == ".tga")
	{
		// Load DDS
		ThrowIfFailed(
			DirectX::LoadFromTGAFile(
				filename.c_str(),
				&metadata,
				scratchImage));
	}
	else
	{
		ThrowIfFailed(
			DirectX::LoadFromWICFile(
				filename.c_str(),
				DirectX::WIC_FLAGS_FORCE_RGB,
				&metadata,
				scratchImage));
	}

	D3D12_RESOURCE_DESC textureDesc = {};
	switch (metadata.dimension)
	{
	case DirectX::TEX_DIMENSION_TEXTURE1D:
		textureDesc = CD3DX12_RESOURCE_DESC::Tex1D(
			metadata.format,
			static_cast<UINT64>(metadata.width),
			static_cast<UINT16>(metadata.arraySize));
		break;
	case DirectX::TEX_DIMENSION_TEXTURE2D:
		textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			metadata.format,
			static_cast<UINT64>(metadata.width),
			static_cast<UINT>(metadata.height),
			static_cast<UINT16>(metadata.arraySize));
		break;
	case DirectX::TEX_DIMENSION_TEXTURE3D:
		textureDesc = CD3DX12_RESOURCE_DESC::Tex3D(
			metadata.format,
			static_cast<UINT64>(metadata.width),
			static_cast<UINT>(metadata.height),
			static_cast<UINT16>(metadata.arraySize));
		break;

	default:
		throw std::exception("Invalid texture dimension.");
		break;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
	ThrowIfFailed(
		this->m_renderDevice->GetD3DDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&textureResource)));

	SetD3DDebugName(textureResource, L"Texture");

	texture.SetDx12Resource(textureResource);
	texture.CreateViews();
	ResourceStateTracker::AddGlobalResourceState(
		textureResource.Get(),
		D3D12_RESOURCE_STATE_COMMON);

	std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
	const DirectX::Image* pImages = scratchImage.GetImages();
	for (int i = 0; i < scratchImage.GetImageCount(); ++i)
	{
		auto& subresource = subresources[i];
		subresource.RowPitch = pImages[i].rowPitch;
		subresource.SlicePitch = pImages[i].slicePitch;
		subresource.pData = pImages[i].pixels;
	}

	this->CopyTextureSubresource(
		texture,
		0,
		static_cast<uint32_t>(subresources.size()),
		subresources.data());

	if (subresources.size() < textureResource->GetDesc().MipLevels)
	{
		// this->GenerateMips(texture);
	}

	ms_textureCache[filename] = textureResource.Get();
}

void Core::CommandList::GenerateMips(Dx12Texture& texture)
{
	return;
	if (this->m_type == D3D12_COMMAND_LIST_TYPE_COPY)
	{
		if (this->m_computeCommandList)
		{
			this->m_computeCommandList = 
				this->m_renderDevice->GetQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE)->GetCommandList();

			this->m_computeCommandList->GenerateMips(texture);
			return;
		}
	}

	auto resource = texture.GetDx12Resource();

	// If the texture doesn't have a valid resource, do nothing.
	if (!resource)
	{
		return;
	}

	auto resourceDesc = resource->GetDesc();
	// If the texture only has a single mip level (level 0)
	// do nothing.
	if (resourceDesc.MipLevels == 1)
	{
		return;
	}

	// Currently, only non-multi-sampled 2D textures are supported.
	if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		resourceDesc.DepthOrArraySize != 1 ||
		resourceDesc.SampleDesc.Count > 1)
	{
		throw std::exception("GenerateMips is only supported for non-multi-sampled 2D Textures.");
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> uavResource = resource;

	// Create an alias of the original resource.
	// This is done to perform a GPU copy of resources with different formats.
	// BGR -> RGB texture copies will fail GPU validation unless performed 
	// through an alias of the BRG resource in a placed heap.
	Microsoft::WRL::ComPtr<ID3D12Resource> aliasResource;

	// TODO:
}

void Core::CommandList::PanoToCubemap(Dx12Texture& cubemap, Dx12Texture const& pano)
{
	if (this->m_type == D3D12_COMMAND_LIST_TYPE_COPY)
	{
		if (this->m_computeCommandList)
		{
			this->m_computeCommandList =
				this->m_renderDevice->GetQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE)->GetCommandList();

			this->m_computeCommandList->PanoToCubemap(cubemap, pano);
			return;
		}
	}

	if (!this->m_panoToCubeMapPso)
	{
		this->m_panoToCubeMapPso = std::make_unique<PanoToCubemapPso>(this->m_renderDevice);
	}
	auto cubemapResource = cubemap.GetDx12Resource();
	if (!cubemapResource)
	{
		return;
	}


	CD3DX12_RESOURCE_DESC cubemapDesc(cubemapResource->GetDesc());

	auto stagingResource = cubemapResource;
	Dx12Texture stagingTexture(this->m_renderDevice, stagingResource);

	// If the passed-in resource does not allow for UAV access
	// then create a staging resource that is used to generate
	// the cubemap.
	if ((cubemapDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0)
	{
		auto stagingDesc = cubemapDesc;
		stagingDesc.Format = Dx12Texture::GetUAVCompatableFormat(cubemapDesc.Format);
		stagingDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		ThrowIfFailed(
			this->m_renderDevice->GetD3DDevice()->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&stagingDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&stagingResource)));

		ResourceStateTracker::AddGlobalResourceState(stagingResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);

		SetD3DDebugName(stagingResource, L"Pano to Cubemap Staging Texture");

		stagingTexture.SetDx12Resource(stagingResource);
		stagingTexture.CreateViews();

		this->CopyResource(stagingTexture, cubemap);
	}

	this->TransitionBarrier(stagingTexture.GetDx12Resource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	this->m_commandList->SetPipelineState(this->m_panoToCubeMapPso->GetPipelineState().Get());
	this->SetComputeRootSignature(this->m_panoToCubeMapPso->GetRootSignature());

	PanoToCubemapCB panoToCubemapCB;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = Dx12Texture::GetUAVCompatableFormat(cubemapDesc.Format);
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Texture2DArray.FirstArraySlice = 0;
	uavDesc.Texture2DArray.ArraySize = 6;

	for (uint32_t mipSlice = 0; mipSlice < cubemapDesc.MipLevels; )
	{
		// Maximum number of mips to generate per pass is 5.
		uint32_t numMips = std::min<uint32_t>(5, cubemapDesc.MipLevels - mipSlice);

		panoToCubemapCB.FirstMip = mipSlice;
		panoToCubemapCB.CubemapSize = std::max<uint32_t>(static_cast<uint32_t>(cubemapDesc.Width), cubemapDesc.Height) >> mipSlice;
		panoToCubemapCB.NumMips = numMips;

		this->SetCompute32BitConstants(PanoToCubemapRS::PanoToCubemapCB, panoToCubemapCB);

		this->SetShaderResourceView(PanoToCubemapRS::SrcTexture, 0, pano, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		for (uint32_t mip = 0; mip < numMips; ++mip)
		{
			uavDesc.Texture2DArray.MipSlice = mipSlice + mip;
			this->SetUnorderedAccessView(
				PanoToCubemapRS::DstMips, mip, stagingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, 0, &uavDesc);
		}

		if (numMips < 5)
		{
			// Pad unused mips. This keeps DX12 runtime happy.
			this->m_dynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
				PanoToCubemapRS::DstMips,
				panoToCubemapCB.NumMips,
				5 - numMips,
				this->m_panoToCubeMapPso->GetDefaultUAV());
		}

		this->Dispatch(
			Math::DivideByMultiple(panoToCubemapCB.CubemapSize, 16),
			Math::DivideByMultiple(panoToCubemapCB.CubemapSize, 16),
			6);

		mipSlice += numMips;
	}

	if (stagingResource != cubemapResource)
	{
		CopyResource(cubemap, stagingTexture);
	}
}

void Core::CommandList::CopyTextureSubresource(
	Dx12Texture& texture,
	uint32_t firstSubResource,
	uint32_t numSubresources,
	D3D12_SUBRESOURCE_DATA* subresourceData)
{
	auto destinationResource = texture.GetDx12Resource();

	if (!destinationResource)
	{
		return;
	}

	this->TransitionBarrier(destinationResource, D3D12_RESOURCE_STATE_COPY_DEST);
	this->FlushResourceBarriers();

	uint64_t requiredSize = 
		GetRequiredIntermediateSize(
			destinationResource.Get(),
			firstSubResource,
			numSubresources);

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
	ThrowIfFailed(
		this->m_renderDevice->GetD3DDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(requiredSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&intermediateResource)));

	UpdateSubresources(
		this->m_commandList.Get(),
		destinationResource.Get(),
		intermediateResource.Get(),
		0,
		firstSubResource,
		numSubresources,
		subresourceData);

	this->TrackResource(intermediateResource);
	this->TrackResource(destinationResource);
}

void Core::CommandList::CopyBuffer(
	Microsoft::WRL::ComPtr<ID3D12Resource>& buffer,
	size_t numOfElements,
	size_t elementStride,
	const void* data,
	D3D12_RESOURCE_FLAGS flags)
{
	size_t bufferSize = numOfElements * elementStride;
	ThrowIfFailed(
		this->m_renderDevice->GetD3DDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&buffer)));

	ResourceStateTracker::AddGlobalResourceState(buffer.Get(), D3D12_RESOURCE_STATE_COMMON);

	if (data)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> uploadResource;
		ThrowIfFailed(
			this->m_renderDevice->GetD3DDevice()->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&uploadResource)));

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = data;
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		this->m_resourceStateTracker->TransitionResource(buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
		this->FlushResourceBarriers();

		// Track resource
		UpdateSubresources<1>(
			this->m_commandList.Get(),
			buffer.Get(), uploadResource.Get(),
			0, 0, 1, &subresourceData);

		// Track Upload Resource
		this->TrackResource(uploadResource);
	}

	this->TrackResource(buffer);
}

void Core::CommandList::CopyResource(Microsoft::WRL::ComPtr<ID3D12Resource> dstRes, Microsoft::WRL::ComPtr<ID3D12Resource> srcRes)
{
	this->TransitionBarrier(dstRes, D3D12_RESOURCE_STATE_COPY_DEST);
	this->TransitionBarrier(srcRes, D3D12_RESOURCE_STATE_COPY_SOURCE);

	this->FlushResourceBarriers();

	this->m_commandList->CopyResource(dstRes.Get(), srcRes.Get());

	this->TrackResource(dstRes);
	this->TrackResource(srcRes);
}

void Core::CommandList::ResolveSubresource(
	Dx12Resrouce const& dstRes,
	Dx12Resrouce const& srcRes,
	uint32_t dstSubresource,
	uint32_t srcSubresource)
{
	this->TransitionBarrier(dstRes.GetDx12Resource(), D3D12_RESOURCE_STATE_RESOLVE_DEST, dstSubresource);
	this->TransitionBarrier(srcRes.GetDx12Resource(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, srcSubresource);

	this->FlushResourceBarriers();

	this->m_commandList->ResolveSubresource(
		dstRes.GetDx12Resource().Get(),
		dstSubresource,
		srcRes.GetDx12Resource().Get(),
		srcSubresource,
		dstRes.GetDx12Resource()->GetDesc().Format);

	TrackResource(srcRes.GetDx12Resource());
	TrackResource(dstRes.GetDx12Resource());
}

void Core::CommandList::ClearDepthStencilTexture(Dx12Texture const& texture, D3D12_CLEAR_FLAGS clearFlags, float depth, uint8_t stencil)
{
	this->TransitionBarrier(texture.GetDx12Resource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
	this->m_commandList->ClearDepthStencilView(
		texture.GetDepthStencilView(),
		clearFlags,
		depth,
		stencil,
		0,
		nullptr);

	this->TrackResource(texture.GetDx12Resource());
}

void Core::CommandList::ClearRenderTarget(Dx12Texture const& texture, std::array<FLOAT, 4> clearColour)
{
	this->ClearRenderTarget(texture.GetDx12Resource(), texture.GetRenderTargetView(), clearColour);
}

void Core::CommandList::ClearRenderTarget(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_CPU_DESCRIPTOR_HANDLE rtv, std::array<FLOAT, 4> clearColour)
{
	this->TransitionBarrier(resource, D3D12_RESOURCE_STATE_RENDER_TARGET);
	this->m_commandList->ClearRenderTargetView(rtv, clearColour.data(), 0, nullptr);

}

void Core::CommandList::SetViewport(CD3DX12_VIEWPORT const& viewport)
{
	this->SetViewports( { viewport } );
}

void Core::CommandList::SetViewports(std::vector<CD3DX12_VIEWPORT> const& viewports)
{
	LOG_CORE_ASSERT(
		viewports.size() < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE,
		"Invalid number of render targets");

	this->m_commandList->RSSetViewports(
		static_cast<UINT>(viewports.size()),
		viewports.data());
}

void Core::CommandList::SetScissorRect(CD3DX12_RECT const& rect)
{
	this->SetScissorRects({ rect });
}

void Core::CommandList::SetScissorRects(std::vector<CD3DX12_RECT> const& rects)
{
	LOG_CORE_ASSERT(
		rects.size() < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE,
		"Invalid number of scissor rects");

	this->m_commandList->RSSetScissorRects(
		static_cast<UINT>(rects.size()),
		rects.data());
}

void Core::CommandList::SetRenderTarget(RenderTarget const& renderTarget)
{
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> renderTargetDescriptors;
	renderTargetDescriptors.reserve(AttachmentPoint::NumAttachmentPoints);
	const auto& textures = renderTarget.GetTextures();

	// Bind color targets (max of 8 render targets can be bound to the rendering pipeline.
	for (int i = 0; i < 8; ++i)
	{
		auto& texture = textures[i];

		if (texture.GetDx12Resource())
		{
			TransitionBarrier(texture.GetDx12Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			renderTargetDescriptors.push_back(texture.GetRenderTargetView());

			TrackResource(texture.GetDx12Resource());
		}
	}

	const auto& depthTexture = renderTarget.GetTexture(AttachmentPoint::DepthStencil);
	CD3DX12_CPU_DESCRIPTOR_HANDLE depthStencilDescriptor(D3D12_DEFAULT);
	if (depthTexture.GetDx12Resource())
	{
		TransitionBarrier(depthTexture.GetDx12Resource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		depthStencilDescriptor = depthTexture.GetDepthStencilView();

		TrackResource(depthTexture.GetDx12Resource());
	}

	D3D12_CPU_DESCRIPTOR_HANDLE* pDSV = depthStencilDescriptor.ptr != 0 ? &depthStencilDescriptor : nullptr;

	this->m_commandList->OMSetRenderTargets(
		static_cast<UINT>(renderTargetDescriptors.size()),
		renderTargetDescriptors.data(),
		FALSE,
		pDSV);
}

void Core::CommandList::SetRenderTarget(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_CPU_DESCRIPTOR_HANDLE& rtv)
{
	TransitionBarrier(resource, D3D12_RESOURCE_STATE_RENDER_TARGET);

	this->m_commandList->OMSetRenderTargets(1, &rtv, false, nullptr);
	TrackResource(resource);
}

void Core::CommandList::SetGraphics32BitConstants(uint32_t rootParameterIndex, uint32_t numConstants, const void* constants)
{
	this->m_commandList->SetGraphicsRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

void Core::CommandList::SetGraphicsDynamicStructuredBuffer(uint32_t slot, size_t numElements, size_t elementSize, const void* bufferData)
{
	size_t bufferSize = numElements * elementSize;
	auto heapAllocation = this->m_uploadBuffer->Allocate(bufferSize, elementSize);

	memcpy(heapAllocation.Cpu, bufferData, bufferSize);

	this->m_commandList->SetGraphicsRootShaderResourceView(slot, heapAllocation.Gpu);
}

void Core::CommandList::SetGraphicsDynamicConstantBuffer(uint32_t rootParameterIndex, size_t sizeInBytes, const void* bufferData)
{
	// Constant buffers must be 256-byte aligned.
	auto heapAllococation = this->m_uploadBuffer->Allocate(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	memcpy(heapAllococation.Cpu, bufferData, sizeInBytes);

	this->m_commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, heapAllococation.Gpu);
}

void Core::CommandList::SetCompute32BitConstants(uint32_t rootParameterIndex, uint32_t numConstants, const void* constants)
{
	this->m_commandList->SetComputeRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

void Core::CommandList::SetGraphicsRootShaderResourceView(
	uint32_t rootParameterIndex,
	Microsoft::WRL::ComPtr<ID3D12Resource> resource)
{
	this->m_commandList->SetGraphicsRootShaderResourceView(0, resource->GetGPUVirtualAddress());
	this->TrackResource(resource);
}

void Core::CommandList::SetUnorderedAccessView(
	uint32_t rootParameterIndex,
	uint32_t descrptorOffset,
	Dx12Resrouce const& resource,
	D3D12_RESOURCE_STATES stateAfter,
	UINT firstSubresource,
	UINT numSubresources,
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav)
{
	if (numSubresources < D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
	{
		for (uint32_t i = 0; i < numSubresources; ++i)
		{
			this->TransitionBarrier(resource.GetDx12Resource(), stateAfter, firstSubresource + i);
		}
	}
	else
	{
		this->TransitionBarrier(resource.GetDx12Resource(), stateAfter);
	}

	this->m_dynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
		rootParameterIndex,
		descrptorOffset,
		1,
		resource.GetUnorderedAccessView(uav));

	this->TrackResource(resource.GetDx12Resource());
}

void Core::CommandList::SetGraphicsRootSignature(RootSignature const& rootSignature)
{
	auto d3d12RootSignature = rootSignature.GetRootSignature().Get();
	if (d3d12RootSignature == this->m_rootSignature)
	{
		//return;
	}

	this->m_rootSignature = d3d12RootSignature;

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		this->m_dynamicDescriptorHeap[i]->ParseRootSignature(rootSignature);
	}

	this->m_commandList->SetGraphicsRootSignature(this->m_rootSignature);
	this->TrackResource(this->m_rootSignature);
}

void Core::CommandList::SetComputeRootSignature(RootSignature const& rootSignature)
{
	auto d3d12RootSignature = rootSignature.GetRootSignature().Get();
	if (d3d12RootSignature == this->m_rootSignature)
	{
		//return;
	}

	this->m_rootSignature = d3d12RootSignature;

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		this->m_dynamicDescriptorHeap[i]->ParseRootSignature(rootSignature);
	}

	this->m_commandList->SetComputeRootSignature(this->m_rootSignature);
	this->TrackResource(this->m_rootSignature);
}

void Core::CommandList::SetGraphicsRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature)
{
	this->m_commandList->SetGraphicsRootSignature(rootSignature.Get());
	this->TrackResource(rootSignature);
}

void Core::CommandList::SetShaderResourceView(uint32_t rootParameterIndex, uint32_t descritporOffset, Dx12Resrouce const& resource, D3D12_RESOURCE_STATES stateAfter, UINT firstSubresource, UINT numSubresources, const D3D12_SHADER_RESOURCE_VIEW_DESC* srv)
{
	if (numSubresources < D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
	{
		for (uint32_t i = 0; i < numSubresources; i++)
		{
			this->TransitionBarrier(resource.GetDx12Resource(), stateAfter, firstSubresource + i);
		}
	}
	else
	{
		this->TransitionBarrier(resource.GetDx12Resource(), stateAfter);
	}

	this->m_dynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]
		->StageDescriptors(
			rootParameterIndex,
			descritporOffset,
			1,
			resource.GetShaderResourceView(srv));

	this->TrackResource(resource.GetDx12Resource());
}

void Core::CommandList::SetPipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState)
{
	this->m_commandList->SetPipelineState(pipelineState.Get());
	this->TrackResource(pipelineState);
}

void Core::CommandList::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology)
{
	this->m_commandList->IASetPrimitiveTopology(topology);
}

void Core::CommandList::SetVertexBuffer(Dx12Buffer& vertexBuffer)
{
	LOG_CORE_ASSERT(vertexBuffer.GetBindings() & BIND_VERTEX_BUFFER, "Unable to bind non vertex buffer");
	D3D12_VERTEX_BUFFER_VIEW view = {};
	view.BufferLocation = vertexBuffer.GetDx12Resource()->GetGPUVirtualAddress();
	view.SizeInBytes = vertexBuffer.GetSizeInBytes();
	view.StrideInBytes = vertexBuffer.GetElementByteStride();

	this->SetVertexBuffer(
		vertexBuffer.GetDx12Resource(),
		view);
}

void Core::CommandList::SetVertexBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_VERTEX_BUFFER_VIEW& vertexView)
{
	this->TransitionBarrier(resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	this->m_commandList->IASetVertexBuffers(0, 1, &vertexView);

	this->TrackResource(resource);
}

void Core::CommandList::SetDynamicVertexBuffer(uint32_t slot, size_t numVertices, size_t vertexSize, const void* vertexBufferData)
{
	size_t bufferSize = numVertices * vertexSize;
	auto heapAllocation = this->m_uploadBuffer->Allocate(bufferSize, vertexSize);
	memcpy(heapAllocation.Cpu, vertexBufferData, bufferSize);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
	vertexBufferView.BufferLocation = heapAllocation.Gpu;
	vertexBufferView.SizeInBytes = static_cast<UINT>(bufferSize);
	vertexBufferView.StrideInBytes = static_cast<UINT>(vertexSize);

	this->m_commandList->IASetVertexBuffers(slot, 1, &vertexBufferView);
}

void Core::CommandList::SetIndexBuffer(Dx12Buffer& indexBuffer)
{
	LOG_CORE_ASSERT(indexBuffer.GetBindings() & BIND_INDEX_BUFFER, "Unable to bind non index buffer");
	LOG_CORE_ASSERT(indexBuffer.GetElementByteStride() == sizeof(uint32_t) || indexBuffer.GetElementByteStride() == sizeof(uint16_t), "Invalid Index stride");

	D3D12_INDEX_BUFFER_VIEW view = {};
	view.BufferLocation = indexBuffer.GetDx12Resource()->GetGPUVirtualAddress();
	view.SizeInBytes = indexBuffer.GetSizeInBytes();
	view.Format = indexBuffer.GetElementByteStride() == sizeof(uint32_t) ? DXGI_FORMAT_R32_UINT: DXGI_FORMAT_R16_UINT;

	this->SetIndexBuffer(
		indexBuffer.GetDx12Resource(),
		view);
}

void Core::CommandList::SetIndexBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_INDEX_BUFFER_VIEW& indexView)
{
	this->TransitionBarrier(resource, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	this->m_commandList->IASetIndexBuffer(&indexView);

	this->TrackResource(resource);
}

void Core::CommandList::SetDynamicIndexBuffer(size_t numIndicies, DXGI_FORMAT indexFormat, const void* indexBufferData)
{
	size_t indexSizeInBytes = indexFormat == DXGI_FORMAT_R16_UINT ? 2 : 4;
	size_t bufferSize = numIndicies * indexSizeInBytes;

	auto heapAllocation = this->m_uploadBuffer->Allocate(bufferSize, indexSizeInBytes);
	memcpy(heapAllocation.Cpu, indexBufferData, bufferSize);

	D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
	indexBufferView.BufferLocation = heapAllocation.Gpu;
	indexBufferView.SizeInBytes = static_cast<UINT>(bufferSize);
	indexBufferView.Format = indexFormat;

	this->m_commandList->IASetIndexBuffer(&indexBufferView);
}

void Core::CommandList::Draw(
	uint32_t vertexCount,
	uint32_t instanceCount,
	uint32_t startVertex,
	uint32_t startInstance)
{
	this->FlushResourceBarriers();
	
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		this->m_dynamicDescriptorHeap[i]->CommitStagedDescriptorsForDraw(*this);
	}

	this->m_commandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
}

void Core::CommandList::DrawIndexed(
	uint32_t indexCount,
	uint32_t instanceCount,
	uint32_t startIndex,
	int32_t baseVertex,
	uint32_t startInstance)
{
	this->FlushResourceBarriers();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		this->m_dynamicDescriptorHeap[i]->CommitStagedDescriptorsForDraw(*this);
	}

	this->m_commandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

void Core::CommandList::Dispatch(uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ)
{
	this->FlushResourceBarriers();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		this->m_dynamicDescriptorHeap[i]->CommitStagedDescriptorsForDispatch(*this);
	}

	this->m_commandList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
}

void Core::CommandList::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* heap)
{
	if (this->m_descriptorHeaps[heapType] == heap)
	{
		return;
	}

	this->m_descriptorHeaps[heapType] = heap;
	this->BindDescriptorHeaps();
}

void Core::CommandList::TrackResource(Microsoft::WRL::ComPtr<ID3D12Object> object)
{
	this->m_trackedObjects.push_back(object);
}

void Core::CommandList::BindDescriptorHeaps()
{
	uint32_t numDescriptorHeaps = 0;
	ID3D12DescriptorHeap* descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

	for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
	{
		ID3D12DescriptorHeap* descriptorHeap = this->m_descriptorHeaps[i];
		if (descriptorHeap)
		{
			descriptorHeaps[numDescriptorHeaps++] = descriptorHeap;
		}
	}

	this->m_commandList->SetDescriptorHeaps(numDescriptorHeaps, descriptorHeaps);
}


