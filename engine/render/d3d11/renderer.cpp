#include "engine/render/d3d11/backend.h"

#include "engine/core/throw_if_failed.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

#include <DirectXColors.h>
#include <memory>
#include <stdexcept>
#include <string>

using namespace DirectX;
using namespace mattmath;

namespace artattack
{
	namespace
	{
		// Where the engine's values become the backend's. Every one of these
		// used to be a member of the mattmath type - Viewport::d3d_viewport(),
		// RectangleF::win_rect(), Colour::xm_vector() - which is what put
		// <d3d11.h> and SimpleMath.h in a library documented as depending on
		// nothing. This file was their only caller, so this is where they go.
		// A second backend writes its own four; it does not inherit these.
		D3D11_VIEWPORT to_d3d_viewport(const Viewport& viewport)
		{
			return { viewport.x, viewport.y, viewport.width, viewport.height,
				viewport.minDepth, viewport.maxDepth };
		}

		RECT to_rect(const RectangleF& rectangle)
		{
			return { static_cast<long>(rectangle.left()),
				static_cast<long>(rectangle.top()),
				static_cast<long>(rectangle.right()),
				static_cast<long>(rectangle.bottom()) };
		}

		RECT to_rect(const RectangleI& rectangle)
		{
			return { rectangle.left(), rectangle.top(),
				rectangle.right(), rectangle.bottom() };
		}

		XMFLOAT2 to_xm(const Vector2F& vector)
		{
			return { vector.x, vector.y };
		}

		XMVECTOR to_xm(const Colour& colour)
		{
			return XMVectorSet(colour.r, colour.g, colour.b, colour.a);
		}

		SpriteEffects to_sprite_effects(SpriteFlip flip)
		{
			switch (flip)
			{
			case SpriteFlip::horizontal: return SpriteEffects_FlipHorizontally;
			case SpriteFlip::vertical:   return SpriteEffects_FlipVertically;
			case SpriteFlip::both:       return SpriteEffects_FlipBoth;
			case SpriteFlip::none:
			default:                     return SpriteEffects_None;
			}
		}
	}

	// --- DrawList::View ------------------------------------------------------

	// Opening is deferred to the first draw, so a view the scene declared and
	// then drew nothing into never opens a batch at all - which is what makes
	// declaring the capacity up front cheap.
	void DrawList::View::open_batch()
	{
		if (this->batch_open)
		{
			return;
		}
		this->batch->Begin(SpriteSortMode_Deferred, nullptr,
			this->owner->sampler(this->filter));
		this->batch_open = true;
		this->touched = true;
	}

	void DrawList::View::close_batch()
	{
		if (!this->batch_open)
		{
			return;
		}
		this->batch->End();
		this->batch_open = false;
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
		this->batch->SetViewport(viewport);
		this->bound = true;
	}

	void DrawList::View::reset()
	{
		this->camera = Camera::DEFAULT_CAMERA;
		this->filter = TextureFilter::point;
		this->batch_open = false;
		this->touched = false;
		this->bound = false;
	}

	ID3D11CommandList* DrawList::View::finish()
	{
		this->close_batch();

		ID3D11CommandList* commands = nullptr;
		// FALSE: the deferred context's state is not restored afterwards,
		// because begin_frame rebinds the render target and the viewport on
		// every context at the top of the next frame. The two hand-written
		// copies of this protocol disagreed about this flag - TRUE in Level,
		// FALSE in MenuPage - and neither had a reason written down.
		ThrowIfFailed(this->context->FinishCommandList(FALSE, &commands));
		return commands;
	}

	// --- DrawList ------------------------------------------------------------

