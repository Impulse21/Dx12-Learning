#pragma once

#include "d3dx12.h"
#include "CommandList.h"

#include <map>
#include <unordered_map>
#include <mutex>

namespace Core
{
	class CommandList;

	class ResourceStateTracker
	{
	public:
		ResourceStateTracker() = default;
		~ResourceStateTracker() = default;

		/** Global State Trackers */
		static void Lock();
		static void Unlock();

		static void AddGlobalResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);
		static void RemoveGlobalResourceState(ID3D12Resource* resource);

		/** Global State Trackers End */
		void ResourceBarrier(D3D12_RESOURCE_BARRIER const& barrier);

		void TransitionResource(ID3D12Resource* resrouce, D3D12_RESOURCE_STATES stateAfter, UINT subResource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

		void UAVBarrier(ID3D12Resource* resource = nullptr);

		void AliasBarrier(ID3D12Resource* resourceBefore, ID3D12Resource* resourceAfter = nullptr);

		// Flush any pending resource parriers to the command list
		uint32_t FlushPendingResourceBarriers(CommandList& commandList);

		/**
		  * Flush any (non-pending) resource barriers that have been pushed to the resource state
		  * tracker.
		  */
		uint32_t FlushResourceBarriers(CommandList& commandList);

		void CommitFinalResourceStates();

		void Reset();

	private:
		using ResourceBarriers = std::vector<D3D12_RESOURCE_BARRIER>;

		// Resource barriers that need to be committed to the command list.
		ResourceBarriers m_resourceBarriers;
		ResourceBarriers m_pendingResourceBarriers;

		struct ResourceState
		{
			explicit ResourceState(D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON)
				: State(state) {}

			void SetSubresourceState(uint32_t subResource, D3D12_RESOURCE_STATES state)
			{
				if (subResource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
				{
					State = state;
					SubresourceState.clear();
				}
				else
				{
					SubresourceState[subResource] = state;
				}
			}

			D3D12_RESOURCE_STATES GetSubresourceState(uint32_t subResource) const
			{
				D3D12_RESOURCE_STATES state = State;
				const auto itr = SubresourceState.find(subResource);
				if (itr != SubresourceState.end())
				{
					state = itr->second;
				}

				return state;
			}

			// If the SubresourceState array (map) is empty, then the State variable defines 
			// the state of all of the subresources.
			D3D12_RESOURCE_STATES State;
			std::map<uint32_t, D3D12_RESOURCE_STATES> SubresourceState;
		};

		using ResourceStateMap = std::unordered_map<ID3D12Resource*, ResourceState>;

		// The final (last known state) of the resources within a command list.
		// The final resource state is committed to the global resource state when the 
		// command list is closed but before it is executed on the command queue.
		ResourceStateMap m_finalResourceState;

		/** Global State */
		// The global resource state array (map) stores the state of a resource
	// between command list execution.
		static ResourceStateMap ms_globalResourceState;
		static std::mutex ms_globalMutex;
		static bool ms_isLocked;
	};
}

