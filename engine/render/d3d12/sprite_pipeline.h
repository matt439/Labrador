#pragma once

#include <Windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>

// The sprite root signature and pipeline state, the pair every draw this
// backend makes binds, and the post stage's pair beside them.
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
		// The root signature's parameters, in order: the pixels-to-clip
		// transform as root constants, then the texture's and the sampler's
		// descriptor tables. A draw binds them by these indices.
		static constexpr UINT transform_parameter = 0;
		static constexpr UINT texture_parameter = 1;
		static constexpr UINT sampler_parameter = 2;
		static constexpr UINT parameter_count = 3;

		// How many 32-bit values the transform is.
		static constexpr UINT transform_constants = 4;

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

	// The post stage's root signature and one pipeline state a material
	// (docs/effects.md#5): identity and the CRT. Each is one full-screen
	// triangle from the vertex index, no input layout, and blending off,
	// because a full-screen pass replaces what it covers. Both materials
	// include post_frame.hlsli, so both run against the one root signature.
	//
	// Backend-private and held by value beside SpritePipeline, under the same
	// terms: it borrows the device and the target format for create and keeps
	// neither.
	class PostPipeline
	{
	public:
		// The root signature's parameters, in order: post_frame.hlsli's
		// PostFrame cbuffer as root constants at b0, visible to both stages,
		// then the scene's descriptor table at t0. The two samplers are
		// static: s0 point and s1 linear, clamped at level zero like the
		// sprite samplers.
		static constexpr UINT frame_parameter = 0;
		static constexpr UINT input_parameter = 1;
		static constexpr UINT parameter_count = 2;

		// How many 32-bit values the frame is: eleven float4s.
		static constexpr UINT frame_constants = 44;

		// Builds the root signature, then each material's pipeline state
		// against it, with the same failure terms as SpritePipeline::create.
		void create(ID3D12Device* device, DXGI_FORMAT target_format);

		// Releases the pipeline states, then the root signature.
		void reset();

		ID3D12RootSignature* root_signature() const
		{
			return this->root_signature_.Get();
		}

		ID3D12PipelineState* identity_state() const
		{
			return this->identity_state_.Get();
		}

		ID3D12PipelineState* crt_state() const
		{
			return this->crt_state_.Get();
		}

	private:
		Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> identity_state_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> crt_state_;
	};
}
