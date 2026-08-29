#pragma once

#include "engine/core/registry.h"
#include "engine/math/vector2i.h"
#include "engine/render/font.h"
#include "engine/render/gl/gl_functions.h"
#include "engine/render/renderer.h"
#include "engine/render/render_resources.h"
#include "engine/render/sprite_vertex.h"

#include <memory>
#include <string>
#include <vector>
#include "engine/render/camera.h"

// The OpenGL 3.3 core backend, for the files that have to name a context.
//
// renderer.h promises that reaching for the device is "a deliberate include of
// engine/render/<backend>/ and not something a game file can do by accident".
// This is that include. Every client of it is in this folder - the three .cpp
// beside it, and nothing else: sprite_shader.h was named here as a fourth and
// includes nothing at all, being two raw-string GLSL constants under a
// #pragma once. The arrow runs the other way, from renderer.cpp to both. And
// cmake/check_engine_includes.cmake fails the build for anything outside the
// folder that names any header in it.
//
// WHAT THIS BACKEND DOES DIFFERENTLY FROM D3D11, AND IT IS NOT ONE THING.
// The largest is the recording shape: D3D11 has deferred
// contexts, so each view records GPU commands on its own worker thread and
// submit() executes the command lists in order. OpenGL has no such thing - a
// context belongs to one thread and every call is immediate - so a view here
// records into plain memory, vertices and a list of what to draw them with, and
// submit() walks the views in order on the thread that owns the context. That
// is not a workaround: the seam's parallelism axis is views, and the engine
// already produces a view's vertices on the CPU
// (engine/render/sprite_geometry.h), so a view's recording is a std::vector and
// touches no driver at all. It is thread-safe by construction rather than by
// the driver's promise. IT ALSO DOES NOT DISTINGUISH THIS BACKEND, because the
// null one took the same shape one step further and records without replaying.
//
// The rest, each written down where it happens: the shader is compiled at
// device creation rather than at build time (engine/CMakeLists.txt); a resize
// rebuilds nothing, because a WGL default framebuffer follows its window
// (Renderer::window_size_changed, and Impl::drawable_size is what that costs);
// there is no device loss to report (Renderer::Impl below); block compression
// is an extension that has to be asked for and a format this backend will not
// take is refused by name (texture_factory.cpp); the vertex store grows without
// a cap where D3D11 wraps a fixed buffer; the depth range a Viewport carries is
// dropped, there being no depth buffer to apply it to; and the seam's three
// debug markers do nothing.

namespace labrador
{
	// A texture, owned.
	//
	// A GL texture is a name rather than a pointer, so it needs a small owner
	// for the registry to hold - which the two Direct3D backends get free from
	// ComPtr and which the Vulkan one writes by hand for a harder reason than
	// this: a VkImage carries no reference to its device, so its owner holds
	// one (vulkan/backend.h says what that costs). A hand-written owner is the
	// majority case behind this seam, not this backend's peculiarity.
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

	// Where a named texture lives, and only a texture. This class held all
	// three tables once and the sentence here still said so after two of them
	// left: fonts and sheets are engine data and are members of RenderResources
	// itself now (engine/render/render_resources.h says why, and what it
	// saved), so what is behind the pimpl is the one table whose resource type
	// this folder owns.
	class RenderResources::Impl
	{
	public:
		// One table, and it is the only one whose resource type this folder
		// owns. The font and sheet tables are RenderResources' own members;
		// engine/render/render_resources.h says why, and what it saved.
		void add_texture(const std::string& name,
			std::unique_ptr<GlTexture> texture);

		void release_all_textures();

		const GlTexture* texture(TextureHandle texture) const;

		Registry<GlTexture> textures{ "Texture" };
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
	// DeviceNotify is stored and never called here. That is not a gap: three of
	// the five backends need the interface - d3d11, d3d12 and vulkan, the last
	// of which reaches it from a VK_ERROR_DEVICE_LOST rather than from DXGI -
	// and a backend that does not is allowed to say so. This is now the
	// minority answer rather than the odd one out.
	class Renderer::Impl
	{
	public:
		~Impl();

		HWND window = nullptr;
		HDC device_context = nullptr;
		HGLRC gl_context = nullptr;

		// Whether submit() has already run for this frame. renderer.h makes a
		// second submit a no-op, and this backend is the one it was written
		// for: replaying the runs a second time drew every sprite twice, which
		// on a premultiplied blend is visible.
		bool frame_submitted = false;

		// The size the shell last reported, and the only thing
		// window_size_changed answers against. NOT the size of anything this
		// backend draws into - see drawable_size below.
		int reported_width = 0;
		int reported_height = 0;

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
		labrador::DeviceNotify* notify = nullptr;

		GLuint sampler(TextureFilter filter) const;

		// How big the thing being drawn into is, right now, in pixels.
		//
		// THE DEFAULT FRAMEBUFFER OF A WGL CONTEXT IS ITS WINDOW'S CLIENT AREA,
		// so the window is the only authority on this and the backend must not
		// keep a second copy of it. A cached size is wrong exactly when someone
		// is looking: engine/app/window.cpp renders a full frame from WM_PAINT
		// for every step of a drag-resize and discards every WM_SIZE until the
		// drag ends, so a stale copy slides the whole picture down the window
		// under a black band for the duration. No other backend can have that
		// bug, and for two different reasons. The two that draw into a swap
		// chain they created at a size they were told need no height at all to
		// place a pane - that is both Direct3D ones, and it is why this is the
		// file the number has to come out of. Vulkan needs a height and cannot
		// go stale on it
		// anyway: its flip is pane-local, so the origin it measures from is the
		// pane's own bottom edge rather than the buffer's (vulkan/backend.h).
		mattmath::Vector2I drawable_size() const;

		// Makes a 3.3 core context on `window` and loads the entry points.
		void create_context(HWND window);

		// The program, the buffers and the samplers.
		void create_gl_resources();

		// Issues one view's runs. Called from submit(), on the thread that owns
		// the context and never from a worker.
		//
		// The flip height is a parameter rather than a member because every
		// pane of one frame must be placed against the same one - submit()
		// reads it once and hands the same number to every view.
		void replay(const DrawList::View& view, int drawable_height);
	};
}
