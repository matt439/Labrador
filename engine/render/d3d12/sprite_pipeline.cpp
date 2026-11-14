#include "engine/render/d3d12/sprite_pipeline.h"

#include "engine/render/sprite_vertex.h"
#include "engine/render/throw_if_failed.h"

// The shaders, as bytes this build compiled (cmake/compile_shaders.cmake) from
// engine/render/sprite.hlsl - the same source the D3D11 backend compiles, at a
// different profile. That file says why it is one file and not two. The post
// vertex shader and identity material come from
// engine/render/materials/identity.hlsl, the CRT material from
// engine/render/materials/crt.hlsl, and both include the frame,
// engine/render/post_frame.hlsli.
#include "engine/render/d3d12/post_crt_pixel_shader.h"
#include "engine/render/d3d12/post_identity_pixel_shader.h"
#include "engine/render/d3d12/post_vertex_shader.h"
#include "engine/render/d3d12/sprite_pixel_shader.h"
#include "engine/render/d3d12/sprite_vertex_shader.h"

#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;

namespace labrador
{
	namespace
	{
		// Serialises `description` for CreateRootSignature. `name` says whose
		// it is in the exception: "sprite" or "post".
		ComPtr<ID3DBlob> serialize_root_signature(
			const D3D12_ROOT_SIGNATURE_DESC& description, const std::string& name)
		{
			ComPtr<ID3DBlob> serialized;
			ComPtr<ID3DBlob> serialize_error;
			const HRESULT serialized_hr = D3D12SerializeRootSignature(
				&description, D3D_ROOT_SIGNATURE_VERSION_1_0,
				serialized.GetAddressOf(), serialize_error.GetAddressOf());
			if (FAILED(serialized_hr))
			{
				// The blob carries a sentence about which parameter is wrong,
				// and throwing the HRESULT alone would throw that away - which
				// is the same argument resource_factory.h makes about an
				// eight-digit HRESULT standing in for a reason (T6).
				const std::string reason = serialize_error
					? std::string(static_cast<const char*>(
						serialize_error->GetBufferPointer()),
						serialize_error->GetBufferSize())
					: std::string("no reason given");
				throw std::runtime_error("The " + name +
					" root signature would not serialise: " + reason);
			}
			return serialized;
		}

		// No culling and no depth bias: every draw this backend makes, sprite
		// or full-screen, rasterises the same way.
		D3D12_RASTERIZER_DESC rasterizer_state()
		{
			D3D12_RASTERIZER_DESC rasterizer = {};
			rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
			rasterizer.CullMode = D3D12_CULL_MODE_NONE;
			rasterizer.FrontCounterClockwise = FALSE;
			rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
			rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
			rasterizer.SlopeScaledDepthBias =
				D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
			rasterizer.DepthClipEnable = TRUE;
			rasterizer.MultisampleEnable = TRUE;
			rasterizer.AntialiasedLineEnable = FALSE;
			rasterizer.ForcedSampleCount = 0;
			rasterizer.ConservativeRaster =
				D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
			return rasterizer;
		}
	}

