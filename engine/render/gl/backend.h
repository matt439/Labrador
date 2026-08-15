#pragma once

#include "engine/core/registry.h"
#include "engine/render/font.h"
#include "engine/render/gl/gl_functions.h"
#include "engine/render/renderer.h"
#include "engine/render/render_resources.h"
#include "engine/render/sprite_vertex.h"

#include <memory>
#include <string>
#include <vector>
#include "engine/render/camera.h"

// The OpenGL 3.3 core backend, for the one caller that has to name it.
//
// renderer.h promises that reaching for the device is "a deliberate include of
// engine/render/<backend>/ and not something a game file can do by accident".
// This is that include, and its only client is
// engine/render/gl/texture_factory.cpp. A second would be a mistake.
//
// WHAT THIS BACKEND DOES DIFFERENTLY FROM THE OTHER ONE, and it is one thing.
// D3D11 has deferred contexts, so each view records GPU commands on its own
// worker thread and submit() executes the command lists in order. OpenGL has no
// such thing: a context belongs to one thread and every call is immediate. So a
// view here records into plain memory - vertices, and a list of what to draw
// them with - and submit() walks the views in order on the thread that owns the
// context, issuing the calls.
//
// That is not a workaround. The seam's parallelism axis is views, and the
// engine already produces a view's vertices on the CPU
// (engine/render/sprite_geometry.h), so a view's recording is a std::vector and
// touches no driver at all. It is thread-safe by construction rather than by
// the driver's promise, and it is what makes this backend's per-draw path
// shorter than the one it copies.

namespace artattack
{
	// A texture, owned.
	//
	// A GL texture is a name rather than a pointer, so it needs a small owner
	// for the registry to hold - which the other backend gets free from ComPtr.
	// The size rides along because the geometry needs it every draw and GL will
	// only answer by binding the texture and asking, where D3D11's runtime
	// keeps a description to hand.
	class GlTexture
	{
	public:
		GlTexture(GLuint name, int width, int height)
			: name_(name), width_(width), height_(height) {}
		~GlTexture();

		GlTexture(const GlTexture&) = delete;
		GlTexture& operator=(const GlTexture&) = delete;

		GLuint name() const { return this->name_; }
		mattmath::Vector2F size() const;

	private:
		GLuint name_ = 0;
		int width_ = 0;
		int height_ = 0;
	};

	// Where every named resource lives. Only the first of the three tables is
	// this backend's; fonts and sheets are engine data and are here because the
	// storage of a pimpl is the pimpl's.
	class RenderResources::Impl
	{
	public:
		void add_texture(const std::string& name,
			std::unique_ptr<GlTexture> texture);
		void add_font(const std::string& name, std::unique_ptr<Font> font);
		void add_sprite_sheet(const std::string& name,
			std::unique_ptr<SpriteSheet> sprite_sheet);

		void release_all_textures();

		const GlTexture* texture(TextureHandle texture) const;
		const Font* font(FontHandle font) const;

		Registry<GlTexture> textures{ "Texture" };
		Registry<Font> fonts{ "Font" };
		Registry<SpriteSheet> sprite_sheets{ "SpriteSheet" };
	};

	// One view's recording, which is memory and nothing else.
	class DrawList::View
	{
	public:
		// A run of sprites that share everything a draw call fixes. Closed and
		// pushed when any of the three changes, or at the end of the view.
		struct Run
		{
			Viewport viewport;
			GLuint texture = 0;
			TextureFilter filter = TextureFilter::point;
			int first_sprite = 0;
			int sprites = 0;
		};

		// How many sprites one run may hold. The index buffer is built once for
		// this many and reused with a base vertex, so a longer run is split
		// rather than growing anything.
		static const int MAX_RUN_SPRITES = 2048;

		Renderer::Impl* owner = nullptr;

		std::vector<SpriteVertex> vertices;
		std::vector<Run> runs;

		// The run being built. Not in `runs` until something closes it.
		Run open;
		bool open_valid = false;

		// Reset at the top of every frame, not carried across one.
		Camera camera = Camera::DEFAULT_CAMERA;
		TextureFilter filter = TextureFilter::point;
		Viewport viewport;

		// Whether anything has been recorded into this view this frame. It is
		// what makes a dropped view detectable - see Renderer::set_view_count.
		bool touched = false;

		void draw(const GlTexture& texture, const SpriteVertex* corners);
		void close_run();
		void reset();
	};

	// The context, the window it draws into, and every GL object with a
	// lifetime longer than one frame.
	//
	// NO DEVICE LOSS, AND THEREFORE NO D3DDeviceNotify EQUIVALENT. A WGL
	// context survives everything a Direct3D device does not, so the seam's
	// DeviceNotify is stored and never called here. That is not a gap: the
	// interface exists because one backend needs it, and a backend that does
	// not is allowed to say so.
	class Renderer::Impl
	{
	public:
		~Impl();

		HWND window = nullptr;
		HDC device_context = nullptr;
		HGLRC gl_context = nullptr;

		int width = 0;
		int height = 0;

		std::vector<std::unique_ptr<DrawList::View>> views;
		int view_count = 0;

		GLuint program = 0;
		GLint transform_uniform = -1;
		GLuint vertex_array = 0;
		GLuint vertex_buffer = 0;
		GLuint index_buffer = 0;
		GLuint point_sampler = 0;
		GLuint linear_sampler = 0;

		const RenderResources* resources = nullptr;
		artattack::DeviceNotify* notify = nullptr;

		GLuint sampler(TextureFilter filter) const;

		// Makes a 3.3 core context on `window` and loads the entry points.
		void create_context(HWND window);

		// The program, the buffers and the samplers.
		void create_gl_resources();

		// Issues one view's runs. Called from submit(), on the thread that owns
		// the context and never from a worker.
		void replay(const DrawList::View& view);
	};
}
