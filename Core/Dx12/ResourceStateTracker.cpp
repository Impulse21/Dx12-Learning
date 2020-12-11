#include "pch.h"

#include "ResourceStateTracker.h"

using namespace Core;

// Static definitions.
std::mutex ResourceStateTracker::ms_globalMutex;
bool ResourceStateTracker::ms_isLocked = false;
ResourceStateTracker::ResourceStateMap ResourceStateTracker::ms_globalResourceState;

void Core::ResourceStateTracker::Lock()
{
	ms_globalMutex.lock();
	ms_isLocked = true;
}

void Core::ResourceStateTracker::Unlock()
{
	ms_isLocked = false;
	ms_globalMutex.unlock();
}

void Core::ResourceStateTracker::AddGlobalResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state)
{
	if (resource != nullptr)
	{
		std::lock_guard<std::mutex> lock(ms_globalMutex);
		ms_globalResourceState[resource].SetSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, state);
	}
}

void Core::ResourceStateTracker::RemoveGlobalResourceState(ID3D12Resource* resource)
{
	if (resource != nullptr)
	{
		std::lock_guard<std::mutex> lock(ms_globalMutex);
		ms_globalResourceState.erase(resource);
	}
}

void Core::ResourceStateTracker::ResourceBarrier(D3D12_RESOURCE_BARRIER const& barrier)
{
	// Transition barriers are the only barrier that needs to know the before state.
	if (barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
	{
		const D3D12_RESOURCE_TRANSITION_BARRIER& transitionBarrier = barrier.Transition;

		// Check if there is already a known final state
		const auto iter = this->m_finalResourceState.find(transitionBarrier.pResource);
		if (iter != this->m_finalResourceState.end())
		{
			auto resourceState = iter->second;

			// if we know the final state of the resouce is different
			if (transitionBarrier.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
				!resourceState.SubresourceState.empty())
			{
				// First transition all of the subresources if they are different than the StateAfter.
				for (auto subresourceState : resourceState.SubresourceState)
				{
					if (transitionBarrier.StateAfter != subresourceState.second)
					{
						D3D12_RESOURCE_BARRIER newBarrier = barrier;
						newBarrier.Transition.Subresource = subresourceState.first;
						newBarrier.Transition.StateBefore = subresourceState.second;
						this->m_resourceBarriers.push_back(newBarrier);
					}
				}
			}
			else
			{
				auto finalState = resourceState.GetSubresourceState(transitionBarrier.Subresource);
				if (transitionBarrier.StateAfter != finalState)
				{
					// Push a new transition barrier with the correct before state.
					D3D12_RESOURCE_BARRIER newBarrier = barrier;
					newBarrier.Transition.StateBefore = finalState;
					this->m_resourceBarriers.push_back(newBarrier);
				}
			}
		}
		else // Resouce being used for the first time
		{   
			// Add a pending barrier. The pending barriers will be resolved
			// before the command list is executed on the command queue.
			this->m_pendingResourceBarriers.push_back(barrier);
		}

		this->m_finalResourceState[transitionBarrier.pResource].SetSubresourceState(transitionBarrier.Subresource, transitionBarrier.StateAfter);
		return;
	}

	this->m_resourceBarriers.push_back(barrier);
}

void Core::ResourceStateTracker::TransitionResource(ID3D12Resource* resrouce, D3D12_RESOURCE_STATES stateAfter, UINT subResource)
{
	if (resrouce)
	{
		this->ResourceBarrier(
			CD3DX12_RESOURCE_BARRIER::Transition(
				resrouce,
				D3D12_RESOURCE_STATE_COMMON,
				stateAfter,
				subResource));
	}
}

void Core::ResourceStateTracker::UAVBarrier(ID3D12Resource* resource)
{
	this->ResourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(resource));
}

void Core::ResourceStateTracker::AliasBarrier(ID3D12Resource* resourceBefore, ID3D12Resource* resourceAfter)
{
	this->ResourceBarrier(
		CD3DX12_RESOURCE_BARRIER::Aliasing(resourceBefore, resourceAfter));
}

uint32_t Core::ResourceStateTracker::FlushResourceBarriers(CommandList& commandList)
{
	uint32_t numOfBarriers = static_cast<uint32_t>(this->m_resourceBarriers.size());

	if (numOfBarriers > 0)
	{
		auto d3d12CommandList = commandList.GetD3D12Impl();
		d3d12CommandList->ResourceBarrier(numOfBarriers, this->m_resourceBarriers.data());
		this->m_resourceBarriers.clear();
	}

	return numOfBarriers;
}

uint32_t Core::ResourceStateTracker::FlushPendingResourceBarriers(CommandList& commandList)
{
	// Assert is locked
	LOG_CORE_ASSERT(ms_isLocked, "Global state isn't locked");

	ResourceBarriers resourceBarriers;
	resourceBarriers.reserve(this->m_pendingResourceBarriers.size());

	for (auto pendingBarrier : this->m_pendingResourceBarriers)
	{
		if (pendingBarrier.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
		{
			continue;
		}

		auto pendingTransition = pendingBarrier.Transition;
		const auto& iter = ms_globalResourceState.find(pendingTransition.pResource);
		if (iter == ms_globalResourceState.end())
		{
			continue;
		}
		// If all subresources are being transitioned, and there are multiple
		// subresources of the resource that are in a different state...
		auto& resourceState = iter->second;
		if (pendingTransition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
			!resourceState.SubresourceState.empty())
		{
			// Transition all subresources
			for (auto subresourceState : resourceState.SubresourceState)
			{
				if (pendingTransition.StateAfter != subresourceState.second)
				{
					D3D12_RESOURCE_BARRIER newBarrier = pendingBarrier;
					newBarrier.Transition.Subresource = subresourceState.first;
					newBarrier.Transition.StateBefore = subresourceState.second;
					resourceBarriers.push_back(newBarrier);
				}
			}
			continue;
		}
		// No (sub)resources need to be transitioned. Just add a single transition barrier (if needed).
		auto globalState = (iter->second).GetSubresourceState(pendingTransition.Subresource);
		if (pendingTransition.StateAfter != globalState)
		{
			// Fix-up the before state based on current global state of the resource.
			pendingBarrier.Transition.StateBefore = globalState;
			resourceBarriers.push_back(pendingBarrier);
		}
	}

	uint32_t numBarriers = static_cast<uint32_t>(resourceBarriers.size());
	if (numBarriers > 0)
	{
		auto d3d12CommandList = commandList.GetD3D12Impl();
		d3d12CommandList->ResourceBarrier(numBarriers, resourceBarriers.data());
	}

	this->m_pendingResourceBarriers.clear();

	return numBarriers;
}

void Core::ResourceStateTracker::CommitFinalResourceStates()
{
	LOG_CORE_ASSERT(ms_isLocked, "Global Resource tracker isn't locked");

	// Commit final resource states to the global resource state array (map).
	for (const auto& resourceState : m_finalResourceState)
	{
		ms_globalResourceState[resourceState.first] = resourceState.second;
	}

	m_finalResourceState.clear();
}

void Core::ResourceStateTracker::Reset()
{
	// Reset the pending, current, and final resource states.
	m_pendingResourceBarriers.clear();
	m_resourceBarriers.clear();
	m_finalResourceState.clear();
}
