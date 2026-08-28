#include "engine/render/d3d11/backend.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/sprite_geometry.h"
#include "engine/render/sprite_vertex.h"
#include "engine/render/throw_if_failed.h"

// The shaders, as bytes this build compiled (cmake/compile_shaders.cmake).
// Nothing is read from disk at run time and nothing is deployed beside the
// executable.
#include "engine/render/d3d11/sprite_pixel_shader.h"
#include "engine/render/d3d11/sprite_vertex_shader.h"

#include <cstddef>
#include <cstring>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace mattmath;

namespace labrador
{
	namespace
	{
		// Where the engine's values become the backend's. This used to be a
		// member of the mattmath type - Viewport::d3d_viewport() - which is
		// what put <d3d11.h> in a library documented as depending on nothing.
		// This file was its only caller, so this is where it goes. A second
		// backend writes its own; it does not inherit this.
		//
		// D3D11_VIEWPORT's four extent members are FLOAT and a fractional
		// viewport is legal, so this backend could keep the fraction - as could
		// d3d12, whose D3D12_VIEWPORT is this struct spelt differently, and
		// vulkan, whose VkViewport is float too. GL is the one that cannot:
		// glViewport takes GLint and GLsizei. None of the three keeps it,
		// because then they would disagree about a pane and the seam's whole
		// claim is that they cannot (viewport.h). A no-op on every viewport this tree produces, all of
		// which are whole pixels already.
		D3D11_VIEWPORT to_d3d_viewport(const Viewport& viewport)
		{
			const mattmath::RectangleI pixels = viewport.pixel_rect();

			return { static_cast<float>(pixels.x),
				static_cast<float>(pixels.y),
				static_cast<float>(pixels.width),
				static_cast<float>(pixels.height),
				viewport.minDepth, viewport.maxDepth };
		}

		// Four corners, two triangles, and the winding the corner order in
		// sprite_geometry.h fixes.
		const int VERTICES_PER_SPRITE = 4;
		const int INDICES_PER_SPRITE = 6;

		// What the vertex shader's one constant buffer holds. Sixteen bytes,
		// which is the smallest a constant buffer is allowed to be.
		struct ViewportTransform
		{
			float x_scale = 0.0f;
			float y_scale = 0.0f;
			float x_offset = 0.0f;
			float y_offset = 0.0f;
		};

		// Pixels to clip space.
		//
		// THE ONE LINE WHERE Y RUNNING DOWN IS DECIDED. The seam's y increases
		// down the screen and clip space's increases up, so the y scale is
		// negative and the offset is +1 rather than -1. Every other backend
		// writes the same four numbers, the OpenGL one included: a framebuffer
		// origin at the bottom is a fact about where a PANE sits, which
		// gl/renderer.cpp answers when it calls glViewport, and not about how a
		// pixel maps into clip space. Vulkan reaches the same place from the
		// other side, by handing the rasteriser a negative viewport height. The
		// four numbers here are the seam's arithmetic and they do not vary.
		//
		// The viewport's position is not in it: the rasteriser already maps
		// clip space onto the viewport rectangle, so a sprite at pixel (0,0)
		// lands at the viewport's top left wherever that is on the back buffer.
		ViewportTransform to_transform(const D3D11_VIEWPORT& viewport)
		{
			ViewportTransform transform;
			transform.x_scale =
				viewport.Width > 0.0f ? 2.0f / viewport.Width : 0.0f;
			transform.y_scale =
				viewport.Height > 0.0f ? -2.0f / viewport.Height : 0.0f;
			transform.x_offset = -1.0f;
			transform.y_offset = 1.0f;
			return transform;
		}
	}

	// --- DrawList::View ------------------------------------------------------

	// Nothing is recorded until a flush, so a view the scene declared and then
	// drew nothing into records nothing at all - which is what makes declaring
	// the capacity up front cheap.
	void DrawList::View::draw(ID3D11ShaderResourceView* texture,
		const SpriteVertex* corners)
	{
		// ONE TEXTURE PER DRAW CALL, so a run of sprites sharing one is one
		// call and a change is a flush. That is the whole of the batching: the
		// paint tiles of a level, the glyphs of a HUD line and the frames of a
		// sheet each share a texture and each collapse into a single draw,
		// which is why a scene with thousands of sprites has tens of calls.
		if (texture != this->batch_texture ||
			this->batch.size() >= static_cast<size_t>(MAX_BATCH_SPRITES) *
				VERTICES_PER_SPRITE)
		{
			this->flush();
			this->batch_texture = texture;
		}

		this->batch.insert(this->batch.end(), corners,
			corners + VERTICES_PER_SPRITE);
		this->touched = true;
	}

