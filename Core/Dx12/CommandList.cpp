#include "pch.h"
#include "CommandList.h"

#include "d3dx12.h"

#include "ResourceStateTracker.h"

Core::CommandList::CommandList(
	Microsoft::WRL::ComPtr<ID3D12Device2> device,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
	ID3D12CommandAllocator* allocator)
	: m_d3d12Device(device)
	, m_commandList(commandList)
	, m_allocator(allocator)
	, m_resourceStateTracker(std::make_unique<ResourceStateTracker>())
{
}

void Core::CommandList::Reset()
{
	ThrowIfFailed(this->m_allocator->Reset());

	ThrowIfFailed(
		this->m_commandList->Reset(this->m_allocator, nullptr));

	this->m_resourceStateTracker->Reset();
	this->m_trackedObjects.clear();
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

void Core::CommandList::CopyBuffer(
	Microsoft::WRL::ComPtr<ID3D12Resource>& buffer,
	size_t numOfElements,
	size_t elementStride,
	const void* data,
	D3D12_RESOURCE_FLAGS flags)
{
	size_t bufferSize = numOfElements * elementStride;

	ThrowIfFailed(
		this->m_d3d12Device->CreateCommittedResource(
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
			this->m_d3d12Device->CreateCommittedResource(
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
		UpdateSubresources(
			this->m_commandList.Get(),
			buffer.Get(), uploadResource.Get(),
			0, 0, 1, &subresourceData);

		// Track Upload Resource
		this->TrackResource(uploadResource);
	}

	this->TrackResource(buffer);
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

void Core::CommandList::SetRenderTarget(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_CPU_DESCRIPTOR_HANDLE& rtv)
{
	TransitionBarrier(resource, D3D12_RESOURCE_STATE_RENDER_TARGET);

	this->m_commandList->OMSetRenderTargets(1, &rtv, false, nullptr);
	TrackResource(resource);
}

void Core::CommandList::SetGraphicsRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature)
{
	this->m_commandList->SetGraphicsRootSignature(rootSignature.Get());
	this->TrackResource(rootSignature);
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

void Core::CommandList::SetVertexBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_VERTEX_BUFFER_VIEW& vertexView)
{
	this->TransitionBarrier(resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	this->m_commandList->IASetVertexBuffers(0, 1, &vertexView);

	this->TrackResource(resource);
}

void Core::CommandList::SetIndexBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_INDEX_BUFFER_VIEW& indexView)
{
	this->TransitionBarrier(resource, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	this->m_commandList->IASetIndexBuffer(&indexView);

	this->TrackResource(resource);
}

void Core::CommandList::Draw(
	uint32_t vertexCount,
	uint32_t instanceCount,
	uint32_t startVertex,
	uint32_t startInstance)
{
	this->m_commandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
}

void Core::CommandList::DrawIndexed(
	uint32_t indexCount,
	uint32_t instanceCount,
	uint32_t startIndex,
	int32_t baseVertex,
	uint32_t startInstance)
{
	this->m_commandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

void Core::CommandList::TrackResource(Microsoft::WRL::ComPtr<ID3D12Object> object)
{
	this->m_trackedObjects.push_back(object);
}


