#pragma once

#include "engine/math/rectanglei.h"
#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"

#include <vector>

// Shared fixture for the null renderer's cases; state belongs to each Harness.
namespace render_tests::null_backend
{
	// A renderer, a table, and content loaded through the real load path.
	//
	// THE LOADERS ARE NOT STUBBED. load_texture_asset opens ./content/quad.dds,
	// parses it and rejects it if it is wrong; only the last step - the bytes
	// becoming a device texture - is this backend's, and it keeps the size.
	// So a test here exercises the same file reading a client does.
	class Harness
	{
	public:
		Harness()
		{
			// No window. The null backend takes one and ignores it, which is
			// the whole reason this can run on a machine with no display.
			this->renderer_.create_device(nullptr, 640, 480, 4);
			this->renderer_.set_resources(&this->resources_);

			labrador::load_texture_asset(this->renderer_, this->resources_,
				"./content/", "quad");
			this->quad = this->resources_.resolve_texture("quad");

			labrador::load_font_asset(this->renderer_, this->resources_,
				"./content/", "jetbrains_mono_bold_16");
			this->font = this->resources_.resolve_sprite_font(
				"jetbrains_mono_bold_16");
		}

		labrador::DrawList begin(int views = 1)
		{
			this->renderer_.begin_frame();
			this->renderer_.set_view_count(views);
			return this->renderer_.view(0);
		}

		labrador::DrawList view(int index)
		{
			return this->renderer_.view(index);
		}

		// A COPY, WHERE THE ENGINE HANDS BACK A REFERENCE. recording.h says the
		// recording is valid until the next begin_frame, which is the right
		// contract for a frame path and the wrong one for a test that wants to
		// compare two frames - the second begin_frame clears the vector the
		// first frame's reference is still pointing at, and every assertion
		// against it silently starts describing the second frame.
		std::vector<labrador::RecordedSprite> end()
		{
			this->renderer_.submit();
			return labrador::recorded_sprites(this->renderer_);
		}

		labrador::Renderer& renderer() { return this->renderer_; }
		labrador::RenderResources& resources() { return this->resources_; }

		static labrador::RectangleI whole()
		{
			return labrador::RectangleI(0, 0, 2, 2);
		}

		labrador::TextureHandle quad;
		labrador::FontHandle font;

	private:
		// The table before the renderer, so it dies after one. It costs this
		// backend nothing - a null texture is two ints - and the order is kept
		// anyway, because render_resources.h states it as a term of the seam
		// and a harness that keeps it only where it is expensive is a harness
		// that teaches the rule wrong.
		labrador::RenderResources resources_;
		labrador::Renderer renderer_;
	};
}