	void DrawList::View::flush()
	{
		if (this->batch.empty())
		{
			return;
		}

		const int sprites =
			static_cast<int>(this->batch.size()) / VERTICES_PER_SPRITE;

		// DISCARD ON A WRAP OR AT THE TOP OF A FRAME, NO_OVERWRITE OTHERWISE,
		// and the distinction is what keeps a flush from stalling. NO_OVERWRITE
		// promises the driver that nothing already queued reads the range about
		// to be written, so it need not wait; DISCARD asks for fresh memory,
		// which is the only honest answer once the buffer is full and the
		// earlier ranges are still being read by draws recorded this frame. THE
		// SECOND DISCARD CASE IS THE FIRST WRITE AFTER A reset(), which
		// begin_frame calls on every view: position zero is either a wrap that
		// has just happened or a frame that has just started, and both want the
		// same answer for the same reason.
		D3D11_MAP how = D3D11_MAP_WRITE_NO_OVERWRITE;
		if (this->buffer_position + sprites > MAX_BATCH_SPRITES)
		{
			this->buffer_position = 0;
			how = D3D11_MAP_WRITE_DISCARD;
		}
		if (this->buffer_position == 0)
		{
			how = D3D11_MAP_WRITE_DISCARD;
		}

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		ThrowIfFailed(this->context->Map(this->vertices.Get(), 0, how, 0,
			&mapped));
		SpriteVertex* destination =
			static_cast<SpriteVertex*>(mapped.pData) +
			static_cast<size_t>(this->buffer_position) * VERTICES_PER_SPRITE;
		std::memcpy(destination, this->batch.data(),
			this->batch.size() * sizeof(SpriteVertex));
		this->context->Unmap(this->vertices.Get(), 0);

		Renderer::Impl& owner_impl = *this->owner;

		const UINT stride = sizeof(SpriteVertex);
		const UINT offset = 0;
		ID3D11Buffer* vertex_buffer = this->vertices.Get();
		ID3D11Buffer* constant_buffer = this->transform.Get();
		ID3D11SamplerState* sampler = owner_impl.sampler(this->filter);
		const float blend_factor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

		// SET IN FULL ON EVERY FLUSH rather than once per view. A flush is
		// already a draw call, these are a dozen more commands into the same
		// deferred context, and the alternative is a rule about which of them
		// survives a viewport change and which does not - which is the rule the
		// four hand-written copies of this protocol used to disagree about.
		this->context->IASetInputLayout(owner_impl.input_layout.Get());
		this->context->IASetPrimitiveTopology(
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		this->context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride,
			&offset);
		this->context->IASetIndexBuffer(owner_impl.indices.Get(),
			DXGI_FORMAT_R16_UINT, 0);

		this->context->VSSetShader(owner_impl.vertex_shader.Get(), nullptr, 0);
		this->context->VSSetConstantBuffers(0, 1, &constant_buffer);

		this->context->PSSetShader(owner_impl.pixel_shader.Get(), nullptr, 0);
		this->context->PSSetShaderResources(0, 1, &this->batch_texture);
		this->context->PSSetSamplers(0, 1, &sampler);

		this->context->OMSetBlendState(owner_impl.blend.Get(), blend_factor,
			0xFFFFFFFFu);
		this->context->OMSetDepthStencilState(owner_impl.depth.Get(), 0);
		this->context->RSSetState(owner_impl.rasterizer.Get());

		this->context->DrawIndexed(
			static_cast<UINT>(sprites * INDICES_PER_SPRITE),
			static_cast<UINT>(this->buffer_position * INDICES_PER_SPRITE), 0);

		this->buffer_position += sprites;
		this->batch.clear();
	}

