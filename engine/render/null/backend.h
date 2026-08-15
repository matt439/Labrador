#pragma once

#include "engine/core/registry.h"
#include "engine/render/font.h"
#include "engine/render/null/recording.h"
#include "engine/render/renderer.h"
#include "engine/render/render_resources.h"
#include "engine/render/sprite_vertex.h"

#include <memory>
#include <string>
#include <vector>
#include "engine/render/camera.h"

// The null backend: everything the seam requires, and no graphics API at all.
//
// ITS ONLY CLIENT IS engine/render/null/texture_factory.cpp, like every other
// backend's header. What a test includes is recording.h beside this, which
// names no backend type - that file explains the difference.
//
// THERE IS NO DEVICE, WHICH IS THE ENTIRE POINT. create_device takes a window
// handle and ignores it; nothing here calls an API that could fail, so this
// backend runs on a build machine, in a container, and on a runner with no
// display. That is what makes it the configuration CI can run completely.

namespace labrador
{
	// A texture that is a size and nothing else.
	//
	// The size is not decoration: engine/render/sprite_geometry.h needs it
	// every draw, to turn a source rectangle in texels into texture
	// coordinates. So a null texture has to remember what a real one would have
	// been, and a test asserting where a sprite sampled from is asserting
	// against a number that came out of the file.
	class NullTexture
	{
	public:
		NullTexture(int width, int height) : width_(width), height_(height) {}

		mattmath::Vector2F size() const
		{
			return mattmath::Vector2F(static_cast<float>(this->width_),
				static_cast<float>(this->height_));
		}

	private:
		int width_ = 0;
		int height_ = 0;
	};

	class RenderResources::Impl
	{
	public:
		void add_texture(const std::string& name,
			std::unique_ptr<NullTexture> texture);
		void add_font(const std::string& name, std::unique_ptr<Font> font);
		void add_sprite_sheet(const std::string& name,
			std::unique_ptr<SpriteSheet> sprite_sheet);

		void release_all_textures();

		const NullTexture* texture(TextureHandle texture) const;
		const Font* font(FontHandle font) const;

		Registry<NullTexture> textures{ "Texture" };
		Registry<Font> fonts{ "Font" };
		Registry<SpriteSheet> sprite_sheets{ "SpriteSheet" };
	};

	// One view's recording, which is a vector of what it was asked to draw.
	//
	// The same shape the OpenGL backend uses, for the same reason and one step
	// further: there, a view records into memory because a GL context belongs
	// to one thread; here there is nothing for it to record into afterwards.
	// Several workers filling their own views at once is safe by construction
	// in both.
	class DrawList::View
	{
	public:
		Renderer::Impl* owner = nullptr;

		std::vector<RecordedSprite> sprites;

		Camera camera = Camera::DEFAULT_CAMERA;
		TextureFilter filter = TextureFilter::point;
		Viewport viewport;

		// Whether anything has been recorded this frame - what makes a dropped
		// view detectable. See Renderer::set_view_count.
		bool touched = false;

		void draw(TextureHandle texture, const SpriteVertex* corners);
		void reset();
	};

	class Renderer::Impl
	{
	public:
		int width = 0;
		int height = 0;

		std::vector<std::unique_ptr<DrawList::View>> views;
		int view_count = 0;

		// What submit() gathers out of the views, in view order. Held here
		// rather than handed back by value so that recorded_sprites() can
		// return a reference and a test can hold it across assertions.
		std::vector<RecordedSprite> recorded;

		const RenderResources* resources = nullptr;
		labrador::DeviceNotify* notify = nullptr;
	};
}
