#include "engine/render/d3d11/backend.h"

#include "engine/core/throw_if_failed.h"

#include <DirectXColors.h>

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

	void DrawList::View::reset()
	{
		this->camera = Camera::DEFAULT_CAMERA;
		this->filter = TextureFilter::point;
		this->batch_open = false;
		this->touched = false;
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

		// Wide in, wide all the way down. DirectXTK's narrow DrawString
		// converts through a utfBuffer owned by the shared SpriteFont, from a
		// const method, under a fan-out where every worker draws the whole HUD.
		this->view_->owner->resources->impl()->sprite_font(font)->DrawString(
			this->view_->batch.get(),
			text.c_str(),
			to_xm(this->view_->camera.calculate_view_position(position)),
			to_xm(tint),
			rotation,
			to_xm(origin),
			this->view_->camera.calculate_view_scale(scale),
			SpriteEffects_None,
			layer_depth);
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

		// The contexts belong to the device and were remade with it, so the
		// batches that write into them are remade too - and the View objects
		// themselves are not, because a DrawList a caller is holding must keep
		// pointing at the same view.
		for (size_t i = 0; i < this->views.size(); i++)
		{
			DrawList::View& view = *this->views[i];
			view.owner = this;
			view.context =
				this->device_resources.deferred_context(static_cast<int>(i));
			view.batch = std::make_unique<SpriteBatch>(view.context);
			view.reset();
		}
	}

	void Renderer::Impl::OnDeviceLost()
	{
		for (std::unique_ptr<DrawList::View>& view : this->views)
		{
			view->batch.reset();
			view->context = nullptr;
			view->batch_open = false;
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
		this->impl_->device_resources.create_deferred_contexts(view_capacity);

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
		// same target and viewport bound before the frame fans out - and needs
		// it again every frame, because submit() finishes each command list
		// without restoring the context state.
		for (std::unique_ptr<DrawList::View>& view : this->impl_->views)
		{
			view->context->OMSetRenderTargets(1, &render_target, nullptr);
			view->context->RSSetViewports(1, &viewport);
			view->batch->SetViewport(viewport);
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
		for (int i = 0; i < this->impl_->view_count; i++)
		{
			ID3D11CommandList* commands =
				this->impl_->views[static_cast<size_t>(i)]->finish();
			immediate->ExecuteCommandList(commands, FALSE);
			commands->Release();
		}
	}

	Vector2F Renderer::back_buffer_size() const
	{
		const RECT size = this->impl_->device_resources.GetOutputSize();
		return { static_cast<float>(size.right - size.left),
			static_cast<float>(size.bottom - size.top) };
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