	void DrawList::View::set_transform(const D3D11_VIEWPORT& viewport)
	{
		const ViewportTransform values = to_transform(viewport);

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		ThrowIfFailed(this->context->Map(this->transform.Get(), 0,
			D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		std::memcpy(mapped.pData, &values, sizeof(values));
		this->context->Unmap(this->transform.Get(), 0);
	}

	void DrawList::View::bind(ID3D11RenderTargetView* render_target,
		const D3D11_VIEWPORT& viewport)
	{
		if (this->bound)
		{
			return;
		}

		this->context->OMSetRenderTargets(1, &render_target, nullptr);
		this->context->RSSetViewports(1, &viewport);
		this->set_transform(viewport);
		this->bound = true;
	}

	void DrawList::View::reset()
	{
		this->camera = Camera::DEFAULT_CAMERA;
		this->filter = TextureFilter::point;
		this->batch.clear();
		this->batch_texture = nullptr;
		this->buffer_position = 0;
		this->touched = false;
		this->bound = false;
	}

	ID3D11CommandList* DrawList::View::finish()
	{
		this->flush();

		ID3D11CommandList* commands = nullptr;
		// FALSE: the deferred context's state is not restored afterwards, and
		// `bound` is cleared below to say so - set_view_count binds again on
		// the next frame, which is the only thing that puts a render target
		// back on this context. The two hand-written copies of this protocol
		// disagreed about this flag - TRUE in Level, FALSE in MenuPage - and
		// neither had a reason written down.
		ThrowIfFailed(this->context->FinishCommandList(FALSE, &commands));
		this->bound = false;
		return commands;
	}

	void DrawList::View::discard()
	{
		if (!this->bound)
		{
			return;
		}

		// NO flush() FIRST, AND THE HRESULT IS NOT CHECKED. Both follow from
		// this being the throwing-away path. What is still batched is CPU
		// memory that reset() is about to clear, and mapping a buffer to record
		// a draw call nobody will execute is work for nothing; and the one way
		// FinishCommandList fails here is a device this frame's exception was
		// probably reporting in the first place, on a context OnDeviceLost is
		// about to release. Turning that into a throw out of begin_frame would
		// make an abandoned frame kill the process on one of the three backends
		// that have device loss at all - this one, d3d12 and vulkan, the last
		// reaching it from VK_ERROR_DEVICE_LOST rather than from DXGI.
		ID3D11CommandList* stranded = nullptr;
		if (SUCCEEDED(this->context->FinishCommandList(FALSE, &stranded)) &&
			stranded != nullptr)
		{
			stranded->Release();
		}
		this->bound = false;
	}

	// --- DrawList ------------------------------------------------------------

	void DrawList::set_viewport(const Viewport& viewport)
	{
		// Both, and in this order. RSSetViewports is what the rasteriser clips
		// against; the constant buffer is what the vertex shader projects with,
		// and on a deferred context nothing can read the first one back to
		// derive the second. Everything already recorded belongs to the old
		// viewport, so it goes out first.
		this->view_->flush();

		const D3D11_VIEWPORT d3d_viewport = to_d3d_viewport(viewport);
		this->view_->context->RSSetViewports(1, &d3d_viewport);
		this->view_->set_transform(d3d_viewport);
	}

	void DrawList::set_camera(const Camera& camera)
	{
		// No flush: the camera is applied to the geometry as each draw is
		// recorded, not by any state the batch holds.
		this->view_->camera = camera;
	}

	void DrawList::set_filter(TextureFilter filter)
	{
		if (this->view_->filter == filter)
		{
			return;
		}
		// One sampler per draw call, so a change is a flush - which is why the
		// seam says to group by filter if it matters.
		this->view_->flush();
		this->view_->filter = filter;
	}

	void DrawList::draw_sprite(TextureHandle texture,
		const RectangleI& source,
		const RectangleF& destination,
		const Colour& tint,
		float rotation,
		const Vector2F& origin,
		SpriteFlip flip,
		float layer_depth)
	{
		// UNUSED, AND SAYING SO IS THE POINT. layer_depth orders nothing -
		// RenderPixelTests pins that call order decides - and now that the
		// sprite vertex has no z there is nowhere left for the value to go. It
		// stays on the seam because the seam is what a client writes against
		// and because a backend that ever does sort will want it; it does not
		// stay in the vertex, where it was written and ignored.
		std::ignore = layer_depth;

		ID3D11ShaderResourceView* view =
			this->view_->owner->resources->impl()->texture(texture);

		SpriteVertex corners[4];
		build_sprite_quad(
			this->view_->camera.calculate_view_rectangle(destination),
			source, Renderer::Impl::texture_size(view), tint, rotation, origin,
			flip, corners);

		this->view_->draw(view, corners);
	}

	void DrawList::draw_text(FontHandle font,
		const std::wstring& text,
		const Vector2F& position,
		const Colour& tint,
		float scale,
		float rotation,
		const Vector2F& origin,
		float layer_depth)
	{
		std::ignore = layer_depth;

		// Two tables and two lookups, and which side of the seam each comes
		// from is the rule rather than a habit: a Font is engine data, so its
		// table is the seam's own; a shader resource view is not, so its table
		// is this folder's.
		const RenderResources& resources = *this->view_->owner->resources;
		const Font& the_font = *resources.font(font);

		// A GLYPH IS A SPRITE, AND THAT IS THE WHOLE OF THIS FUNCTION. The walk
		// is the engine's (font.h) and so is the quad (sprite_geometry.h); what
		// is left here is resolving two handles and asking for one quad per
		// glyph. A second backend writes this and nothing more to draw text.
		ID3D11ShaderResourceView* atlas =
			resources.impl()->texture(the_font.atlas());
		const Vector2F atlas_size = Renderer::Impl::texture_size(atlas);

		const Vector2F screen_position =
			this->view_->camera.calculate_view_position(position);
		const float screen_scale =
			this->view_->camera.calculate_view_scale(scale);

		// THE PEN AND THE BEARING ARE THE ENGINE'S TOO, and this backend no
		// longer spells either of them. build_glyph_quad takes the glyph and
		// the pen the walk reported and answers with four corners, so what is
		// left here is resolving two handles and submitting.
		DrawList::View& view = *this->view_;
		the_font.for_each_glyph(text,
			[&](const Glyph& glyph, const Vector2F& pen)
			{
				SpriteVertex corners[4];
				build_glyph_quad(screen_position, screen_scale, glyph, pen,
					atlas_size, tint, rotation, origin, corners);

				view.draw(atlas, corners);
			});
	}

	// --- Renderer::Impl ------------------------------------------------------

	Renderer::Impl::Impl()
	{
		this->device_resources.RegisterDeviceNotify(this);
	}

	ID3D11SamplerState* Renderer::Impl::sampler(TextureFilter filter) const
	{
		// Held rather than demanded from a state cache, because there are two
		// of them and they are made with the device. Two objects used to cache
		// CommonStates::PointClamp() as a raw pointer and hold it across a
		// device loss, which freed the CommonStates that owned it; these are
		// remade in create_device_dependent_resources like everything else, and
		// nothing outside this class ever holds one.
		return filter == TextureFilter::linear
			? this->linear_sampler.Get()
			: this->point_sampler.Get();
	}

	Vector2F Renderer::Impl::texture_size(ID3D11ShaderResourceView* texture)
	{
		// ASKED OF THE VIEW, EVERY DRAW. The alternative is a size cached
		// beside each texture in the table, which is a second thing to keep
		// right across a device loss for a value the runtime already holds -
		// and this is three virtual calls into the runtime's own bookkeeping,
		// counting the QueryInterface the As() below is, not a GPU round trip. DirectXTK did exactly this, in the same place.
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		texture->GetResource(resource.GetAddressOf());

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_2d;
		ThrowIfFailed(resource.As(&texture_2d));

		D3D11_TEXTURE2D_DESC description = {};
		texture_2d->GetDesc(&description);
		return Vector2F(static_cast<float>(description.Width),
			static_cast<float>(description.Height));
	}

	void Renderer::Impl::create_device_dependent_resources()
	{
		ID3D11Device1* device = this->device_resources.GetD3DDevice();

		ThrowIfFailed(device->CreateVertexShader(SPRITE_VERTEX_SHADER,
			sizeof(SPRITE_VERTEX_SHADER), nullptr,
			this->vertex_shader.ReleaseAndGetAddressOf()));
		ThrowIfFailed(device->CreatePixelShader(SPRITE_PIXEL_SHADER,
			sizeof(SPRITE_PIXEL_SHADER), nullptr,
			this->pixel_shader.ReleaseAndGetAddressOf()));

		// The three fields of SpriteVertex, located by offsetof rather than by
		// position, which is why sprite_vertex.h is free to declare them in any
		// order it likes.
		const D3D11_INPUT_ELEMENT_DESC elements[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
				offsetof(SpriteVertex, position),
				D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
				offsetof(SpriteVertex, colour),
				D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
				offsetof(SpriteVertex, texcoord),
				D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		ThrowIfFailed(device->CreateInputLayout(elements,
			static_cast<UINT>(std::size(elements)), SPRITE_VERTEX_SHADER,
			sizeof(SPRITE_VERTEX_SHADER),
			this->input_layout.ReleaseAndGetAddressOf()));

		// PREMULTIPLIED ALPHA: the source factor is ONE and not SRC_ALPHA.
		// RenderPixelTests calls this the term most likely to be got wrong,
		// because both answers look plausible and every opaque sprite in both
		// samples renders identically either way. It is four lines here and it
		// is the whole difference.
		D3D11_BLEND_DESC blend_description = {};
		blend_description.RenderTarget[0].BlendEnable = TRUE;
		blend_description.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blend_description.RenderTarget[0].DestBlend =
			D3D11_BLEND_INV_SRC_ALPHA;
		blend_description.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_description.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blend_description.RenderTarget[0].DestBlendAlpha =
			D3D11_BLEND_INV_SRC_ALPHA;
		blend_description.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_description.RenderTarget[0].RenderTargetWriteMask =
			D3D11_COLOR_WRITE_ENABLE_ALL;
		ThrowIfFailed(device->CreateBlendState(&blend_description,
			this->blend.ReleaseAndGetAddressOf()));

		// No depth, because there is no depth buffer to test against.
		D3D11_DEPTH_STENCIL_DESC depth_description = {};
		depth_description.DepthEnable = FALSE;
		depth_description.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depth_description.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depth_description.StencilEnable = FALSE;
		ThrowIfFailed(device->CreateDepthStencilState(&depth_description,
			this->depth.ReleaseAndGetAddressOf()));

		// NO CULLING, WHERE SpriteBatch CULLED BACK FACES. A sprite is a quad
		// whose winding never changes: a flip mirrors the texture coordinates
		// and leaves the corners where they were (sprite_geometry.cpp), so
		// there is no back face to find. Culling nothing is one fewer thing for
		// a second backend to match, and one fewer way for a quad to vanish.
		D3D11_RASTERIZER_DESC rasterizer_description = {};
		rasterizer_description.FillMode = D3D11_FILL_SOLID;
		rasterizer_description.CullMode = D3D11_CULL_NONE;
		rasterizer_description.DepthClipEnable = TRUE;
		rasterizer_description.MultisampleEnable = TRUE;
		ThrowIfFailed(device->CreateRasterizerState(&rasterizer_description,
			this->rasterizer.ReleaseAndGetAddressOf()));

		// Clamped, so a source rectangle at the edge of an atlas cannot bleed
		// the far side of it into a sprite - which is what a sheet is full of.
		D3D11_SAMPLER_DESC sampler_description = {};
		sampler_description.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_description.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_description.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_description.MaxAnisotropy = 1;
		sampler_description.ComparisonFunc = D3D11_COMPARISON_NEVER;

		// LEVEL ZERO, ALWAYS, and this line is the whole of the term on this
		// backend. It was D3D11_FLOAT32_MAX, which samples whatever chain the
		// texture carries; the GL backend's min filters are GL_NEAREST and
		// GL_LINEAR, which are the non-mipmap ones, so it has always answered
		// level zero. The two agreed only because no file in either client has
		// ever carried a second level - agreement by content, which stops the
		// first time somebody authors a chain and stops silently.
		//
		// The seam decides it this way round because the alternative hands a
		// which-texel decision to the rasteriser: a mip level is chosen per
		// pixel from screen-space derivatives, both APIs allow an
		// implementation to approximate that, and nothing this engine does
		// would decide it. renderer.h's closing section says NOTHING A BACKEND
		// DOES DECIDES WHERE A PIXEL GOES, and which texel is the same
		// sentence. Chains are still read and still uploaded; they are not
		// sampled from.
		sampler_description.MinLOD = 0.0f;
		sampler_description.MaxLOD = 0.0f;

		sampler_description.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		ThrowIfFailed(device->CreateSamplerState(&sampler_description,
			this->point_sampler.ReleaseAndGetAddressOf()));

		sampler_description.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		ThrowIfFailed(device->CreateSamplerState(&sampler_description,
			this->linear_sampler.ReleaseAndGetAddressOf()));

		// The index buffer, which is the same for every sprite there will ever
		// be: two triangles over four corners, immutable, filled once.
		std::vector<unsigned short> index_data;
		index_data.reserve(static_cast<size_t>(DrawList::View::
			MAX_BATCH_SPRITES) * INDICES_PER_SPRITE);
		for (int sprite = 0; sprite < DrawList::View::MAX_BATCH_SPRITES;
			sprite++)
		{
			const unsigned short first =
				static_cast<unsigned short>(sprite * VERTICES_PER_SPRITE);
			index_data.push_back(static_cast<unsigned short>(first + 0));
			index_data.push_back(static_cast<unsigned short>(first + 1));
			index_data.push_back(static_cast<unsigned short>(first + 2));
			index_data.push_back(static_cast<unsigned short>(first + 1));
			index_data.push_back(static_cast<unsigned short>(first + 3));
			index_data.push_back(static_cast<unsigned short>(first + 2));
		}

		D3D11_BUFFER_DESC index_description = {};
		index_description.ByteWidth = static_cast<UINT>(index_data.size() *
			sizeof(unsigned short));
		index_description.Usage = D3D11_USAGE_IMMUTABLE;
		index_description.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA index_initial = {};
		index_initial.pSysMem = index_data.data();
		ThrowIfFailed(device->CreateBuffer(&index_description, &index_initial,
			this->indices.ReleaseAndGetAddressOf()));

		// A context belongs to the device that made it, so both are remade
		// here together - and the View objects themselves are not, because a
		// DrawList a caller is holding must keep pointing at the same view.
		//
		// The context is created beside the buffers that record into it, which
		// is the whole point of it living here. DeviceResources used to own a
		// pool of them and this loop borrowed the i'th, so the ordering that
		// made that work - rebuild the pool after the device and before
		// OnDeviceRestored - was a rule spanning two classes with nowhere to
		// write it down. Now there is no ordering to state.
		//
		// ReleaseAndGetAddressOf, not GetAddressOf: this function can run
		// twice over the same views for one device loss, because
		// CreateWindowSizeDependentResources can re-enter HandleDeviceLost
		// whose inner OnDeviceRestored has already been through here. The
		// releasing form makes the second pass free the first pass's context
		// instead of leaking one per view.
		D3D11_BUFFER_DESC vertex_description = {};
		vertex_description.ByteWidth = static_cast<UINT>(
			static_cast<size_t>(DrawList::View::MAX_BATCH_SPRITES) *
			VERTICES_PER_SPRITE * sizeof(SpriteVertex));
		vertex_description.Usage = D3D11_USAGE_DYNAMIC;
		vertex_description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertex_description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		D3D11_BUFFER_DESC transform_description = {};
		transform_description.ByteWidth = sizeof(ViewportTransform);
		transform_description.Usage = D3D11_USAGE_DYNAMIC;
		transform_description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		transform_description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		for (std::unique_ptr<DrawList::View>& view_ptr : this->views)
		{
			DrawList::View& view = *view_ptr;
			view.owner = this;
			ThrowIfFailed(device->CreateDeferredContext(0,
				view.context.ReleaseAndGetAddressOf()));
			ThrowIfFailed(device->CreateBuffer(&vertex_description, nullptr,
				view.vertices.ReleaseAndGetAddressOf()));
			ThrowIfFailed(device->CreateBuffer(&transform_description, nullptr,
				view.transform.ReleaseAndGetAddressOf()));
			view.reset();
		}
	}

	void Renderer::Impl::OnDeviceLost()
	{
		for (std::unique_ptr<DrawList::View>& view : this->views)
		{
			view->vertices.Reset();
			view->transform.Reset();
			view->context.Reset();
			view->batch.clear();
			view->batch_texture = nullptr;
			view->buffer_position = 0;
			// The context that held the bind has gone with the device, so the
			// next one starts unbound. Without this, set_view_count would skip
			// binding a fresh context that has never been bound at all.
			view->bound = false;
		}

		this->indices.Reset();
		this->vertex_shader.Reset();
		this->pixel_shader.Reset();
		this->input_layout.Reset();
		this->blend.Reset();
		this->depth.Reset();
		this->rasterizer.Reset();
		this->point_sampler.Reset();
		this->linear_sampler.Reset();

		if (this->notify != nullptr)
		{
			this->notify->on_device_lost();
		}
	}

	void Renderer::Impl::OnDeviceRestored()
	{
		this->create_device_dependent_resources();

		if (this->notify != nullptr)
		{
			this->notify->on_device_restored();
		}
	}

	// --- Renderer ------------------------------------------------------------

	Renderer::Renderer() : impl_(std::make_unique<Impl>())
	{

	}

	Renderer::~Renderer() = default;
	Renderer::Renderer(Renderer&&) noexcept = default;
	Renderer& Renderer::operator=(Renderer&&) noexcept = default;

	void Renderer::create_device(void* native_window, int width, int height,
		int view_capacity)
	{
		if (view_capacity < 1)
		{
			throw std::invalid_argument(
				"Renderer::create_device needs a view capacity of at least 1.");
		}

		this->impl_->device_resources.SetWindow(
			static_cast<HWND>(native_window), width, height);
		this->impl_->device_resources.CreateDeviceResources();

		this->impl_->views.clear();
		this->impl_->views.reserve(static_cast<size_t>(view_capacity));
		for (int i = 0; i < view_capacity; i++)
		{
			this->impl_->views.push_back(std::make_unique<DrawList::View>());
		}

		this->impl_->create_device_dependent_resources();
		this->impl_->device_resources.CreateWindowSizeDependentResources();
	}

	bool Renderer::window_size_changed(int width, int height)
	{
		Impl& impl = *this->impl_;

		// BEFORE THERE IS A DEVICE THERE IS NOTHING TO REBUILD, and the seam
		// says the answer is false (renderer.h). Reached by a shell that gets a
		// WM_SIZE between creating its window and creating its device, which is
		// an ordering Win32 allows and nothing here forbids -
		// tests/render/renderer_seam_tests.cpp holds all five backends to it.
		if (impl.device_resources.GetD3DDevice() == nullptr)
		{
			return false;
		}

		// ASKED BEFORE ANYTHING IS THROWN AWAY, because most calls to this
		// change nothing and a frame is not worth losing to one of them.
		// Application::on_window_moved calls this with the size it already
		// has on every move of the window outside a drag; inside one, window.cpp
		// gates WM_MOVE the way it gates WM_SIZE, because that argument is a
		// swap chain's size here and the live client rect on the GL backend.
		// The comparison is the one WindowSizeChanged makes itself; it is
		// repeated here rather than added to that file, which is Microsoft's
		// and carried with its own naming (NOTICE).
		const RECT size = impl.device_resources.GetOutputSize();
		if (static_cast<long>(width) == size.right - size.left &&
			static_cast<long>(height) == size.bottom - size.top)
		{
			return false;
		}

		// A FRAME IN PROGRESS IS RESTARTED, NOT REFUSED (renderer.h), and this
		// backend used to be the one that refused. Every bound view holds this
		// frame's render target view in its deferred context, and DXGI will
		// not resize a swap chain while anything still references its buffers:
		// ResizeBuffers answered DXGI_ERROR_INVALID_CALL and the throw came out
		// of a window message, where the shell has nowhere to catch it. So the
		// recordings go first, which is also what releases the reference.
		//
		// discard() rather than finish(): what these contexts hold was drawn
		// against a buffer that is about to stop existing, so there is nothing
		// worth executing and no command list worth owning.
		//
		// AND WHETHER THERE IS A FRAME IS A THING THE FRAME SAYS, not a thing
		// the views are asked. This used to be `restart || view.bound`, and no
		// view is bound between begin_frame and the first set_view_count - so
		// a resize arriving there rebuilt the buffer and then left it to be
		// drawn into without the clear below. Impl::frame_begun is the
		// interval renderer.h actually names.
		const bool restart = impl.frame_begun;

		const int capacity = static_cast<int>(impl.views.size());
		for (int i = 0; i < capacity; i++)
		{
			DrawList::View& view = *impl.views[static_cast<size_t>(i)];
			view.discard();
			view.reset();
		}

		const bool rebuilt =
			impl.device_resources.WindowSizeChanged(width, height);

		if (rebuilt && restart)
		{
			// Cleared and rebound against the buffer that now exists, so a
			// DrawList the caller is still holding draws into this frame
			// instead of into a view with no target bound at all.
			DeviceResources& device = impl.device_resources;
			ID3D11DeviceContext1* context = device.GetD3DDeviceContext();
			ID3D11RenderTargetView* render_target =
				device.GetRenderTargetView();
			const D3D11_VIEWPORT viewport = device.GetScreenViewport();

			const float BLACK[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			context->ClearRenderTargetView(render_target, BLACK);
			context->OMSetRenderTargets(1, &render_target, nullptr);
			context->RSSetViewports(1, &viewport);

			// None of them when the frame has not said how many it has yet,
			// which leaves the clear above as the whole of the restart - and
			// that is exactly what the frame is owed at that point.
			for (int i = 0; i < impl.view_count; i++)
			{
				impl.views[static_cast<size_t>(i)]->bind(render_target,
					viewport);
			}
		}

		return rebuilt;
	}

	void Renderer::set_device_notify(DeviceNotify* device_notify)
	{
		this->impl_->notify = device_notify;
	}

	void Renderer::set_resources(const RenderResources* resources)
	{
		this->impl_->resources = resources;
	}

	void Renderer::begin_frame()
	{
		DeviceResources& device = this->impl_->device_resources;

		ID3D11DeviceContext1* context = device.GetD3DDeviceContext();
		ID3D11RenderTargetView* render_target = device.GetRenderTargetView();
		const D3D11_VIEWPORT viewport = device.GetScreenViewport();

		// Opaque black, spelt out. It was DirectX::Colors::Black, which is the
		// last thing <DirectXColors.h> was in this file for - and the clear
		// colour is a term RenderPixelTests pins, so it is worth being able to
		// read it here rather than in a table of a hundred and forty names.
		const float BLACK[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		context->ClearRenderTargetView(render_target, BLACK);
		context->OMSetRenderTargets(1, &render_target, nullptr);
		context->RSSetViewports(1, &viewport);

		// Every view records into its own deferred context, so each needs the
		// same target and viewport bound before it is drawn into - and needs it
		// again every frame, because submit() finishes each command list without
		// restoring the context state.
		//
		// THE BINDING IS NOT DONE HERE, and that is the fix rather than an
		// optimisation. Binding is two commands recorded into a deferred
		// context, and a deferred context holds what is recorded into it until
		// FinishCommandList takes it away - so binding every view in the
		// capacity while submit() finishes only the views the frame declared
		// stranded two commands per idle view per frame, permanently. Nothing
		// below this line binds anything; set_view_count binds the views the
		// frame actually has, which is the first moment anybody knows what they
		// are.
		//
		// IT DOES DRAIN, THOUGH, AND ONLY THIS BACKEND HAS ANYTHING TO DRAIN.
		// "begin_frame resets every view's recording" (renderer.h) has to mean
		// the same thing on all five, and on three of the other four a recording
		// is a vector that reset() empties. Here it is a deferred context, and the
		// commands in one outlive any flag this loop clears: a frame begun,
		// drawn into and never submitted used to leave its flushed geometry
		// where the next frame's ExecuteCommandList would find it at the head
		// of the list, drawn over the top of the clear above. discard() is a
		// no-op on every view submit() already finished, which is every view on
		// every normal frame.
		for (std::unique_ptr<DrawList::View>& view : this->impl_->views)
		{
			view->discard();
			view->reset();
		}

		this->impl_->view_count = 0;

		// AND THE FRAME IS OPEN FROM HERE UNTIL end_frame, which is what a
		// resize arriving before the first set_view_count needs to be told.
		// The views cannot say it - none of them is bound yet - and the clear
		// at the top of this function is already this frame's. See
		// Renderer::window_size_changed.
		this->impl_->frame_begun = true;
		this->impl_->frame_submitted = false;
	}

	void Renderer::end_frame()
	{
		// Cleared before the present rather than after it, because Present is
		// where a device loss surfaces and what it does about one is rebuild
		// every resource this frame was drawn with. There is nothing left to
		// restart by then.
		this->impl_->frame_begun = false;

		this->impl_->device_resources.Present();
	}

	void Renderer::set_view_count(int count)
	{
		const int capacity = static_cast<int>(this->impl_->views.size());
		if (count < 0 || count > capacity)
		{
			throw std::out_of_range("Renderer::set_view_count(" +
				std::to_string(count) + ") is outside the capacity of " +
				std::to_string(capacity) + " create_device was given.");
		}

		// Lowering it past a view something has already drawn into strands
		// that recording: submit() would not execute it, and the commands
		// would surface at the top of the next frame's list instead. Say so
		// here rather than let it turn up as one pane's worth of last frame.
		for (int i = count; i < this->impl_->view_count; i++)
		{
			if (this->impl_->views[static_cast<size_t>(i)]->touched)
			{
				throw std::logic_error("Renderer::set_view_count(" +
					std::to_string(count) + ") would drop view " +
					std::to_string(i) + ", which has already been drawn into "
					"this frame.");
			}
		}

		this->impl_->view_count = count;

		// Bind what the frame is going to draw into, and nothing else. Views
		// already bound this frame are left alone, so declaring the count twice
		// is free and does not overwrite a viewport set_viewport has since
		// chosen. See DrawList::View::bound.
		DeviceResources& device = this->impl_->device_resources;
		ID3D11RenderTargetView* render_target = device.GetRenderTargetView();
		const D3D11_VIEWPORT viewport = device.GetScreenViewport();

		for (int i = 0; i < count; i++)
		{
			this->impl_->views[static_cast<size_t>(i)]->bind(render_target,
				viewport);
		}
	}

	int Renderer::view_count() const
	{
		return this->impl_->view_count;
	}

	DrawList Renderer::view(int index) const
	{
		if (index < 0 || index >= this->impl_->view_count)
		{
			throw std::out_of_range("Renderer::view(" + std::to_string(index) +
				") - this frame has " + std::to_string(this->impl_->view_count) +
				" views. Asking for one nobody set up used to answer with a "
				"fullscreen pane and draw a whole extra pass.");
		}
		return DrawList(this->impl_->views[static_cast<size_t>(index)].get());
	}

	void Renderer::submit()
	{
		// ONCE PER FRAME, AND A SECOND CALL ADDS NOTHING (renderer.h). Cleared
		// by begin_frame, which is the only thing that starts a frame.
		if (this->impl_->frame_submitted)
		{
			return;
		}
		this->impl_->frame_submitted = true;

		ID3D11DeviceContext1* immediate =
			this->impl_->device_resources.GetD3DDeviceContext();

		// Record, finish, execute, release - the protocol that was hand-written
		// in four places, each of which had to pre-size a vector, pre-fill it
		// with null and Release every non-null entry. The lists are executed in
		// view order, which is the only ordering guarantee the seam makes.
		//
		// EVERY BOUND VIEW IS FINISHED; ONLY THE DECLARED ONES ARE EXECUTED,
		// and the two sets are not always the same one. Lowering the view count
		// past a view that was bound and not drawn into is legal - set_view_count
		// rejects only the ones that were drawn into - and that view's context
		// is holding the bind. Finishing without executing is how a context is
		// drained; skipping it is how commands accumulate for the life of the
		// process.
		const int capacity = static_cast<int>(this->impl_->views.size());
		for (int i = 0; i < capacity; i++)
		{
			DrawList::View& view = *this->impl_->views[static_cast<size_t>(i)];
			if (!view.bound)
			{
				continue;
			}

			ID3D11CommandList* commands = view.finish();
			if (i < this->impl_->view_count)
			{
				immediate->ExecuteCommandList(commands, FALSE);
			}
			commands->Release();
		}
	}

	Vector2F Renderer::back_buffer_size() const
	{
		const RECT size = this->impl_->device_resources.GetOutputSize();
		return { static_cast<float>(size.right - size.left),
			static_cast<float>(size.bottom - size.top) };
	}

	void Renderer::read_back_buffer(std::vector<unsigned char>& pixels)
	{
		DeviceResources& device = this->impl_->device_resources;

		// Through the view rather than a GetRenderTarget() accessor, because
		// the view is what the seam already needs and one reader does not
		// justify widening the wrapper's surface again.
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		device.GetRenderTargetView()->GetResource(resource.GetAddressOf());

		Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
		ThrowIfFailed(resource.As(&back_buffer));

		// A staging copy, because the back buffer is not CPU-readable. Made per
		// call and thrown away: this is not a frame-path function and a cached
		// staging texture would be one more thing to remake on a device loss.
		D3D11_TEXTURE2D_DESC description = {};
		back_buffer->GetDesc(&description);
		description.Usage = D3D11_USAGE_STAGING;
		description.BindFlags = 0;
		description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		description.MiscFlags = 0;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
		ThrowIfFailed(device.GetD3DDevice()->CreateTexture2D(&description,
			nullptr, staging.ReleaseAndGetAddressOf()));

		ID3D11DeviceContext1* context = device.GetD3DDeviceContext();
		context->CopyResource(staging.Get(), back_buffer.Get());

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		ThrowIfFailed(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0,
			&mapped));

		const size_t width = static_cast<size_t>(description.Width);
		const size_t height = static_cast<size_t>(description.Height);
		pixels.resize(width * height * 4);

		// Row pitch is the driver's, not width * 4, so the rows are walked
		// rather than the buffer memcpy'd. B and R swap on the way out: the
		// back buffer is B8G8R8A8 and the seam promises RGBA.
		const unsigned char* source =
			static_cast<const unsigned char*>(mapped.pData);
		for (size_t y = 0; y < height; y++)
		{
			const unsigned char* row = source + y * mapped.RowPitch;
			for (size_t x = 0; x < width; x++)
			{
				unsigned char* out = pixels.data() + (y * width + x) * 4;
				out[0] = row[x * 4 + 2];
				out[1] = row[x * 4 + 1];
				out[2] = row[x * 4 + 0];
				out[3] = row[x * 4 + 3];
			}
		}

		context->Unmap(staging.Get(), 0);
	}

	// THE ONE BACKEND THAT FORWARDS THEM, and therefore the one that has
	// anything to be null. ID3DUserDefinedAnnotation is made with the device,
	// so a marker before create_device dereferenced a null ComPtr here while
	// the other four discarded the call harmlessly - a divergence a caller
	// could only find by crashing. renderer.h makes markers advisory and legal
	// before there is a device; this is the line that makes that true, and
	// tests/render/renderer_seam_tests.cpp is what holds all five to it.
	void Renderer::begin_marker(const wchar_t* name)
	{
		if (this->impl_->device_resources.GetD3DDevice() == nullptr)
		{
			return;
		}
		this->impl_->device_resources.PIXBeginEvent(name);
	}

	void Renderer::end_marker()
	{
		if (this->impl_->device_resources.GetD3DDevice() == nullptr)
		{
			return;
		}
		this->impl_->device_resources.PIXEndEvent();
	}

	void Renderer::set_marker(const wchar_t* name)
	{
		if (this->impl_->device_resources.GetD3DDevice() == nullptr)
		{
			return;
		}
		this->impl_->device_resources.PIXSetMarker(name);
	}
}