	void SpritePipeline::create(ID3D12Device* device,
		DXGI_FORMAT target_format)
	{
		// --- The root signature ---------------------------------------------
		//
		// THREE PARAMETERS, AND THE FIRST ONE IS WHERE THE CONSTANT BUFFER
		// WENT. The D3D11 backend gives the vertex shader its pixels-to-clip
		// transform through a dynamic constant buffer per view, mapped on every
		// viewport change - sixteen bytes behind a buffer object, because that
		// is the only way that API takes them. Here four floats are four root
		// constants written straight into the command list, so there is no
		// buffer, no map, no per-view allocation and nothing to rebuild on a
		// device loss. The shader is identical; only the binding is not.
		D3D12_DESCRIPTOR_RANGE texture_range = {};
		texture_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		texture_range.NumDescriptors = 1;
		texture_range.BaseShaderRegister = 0;
		texture_range.RegisterSpace = 0;
		texture_range.OffsetInDescriptorsFromTableStart =
			D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE sampler_range = {};
		sampler_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		sampler_range.NumDescriptors = 1;
		sampler_range.BaseShaderRegister = 0;
		sampler_range.RegisterSpace = 0;
		sampler_range.OffsetInDescriptorsFromTableStart =
			D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER parameters[parameter_count] = {};
		D3D12_ROOT_PARAMETER& transform = parameters[transform_parameter];
		transform.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		transform.Constants.ShaderRegister = 0;
		transform.Constants.RegisterSpace = 0;
		transform.Constants.Num32BitValues = transform_constants;
		transform.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		D3D12_ROOT_PARAMETER& texture = parameters[texture_parameter];
		texture.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		texture.DescriptorTable.NumDescriptorRanges = 1;
		texture.DescriptorTable.pDescriptorRanges = &texture_range;
		texture.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_PARAMETER& sampler = parameters[sampler_parameter];
		sampler.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		sampler.DescriptorTable.NumDescriptorRanges = 1;
		sampler.DescriptorTable.pDescriptorRanges = &sampler_range;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC root_description = {};
		root_description.NumParameters =
			static_cast<UINT>(std::size(parameters));
		root_description.pParameters = parameters;
		root_description.NumStaticSamplers = 0;
		root_description.pStaticSamplers = nullptr;
		root_description.Flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		const ComPtr<ID3DBlob> serialized =
			serialize_root_signature(root_description, "sprite");
		ThrowIfFailed(device->CreateRootSignature(0,
			serialized->GetBufferPointer(), serialized->GetBufferSize(),
			IID_PPV_ARGS(this->root_signature_.ReleaseAndGetAddressOf())),
			"The sprite root signature could not be created");

		// --- The pipeline state ---------------------------------------------
		//
		// ONE OBJECT WHERE D3D11 HAS FIVE, AND IT IS MADE ONCE. The blend, the
		// depth state, the rasteriser state, the input layout and the two
		// shaders are all fields of it, so this backend has nothing to bind per
		// flush that the D3D11 one binds - and nothing to choose between,
		// because every draw this engine makes wants the same one. The two
		// samplers are the exception and are not in here: the seam lets a draw
		// list change its filter mid-list, so they are descriptors a draw call
		// names rather than state a pipeline fixes.
		const D3D12_INPUT_ELEMENT_DESC elements[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
				static_cast<UINT>(offsetof(SpriteVertex, position)),
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
				static_cast<UINT>(offsetof(SpriteVertex, colour)),
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
				static_cast<UINT>(offsetof(SpriteVertex, texcoord)),
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline = {};
		pipeline.pRootSignature = this->root_signature_.Get();
		pipeline.VS = { sprite_vertex_shader, sizeof(sprite_vertex_shader) };
		pipeline.PS = { sprite_pixel_shader, sizeof(sprite_pixel_shader) };

		// PREMULTIPLIED ALPHA: the source factor is ONE and not SRC_ALPHA.
		// RenderPixelTests calls this the term most likely to be got wrong,
		// because both answers look plausible and every opaque sprite renders
		// identically either way.
		pipeline.BlendState.AlphaToCoverageEnable = FALSE;
		pipeline.BlendState.IndependentBlendEnable = FALSE;
		pipeline.BlendState.RenderTarget[0].BlendEnable = TRUE;
		pipeline.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
		pipeline.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		pipeline.BlendState.RenderTarget[0].DestBlend =
			D3D12_BLEND_INV_SRC_ALPHA;
		pipeline.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		pipeline.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		pipeline.BlendState.RenderTarget[0].DestBlendAlpha =
			D3D12_BLEND_INV_SRC_ALPHA;
		pipeline.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		pipeline.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
		pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask =
			D3D12_COLOR_WRITE_ENABLE_ALL;

		pipeline.SampleMask = UINT_MAX;

		// NO CULLING. A sprite is a quad whose winding never changes: a flip
		// mirrors the texture coordinates and leaves the corners where they
		// were (sprite_geometry.cpp).
		pipeline.RasterizerState = rasterizer_state();

		// No depth, because there is no depth buffer to test against.
		pipeline.DepthStencilState.DepthEnable = FALSE;
		pipeline.DepthStencilState.StencilEnable = FALSE;

		pipeline.InputLayout = { elements,
			static_cast<UINT>(std::size(elements)) };
		pipeline.PrimitiveTopologyType =
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipeline.NumRenderTargets = 1;
		pipeline.RTVFormats[0] = target_format;
		pipeline.DSVFormat = DXGI_FORMAT_UNKNOWN;
		pipeline.SampleDesc.Count = 1;
		pipeline.SampleDesc.Quality = 0;

		ThrowIfFailed(device->CreateGraphicsPipelineState(&pipeline,
			IID_PPV_ARGS(this->pipeline_state_.ReleaseAndGetAddressOf())),
			"The sprite pipeline state could not be created");
	}

	void SpritePipeline::reset()
	{
		this->pipeline_state_.Reset();
		this->root_signature_.Reset();
	}

	void PostPipeline::create(ID3D12Device* device, DXGI_FORMAT target_format)
	{
		// --- The root signature ---------------------------------------------
		//
		// THE WHOLE FRAME AS ROOT CONSTANTS: 44 DWORDs plus one for the table,
		// under the 64 a root signature holds, so the values set_post_values
		// gave go straight into the command list with no buffer to keep per
		// frame in flight. A frame past the limit moves to a root CBV
		// (docs/effects.md#5).
		D3D12_DESCRIPTOR_RANGE input_range = {};
		input_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		input_range.NumDescriptors = 1;
		input_range.BaseShaderRegister = 0;
		input_range.RegisterSpace = 0;
		input_range.OffsetInDescriptorsFromTableStart =
			D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER parameters[parameter_count] = {};
		D3D12_ROOT_PARAMETER& frame = parameters[frame_parameter];
		frame.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		frame.Constants.ShaderRegister = 0;
		frame.Constants.RegisterSpace = 0;
		frame.Constants.Num32BitValues = frame_constants;
		frame.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_PARAMETER& input = parameters[input_parameter];
		input.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		input.DescriptorTable.NumDescriptorRanges = 1;
		input.DescriptorTable.pDescriptorRanges = &input_range;
		input.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// STATIC, because which filter a pass samples with is the material's
		// choice and never the caller's: s0 point and s1 linear, clamped and
		// at level zero, as the sprite samplers are.
		D3D12_STATIC_SAMPLER_DESC samplers[2] = {};
		for (UINT i = 0; i < 2; i++)
		{
			D3D12_STATIC_SAMPLER_DESC& sampler = samplers[i];
			sampler.Filter = i == 0
				? D3D12_FILTER_MIN_MAG_MIP_POINT
				: D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.MipLODBias = 0.0f;
			sampler.MaxAnisotropy = 1;
			sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
			sampler.MinLOD = 0.0f;
			sampler.MaxLOD = 0.0f;
			sampler.ShaderRegister = i;
			sampler.RegisterSpace = 0;
			sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}

		D3D12_ROOT_SIGNATURE_DESC root_description = {};
		root_description.NumParameters =
			static_cast<UINT>(std::size(parameters));
		root_description.pParameters = parameters;
		root_description.NumStaticSamplers =
			static_cast<UINT>(std::size(samplers));
		root_description.pStaticSamplers = samplers;
		// No input-assembler flag: the triangle comes from SV_VertexID.
		root_description.Flags =
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		const ComPtr<ID3DBlob> serialized =
			serialize_root_signature(root_description, "post");
		ThrowIfFailed(device->CreateRootSignature(0,
			serialized->GetBufferPointer(), serialized->GetBufferSize(),
			IID_PPV_ARGS(this->root_signature_.ReleaseAndGetAddressOf())),
			"The post root signature could not be created");

		// --- The material pipeline states -----------------------------------
		//
		// Every field but the pixel shader is shared, so the description is
		// filled once and only PS changes between the two creates.
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline = {};
		pipeline.pRootSignature = this->root_signature_.Get();
		pipeline.VS = { post_vertex_bytecode, sizeof(post_vertex_bytecode) };
		pipeline.PS = { post_identity_bytecode, sizeof(post_identity_bytecode) };

		// A FULL-SCREEN PASS REPLACES; only draw lists blend (docs/effects.md).
		pipeline.BlendState.AlphaToCoverageEnable = FALSE;
		pipeline.BlendState.IndependentBlendEnable = FALSE;
		pipeline.BlendState.RenderTarget[0].BlendEnable = FALSE;
		pipeline.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
		pipeline.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		pipeline.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		pipeline.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		pipeline.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		pipeline.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		pipeline.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		pipeline.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
		pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask =
			D3D12_COLOR_WRITE_ENABLE_ALL;

		pipeline.SampleMask = UINT_MAX;
		pipeline.RasterizerState = rasterizer_state();
		pipeline.DepthStencilState.DepthEnable = FALSE;
		pipeline.DepthStencilState.StencilEnable = FALSE;

		pipeline.InputLayout = { nullptr, 0 };
		pipeline.PrimitiveTopologyType =
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipeline.NumRenderTargets = 1;
		pipeline.RTVFormats[0] = target_format;
		pipeline.DSVFormat = DXGI_FORMAT_UNKNOWN;
		pipeline.SampleDesc.Count = 1;
		pipeline.SampleDesc.Quality = 0;

		ThrowIfFailed(device->CreateGraphicsPipelineState(&pipeline,
			IID_PPV_ARGS(this->identity_state_.ReleaseAndGetAddressOf())),
			"The identity post pipeline state could not be created");

		pipeline.PS = { post_crt_bytecode, sizeof(post_crt_bytecode) };
		ThrowIfFailed(device->CreateGraphicsPipelineState(&pipeline,
			IID_PPV_ARGS(this->crt_state_.ReleaseAndGetAddressOf())),
			"The CRT post pipeline state could not be created");
	}

	void PostPipeline::reset()
	{
		this->crt_state_.Reset();
		this->identity_state_.Reset();
		this->root_signature_.Reset();
	}
}
