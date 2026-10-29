#pragma once

#include <Windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>

// The sprite root signature and pipeline state, the pair every draw this
// backend makes binds.
//
// Backend-private: Renderer::Impl holds one by value and keeps coordinating
// everything around it - descriptor heaps, command lists, the index upload,
// waits and device-loss callbacks stay in renderer.cpp. This owner borrows the
// device and the target format for create and keeps neither.

namespace labrador
{
	class SpritePipeline
	{
	public:
		// Builds the root signature, then the pipeline state against it,
		// assigning each member in place. A root signature that will not
		// serialise throws with the API's reason and leaves both members as
		// they were. A failed creation call leaves its own member empty; a
		// root signature created before a failed pipeline creation stays
		// until reset() or destruction.
		void create(ID3D12Device* device, DXGI_FORMAT target_format);

		// Releases the pipeline state, then the root signature.
		void reset();

		ID3D12RootSignature* root_signature() const
		{
			return this->root_signature_.Get();
		}

		ID3D12PipelineState* pipeline_state() const
		{
			return this->pipeline_state_.Get();
		}

	private:
		// Declared root first, so destruction releases the pipeline state
		// before the root signature it was made against, as reset() does.
		Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state_;
	};
}
