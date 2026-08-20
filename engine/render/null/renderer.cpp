#include "engine/render/null/backend.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/sprite_geometry.h"
#include "engine/render/sprite_vertex.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace mattmath;

namespace labrador
{
	// --- DrawList::View ------------------------------------------------------

	void DrawList::View::draw(TextureHandle texture,
		const SpriteVertex* corners)
	{
		RecordedSprite sprite;
		sprite.texture = texture;
		sprite.filter = this->filter;
		sprite.viewport = this->viewport;
		for (int i = 0; i < 4; i++)
		{
			sprite.corners[i] = corners[i];
		}

		// NO BATCHING, AND NOTHING TO BATCH FOR. The other two backends group a
		// run of sprites sharing a texture into one draw call because a draw
		// call costs something. Nothing here costs anything, and a test asking
		// "what was drawn" wants the sprites it asked for rather than the runs
		// a driver would have preferred.
		this->sprites.push_back(sprite);
		this->touched = true;
	}

	void DrawList::View::reset()
	{
		this->camera = Camera::DEFAULT_CAMERA;
		this->filter = TextureFilter::point;
		this->sprites.clear();
		this->touched = false;
	}

	// --- DrawList ------------------------------------------------------------

	void DrawList::set_viewport(const Viewport& viewport)
	{
		this->view_->viewport = viewport;
	}

	void DrawList::set_camera(const Camera& camera)
	{
		this->view_->camera = camera;
	}

	void DrawList::set_filter(TextureFilter filter)
	{
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
		std::ignore = layer_depth;

		const NullTexture& null_texture =
			*this->view_->owner->resources->impl()->texture(texture);

		// THE SAME GEOMETRY THE REAL BACKENDS RUN, which is what makes a
		// recording worth asserting on. If this backend built its own idea of
		// where a sprite goes, a test passing here would say nothing about what
		// a device would have drawn.
		SpriteVertex corners[4];
		build_sprite_quad(
			this->view_->camera.calculate_view_rectangle(destination),
			source, null_texture.size(), tint, rotation, origin, flip,
			corners);

		this->view_->draw(texture, corners);
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

		// The font table is the seam's own and the texture table is this
		// folder's, which is the rule about which half lives where reading as
		// two lines of code (engine/render/render_resources.h).
		const RenderResources& resources = *this->view_->owner->resources;
		const Font& the_font = *resources.font(font);
		const TextureHandle atlas = the_font.atlas();
		const Vector2F atlas_size = resources.impl()->texture(atlas)->size();

		const Vector2F screen_position =
			this->view_->camera.calculate_view_position(position);
		const float screen_scale =
			this->view_->camera.calculate_view_scale(scale);

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
		// The window is ignored rather than rejected when it is null. A test
		// hands nullptr and a shell hands a real HWND, and neither is wrong:
		// this backend has nothing to present to either way, and refusing the
		// second would mean a client could not run against it at all.
		std::ignore = native_window;

		if (view_capacity < 1)
		{
			throw std::invalid_argument(
				"Renderer::create_device needs a view capacity of at least 1.");
		}

		this->impl_->device_created = true;
		this->impl_->width = width;
		this->impl_->height = height;

		this->impl_->views.clear();
		this->impl_->views.reserve(static_cast<size_t>(view_capacity));
		for (int i = 0; i < view_capacity; i++)
		{
			this->impl_->views.push_back(std::make_unique<DrawList::View>());
			this->impl_->views.back()->owner = this->impl_.get();
			this->impl_->views.back()->reset();
		}
	}

