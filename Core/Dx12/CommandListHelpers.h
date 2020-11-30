#pragma once

#include "d3dx12.h"
#include <wrl.h>
#include <array>

namespace Core
{
	class CommandListHelpers
	{
	public:
		static void TransitionResource(
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
			Microsoft::WRL::ComPtr<ID3D12Resource> resource,
			D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
		{

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				resource.Get(),
				beforeState, afterState);

			commandList->ResourceBarrier(1, &barrier);
		}
		static void ClearRenderTarget(
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
			D3D12_CPU_DESCRIPTOR_HANDLE rtv,
			std::array<FLOAT, 4> clearColour)
		{
			commandList->ClearRenderTargetView(rtv, clearColour.data(), 0, nullptr);
		}

		void ClearDepthStencilView(
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
			D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth)
		{
			commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
		}
	};
}