	void DrawList::set_viewport(const Viewport& viewport)
	{
		// Both, and in this order. RSSetViewports is what the rasteriser
		// clips against; SetViewport is what SpriteBatch builds its projection
		// from, and on a deferred context it cannot read the first back.
		this->view_->close_batch();

		const D3D11_VIEWPORT d3d_viewport = to_d3d_viewport(viewport);
		this->view_->context->RSSetViewports(1, &d3d_viewport);
		this->view_->batch->SetViewport(d3d_viewport);
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
		this->view_->close_batch();
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
		this->view_->open_batch();

		const RECT source_rect = to_rect(source);
		const RECT destination_rect =
			to_rect(this->view_->camera.calculate_view_rectangle(destination));

		this->view_->batch->Draw(
			this->view_->owner->resources->impl()->texture(texture),
			destination_rect,
			&source_rect,
			to_xm(tint),
			rotation,
			to_xm(origin),
			to_sprite_effects(flip),
			layer_depth);
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
		this->view_->open_batch();

		const RenderResources::Impl& resources =
			*this->view_->owner->resources->impl();
		const Font& the_font = *resources.font(font);

		// A GLYPH IS A SPRITE, AND THAT IS THE WHOLE OF THIS FUNCTION. What
		// used to be here was SpriteFont::DrawString, which walked its own
		// glyph table and made exactly these calls. The walk is the engine's
		// now (font.h) and what is left below is the part of it that names a
		// batch - which is what a second backend has to write, and all it has
		// to write, to draw text.
		ID3D11ShaderResourceView* atlas = resources.texture(the_font.atlas());

		const XMFLOAT2 screen_position = to_xm(
			this->view_->camera.calculate_view_position(position));
		const XMVECTOR colour = to_xm(tint);
		const float screen_scale =
			this->view_->camera.calculate_view_scale(scale);

		// THE POSITION FORM, NOT THE RECTANGLE FORM draw_sprite uses. A
		// destination rectangle is truncated to whole pixels - RenderPixelTests
		// pins that - so a line of text laid out through one would jitter
		// against its own advance, which is fractional in most fonts. Here the
		// pen offset rides in the origin, in unscaled source texels, which is
		// where the seam already says an origin is measured.
		SpriteBatch& batch = *this->view_->batch;
		the_font.for_each_glyph(text,
			[&](const Glyph& glyph, const Vector2F& pen)
			{
				const RECT subrect = to_rect(glyph.subrect);
				const XMFLOAT2 glyph_origin{ origin.x - pen.x,
					origin.y - (pen.y + glyph.y_offset) };

				batch.Draw(atlas, screen_position, &subrect, colour, rotation,
					glyph_origin, screen_scale, SpriteEffects_None,
					layer_depth);
			});
	}

	// --- Renderer::Impl ------------------------------------------------------

	Renderer::Impl::Impl()
	{
		this->device_resources.RegisterDeviceNotify(this);
	}

	ID3D11SamplerState* Renderer::Impl::sampler(TextureFilter filter) const
	{
		// Read out of the CommonStates every time rather than cached at
		// construction. Two objects used to cache PointClamp() as a raw
		// ID3D11SamplerState* and hold it across a device loss, which frees the
		// CommonStates that owns it. The states object is remade with the
		// device; asking it per batch is a member read.
		return filter == TextureFilter::linear
			? this->states->LinearClamp()
			: this->states->PointClamp();
	}

	void Renderer::Impl::create_device_dependent_resources()
	{
		ID3D11Device1* device = this->device_resources.GetD3DDevice();
		this->states = std::make_unique<CommonStates>(device);

		// A context belongs to the device that made it, so both are remade
		// here together - and the View objects themselves are not, because a
		// DrawList a caller is holding must keep pointing at the same view.
		//
		// The context is created two lines above the SpriteBatch that records
		// into it, which is the whole point of it living here. DeviceResources
		// used to own a pool of them and this loop borrowed the i'th, so the
		// ordering that made that work - rebuild the pool after the device and
		// before OnDeviceRestored - was a rule spanning two classes with
		// nowhere to write it down. Now there is no ordering to state.
		//
		// ReleaseAndGetAddressOf, not GetAddressOf: this function can run
		// twice over the same views for one device loss, because
		// CreateWindowSizeDependentResources can re-enter HandleDeviceLost
		// whose inner OnDeviceRestored has already been through here. The
		// releasing form makes the second pass free the first pass's context
		// instead of leaking one per view.
		for (std::unique_ptr<DrawList::View>& view_ptr : this->views)
		{
			DrawList::View& view = *view_ptr;
			view.owner = this;
			ThrowIfFailed(device->CreateDeferredContext(0,
				view.context.ReleaseAndGetAddressOf()));
			view.batch = std::make_unique<SpriteBatch>(view.context.Get());
			view.reset();
		}
	}

	void Renderer::Impl::OnDeviceLost()
	{
		for (std::unique_ptr<DrawList::View>& view : this->views)
		{
			view->batch.reset();
			view->context.Reset();
			view->batch_open = false;
			// The context that held the bind has gone with the device, so the
			// next one starts unbound. Without this, set_view_count would skip
			// binding a fresh context that has never been bound at all.
			view->bound = false;
		}
		this->states.reset();

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
		return this->impl_->device_resources.WindowSizeChanged(width, height);
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

		context->ClearRenderTargetView(render_target, Colors::Black);
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
		// below this line touches a context; set_view_count binds the views the
		// frame actually has, which is the first moment anybody knows what they
		// are.
		for (std::unique_ptr<DrawList::View>& view : this->impl_->views)
		{
			view->reset();
		}

		this->impl_->view_count = 0;
	}

	void Renderer::end_frame()
	{
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

	void Renderer::begin_marker(const wchar_t* name)
	{
		this->impl_->device_resources.PIXBeginEvent(name);
	}

	void Renderer::end_marker()
	{
		this->impl_->device_resources.PIXEndEvent();
	}

	void Renderer::set_marker(const wchar_t* name)
	{
		this->impl_->device_resources.PIXSetMarker(name);
	}
}