	bool Renderer::window_size_changed(int width, int height)
	{
		if (width == this->impl_->width && height == this->impl_->height)
		{
			return false;
		}
		this->impl_->width = width;
		this->impl_->height = height;

		// AND THE FRAME IN PROGRESS IS RESTARTED, WHICH THIS BACKEND NEEDS
		// EVEN LESS THAN THE OPENGL ONE AND DOES ANYWAY.
		//
		// renderer.h makes it a term of the seam that a resize arriving
		// mid-frame drops what every view has recorded and reopens them at the
		// new size. On the two Direct3D backends that is a correctness fix:
		// their recordings name a render target the resize destroys. Here
		// there is no target and no buffer - a recording is a vector of
		// structs - so this backend could keep every sprite it has been given
		// and hand them all back.
		//
		// It does not, for the reason gl/renderer.cpp already argues at
		// length: the difference would be observable. A caller that draws, is
		// resized under it and submits would see its drawing survive here and
		// vanish on the other three, from one call, with nothing in the seam
		// to say which to expect - and this is the configuration a client is
		// most likely to be tested in, because it is the one that needs no
		// driver. The most permissive backend teaching a rule that does not
		// hold anywhere else is worse than a divergence a device would catch.
		//
		// view->reset() is also what clears `touched`, which is not a
		// bookkeeping detail here: without it a shell doing exactly what the
		// return value told it to - re-running its layout mid-frame, from two
		// views to one - got std::logic_error out of set_view_count under this
		// backend and silence under the other three.
		//
		// WHAT IS NOT TOUCHED: view_count, because renderer.h says the layout
		// is the shell's to decide and this call is what tells it to decide
		// again; and impl_->recorded, because submit() rebuilds it wholesale
		// and end_frame promises the last recording stays readable until the
		// next begin_frame.
		for (std::unique_ptr<DrawList::View>& view : this->impl_->views)
		{
			view->reset();
			view->viewport = Viewport(0.0f, 0.0f,
				static_cast<float>(this->impl_->width),
				static_cast<float>(this->impl_->height));
		}

		return true;
	}

	void Renderer::set_device_notify(DeviceNotify* device_notify)
	{
		// Stored and never called. Nothing here can be lost.
		this->impl_->notify = device_notify;
	}

	void Renderer::set_resources(const RenderResources* resources)
	{
		this->impl_->resources = resources;
	}

	void Renderer::begin_frame()
	{
		this->impl_->recorded.clear();

		for (std::unique_ptr<DrawList::View>& view : this->impl_->views)
		{
			view->reset();
			view->viewport = Viewport(0.0f, 0.0f,
				static_cast<float>(this->impl_->width),
				static_cast<float>(this->impl_->height));
		}

		this->impl_->view_count = 0;
	}

	void Renderer::end_frame()
	{
		// Nothing to present. The recording stays until the next begin_frame,
		// so a caller that reads it after this rather than before gets the same
		// answer - which the other two backends cannot offer, because
		// presenting discards what they drew.
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

		// The same throw the other two make, and it costs this backend nothing
		// to be strict about - which is the point of testing a client here. A
		// caller that drops a drawn-into view fails in every configuration or
		// in none.
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
		// In view order, which is the only ordering guarantee the seam makes.
		// Gathering here rather than in draw() is what makes the recording that
		// order rather than the order several workers happened to finish in.
		this->impl_->recorded.clear();
		for (int i = 0; i < this->impl_->view_count; i++)
		{
			DrawList::View& view = *this->impl_->views[static_cast<size_t>(i)];
			for (RecordedSprite& sprite : view.sprites)
			{
				sprite.view = i;
				this->impl_->recorded.push_back(sprite);
			}
		}
	}

	Vector2F Renderer::back_buffer_size() const
	{
		return { static_cast<float>(this->impl_->width),
			static_cast<float>(this->impl_->height) };
	}

	void Renderer::read_back_buffer(std::vector<unsigned char>& pixels)
	{
		std::ignore = pixels;

		// THE ONE THING THIS BACKEND CANNOT DO, and it says so rather than
		// answering with a black rectangle. Rasterising would mean a third
		// implementation of the pixel contract plus a BC decoder plus a fill
		// rule that agreed with two hardware ones exactly - a large thing to
		// build and a larger one to be quietly wrong about. recording.h says
		// what to read instead.
		throw std::logic_error("The null backend records what it was asked to "
			"draw and never draws it, so there is no back buffer to read. Use "
			"recorded_sprites() from engine/render/null/recording.h, or build "
			"a configuration with a real backend for RenderPixelTests.");
	}

	// Nothing to mark, and nothing listening.
	void Renderer::begin_marker(const wchar_t* name) { std::ignore = name; }
	void Renderer::end_marker() { }
	void Renderer::set_marker(const wchar_t* name) { std::ignore = name; }

	// --- recording.h ---------------------------------------------------------

	const std::vector<RecordedSprite>& recorded_sprites(
		const Renderer& renderer)
	{
		return renderer.impl()->recorded;
	}
}
