#include "engine/render/gl/backend.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"
#include "engine/render/gl/sprite_shader.h"
#include "engine/render/sprite_geometry.h"
#include "engine/render/sprite_vertex.h"

#include <cstddef>
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
		const int VERTICES_PER_SPRITE = 4;
		const int INDICES_PER_SPRITE = 6;

		// wglCreateContextAttribsARB, which is itself an extension and so has
		// to be fetched through a context that already exists. That is the
		// bootstrap every WGL program does: a 1.1 context to ask the question,
		// a 3.3 core context to answer it, and the first one deleted.
		using CreateContextAttribs = HGLRC(WINAPI*)(HDC, HGLRC, const int*);

		// Whether two viewports are the same pane. A run cannot span a viewport
		// change - and this is not what closes one, which the sentence here
		// used to claim. DrawList::set_viewport closes the run itself before it
		// writes the new pane, and the only other two writes of View::viewport
		// follow a reset(); every one of the three leaves open_valid false, and
		// the join predicate below tests that first. So this term has never
		// once answered false. It is kept because the predicate it belongs to
		// is the same predicate on four backends and a term missing from one
		// copy is how those four drift apart - not because anything here needs
		// it today.
		bool same_viewport(const Viewport& left, const Viewport& right)
		{
			return left.x == right.x && left.y == right.y &&
				left.width == right.width && left.height == right.height;
		}

		std::string shader_log(GLuint shader)
		{
			GLint length = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH_, &length);
			if (length <= 0)
			{
				return "(the driver offered no message)";
			}

			std::string log(static_cast<size_t>(length), '\0');
			glGetShaderInfoLog(shader, length, nullptr, log.data());
			return log;
		}

		std::string program_log(GLuint program)
		{
			GLint length = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH_, &length);
			if (length <= 0)
			{
				return "(the driver offered no message)";
			}

			std::string log(static_cast<size_t>(length), '\0');
			glGetProgramInfoLog(program, length, nullptr, log.data());
			return log;
		}

		// THE COMPILER'S OWN MESSAGE, NAMED (T6). A shader that will not compile
		// is a build failure on the three backends that compile sprite.hlsl
		// ahead of time - d3d11 and d3d12 through fxc, vulkan through the SDK's
		// dxc (engine/CMakeLists.txt) - and cannot be one here, because GL
		// 3.3 has no offline shader format. So the least this can do is say
		// which stage failed and repeat what the driver said about it, rather
		// than drawing nothing and leaving somebody to guess.
		GLuint compile_shader(GLenum stage, const char* source,
			const char* what)
		{
			const GLuint shader = glCreateShader(stage);
			glShaderSource(shader, 1, &source, nullptr);
			glCompileShader(shader);

			GLint compiled = GL_FALSE;
			glGetShaderiv(shader, GL_COMPILE_STATUS_, &compiled);
			if (compiled == GL_FALSE)
			{
				const std::string log = shader_log(shader);
				glDeleteShader(shader);
				throw std::runtime_error(std::string("The ") + what +
					" shader did not compile on this driver:\n" + log);
			}
			return shader;
		}
	}

	// --- DrawList::View ------------------------------------------------------

	// A DRAW TOUCHES NO DRIVER. Everything a view records is memory: the four
	// corners the engine's geometry produced, and a run saying what to draw them
	// with. That is what lets several workers record at once into a single-
	// threaded API. It is NOT the one place this backend differs in shape from
	// D3D11 - gl/backend.h retracted that heading twelve files ago and this copy
	// of it was left behind - and it is not this backend's alone either: null/
	// and vulkan/ record a frame into vectors the same way, where the two
	// Direct3D backends record into a deferred context and a command list.
	void DrawList::View::draw(const GlTexture& texture,
		const SpriteVertex* corners)
	{
		const int first =
			static_cast<int>(this->vertices.size()) / VERTICES_PER_SPRITE;

		const bool joins = this->open_valid &&
			this->open.texture == texture.name() &&
			this->open.filter == this->filter &&
			same_viewport(this->open.viewport, this->viewport) &&
			this->open.sprites < MAX_RUN_SPRITES;

		if (!joins)
		{
			this->close_run();
			this->open.viewport = this->viewport;
			this->open.texture = texture.name();
			this->open.filter = this->filter;
			this->open.first_sprite = first;
			this->open.sprites = 0;
			this->open_valid = true;
		}

		this->vertices.insert(this->vertices.end(), corners,
			corners + VERTICES_PER_SPRITE);
		this->open.sprites++;
		this->touched = true;
	}

	void DrawList::View::close_run()
	{
		if (!this->open_valid || this->open.sprites == 0)
		{
			this->open_valid = false;
			return;
		}
		this->runs.push_back(this->open);
		this->open_valid = false;
	}

	void DrawList::View::reset()
	{
		this->camera = Camera::DEFAULT_CAMERA;
		this->filter = TextureFilter::point;
		this->vertices.clear();
		this->runs.clear();
		this->open_valid = false;
		this->touched = false;
	}

	// --- DrawList ------------------------------------------------------------

	void DrawList::set_viewport(const Viewport& viewport)
	{
		// Recorded rather than applied. glViewport is a call on a context this
		// thread may not own, and everything already recorded belongs to the
		// old viewport - so the run closes and the new one carries the new
		// pane, which submit() applies in order.
		this->view_->close_run();
		this->view_->viewport = viewport;
	}

	void DrawList::set_camera(const Camera& camera)
	{
		// No flush: the camera is applied to the geometry as each draw is
		// recorded, not by any state a run holds.
		this->view_->camera = camera;
	}

	void DrawList::set_filter(TextureFilter filter)
	{
		if (this->view_->filter == filter)
		{
			return;
		}
		this->view_->close_run();
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
		// RenderPixelTests pins that call order decides - and the sprite vertex
		// has no z for it to ride in. It stays on the seam because the seam is
		// what a client writes against.
		std::ignore = layer_depth;

		const GlTexture& gl_texture =
			*this->view_->owner->resources->impl()->texture(texture);

		SpriteVertex corners[4];
		build_sprite_quad(
			this->view_->camera.calculate_view_rectangle(destination),
			source, gl_texture.size(), tint, rotation, origin, flip, corners);

		this->view_->draw(gl_texture, corners);
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
		const GlTexture& atlas = *resources.impl()->texture(the_font.atlas());
		const Vector2F atlas_size = atlas.size();

		const Vector2F screen_position =
			this->view_->camera.calculate_view_position(position);
		const float screen_scale =
			this->view_->camera.calculate_view_scale(scale);

		// IDENTICAL TO EVERY OTHER BACKEND'S, LINE FOR LINE BAR THE TYPES, which
		// is the point of the walk and the quad both being the engine's. A
		// glyph is a sprite; what is left here is resolving two handles.
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

	Renderer::Impl::~Impl()
	{
		if (this->gl_context != nullptr)
		{
			wglMakeCurrent(this->device_context, nullptr);
			wglDeleteContext(this->gl_context);
		}
		if (this->device_context != nullptr && this->window != nullptr)
		{
			ReleaseDC(this->window, this->device_context);
		}
	}

	GLuint Renderer::Impl::sampler(TextureFilter filter) const
	{
		return filter == TextureFilter::linear
			? this->linear_sampler
			: this->point_sampler;
	}

	Vector2I Renderer::Impl::drawable_size() const
	{
		RECT client = {};
		if (this->window == nullptr || !GetClientRect(this->window, &client))
		{
			return { 0, 0 };
		}
		return { static_cast<int>(client.right - client.left),
			static_cast<int>(client.bottom - client.top) };
	}

	void Renderer::Impl::create_context(HWND native_window)
	{
		this->window = native_window;
		this->device_context = GetDC(native_window);
		if (this->device_context == nullptr)
		{
			throw std::runtime_error(
				"The window has no device context to draw into.");
		}

		// SetPixelFormat may be called once per window and never undone, which
		// is why the usual bootstrap uses a throwaway window: it wants
		// wglChoosePixelFormatARB to ask for multisampling or sRGB before
		// committing. This renderer wants neither - it draws 2D sprites into an
		// 8-bit-per-channel back buffer - so ChoosePixelFormat on the real
		// window is enough, and there is no second window to create and destroy.
		PIXELFORMATDESCRIPTOR wanted = {};
		wanted.nSize = sizeof(wanted);
		wanted.nVersion = 1;
		wanted.dwFlags =
			PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		wanted.iPixelType = PFD_TYPE_RGBA;
		wanted.cColorBits = 32;
		wanted.cAlphaBits = 8;

		const int format = ChoosePixelFormat(this->device_context, &wanted);
		if (format == 0)
		{
			throw std::runtime_error("This window cannot be given a 32-bit "
				"double-buffered OpenGL pixel format.");
		}

		// WHAT WAS ASKED FOR IS NOT NECESSARILY WHAT WAS CHOSEN, and until this
		// block nothing here ever found out. ChoosePixelFormat returns the
		// CLOSEST format it has and is under no obligation to match the
		// request: a driver with no 8-bit alpha answers with one that has none,
		// successfully, and every term this seam promises about a pixel then
		// changes underneath it - what the blend equation's destination alpha
		// is, what read_back_buffer's fourth channel means, and how many
		// distinct values a channel holds at all. The frame would look nearly
		// right and the pixel contract would fail for a reason no assertion in
		// it could name.
		//
		// The other backend cannot have this problem: it names
		// DXGI_FORMAT_B8G8R8A8_UNORM and device creation fails if it cannot
		// have it. This is the same refusal, written out, because GL's
		// selection is a negotiation and D3D's is a demand (T6).
		//
		// PER CHANNEL RATHER THAN cColorBits, which is documented as excluding
		// the alpha bitplanes and which drivers report inconsistently as 24 or
		// 32 for the same format. The four channel counts are unambiguous and
		// are what the contract actually needs.
		//
		// BEFORE SetPixelFormat, which may be called once per window and never
		// undone - so a format worth refusing is refused while refusing is
		// still possible.
		PIXELFORMATDESCRIPTOR given = {};
		if (DescribePixelFormat(this->device_context, format, sizeof(given),
			&given) == 0)
		{
			throw std::runtime_error("The OpenGL pixel format this window was "
				"offered cannot be described.");
		}
		if (given.iPixelType != PFD_TYPE_RGBA ||
			(given.dwFlags & PFD_DOUBLEBUFFER) == 0 ||
			given.cRedBits < 8 || given.cGreenBits < 8 ||
			given.cBlueBits < 8 || given.cAlphaBits < 8)
		{
			throw std::runtime_error("This window's OpenGL pixel format is "
				"r" + std::to_string(given.cRedBits) +
				"g" + std::to_string(given.cGreenBits) +
				"b" + std::to_string(given.cBlueBits) +
				"a" + std::to_string(given.cAlphaBits) +
				(((given.dwFlags & PFD_DOUBLEBUFFER) != 0)
					? ", double buffered" : ", single buffered") +
				". The renderer needs at least eight bits of each channel and "
				"two buffers. ChoosePixelFormat answers with the nearest it "
				"has rather than refusing, so this is where it is refused.");
		}

		if (!SetPixelFormat(this->device_context, format, &wanted))
		{
			throw std::runtime_error("This window cannot be given a 32-bit "
				"double-buffered OpenGL pixel format.");
		}

		// The bootstrap. A 1.1 context exists only so that
		// wglCreateContextAttribsARB can be asked for, because an extension
		// entry point is fetched through a context and the context that can
		// create a 3.3 core one is the thing being fetched.
		HGLRC legacy = wglCreateContext(this->device_context);
		if (legacy == nullptr ||
			!wglMakeCurrent(this->device_context, legacy))
		{
			throw std::runtime_error(
				"This machine has no OpenGL driver at all.");
		}

		CreateContextAttribs create_context_attribs =
			reinterpret_cast<CreateContextAttribs>(
				wglGetProcAddress("wglCreateContextAttribsARB"));
		if (create_context_attribs == nullptr)
		{
			wglMakeCurrent(this->device_context, nullptr);
			wglDeleteContext(legacy);
			throw std::runtime_error("This driver has no "
				"WGL_ARB_create_context, so it cannot make an OpenGL 3.3 core "
				"context. It is an OpenGL 2.1 driver or a software fallback.");
		}

		const int attributes[] =
		{
			WGL_CONTEXT_MAJOR_VERSION_ARB_, 3,
			WGL_CONTEXT_MINOR_VERSION_ARB_, 3,
			WGL_CONTEXT_PROFILE_MASK_ARB_, WGL_CONTEXT_CORE_PROFILE_BIT_ARB_,
			0
		};
		this->gl_context = create_context_attribs(this->device_context,
			nullptr, attributes);

		wglMakeCurrent(this->device_context, nullptr);
		wglDeleteContext(legacy);

		if (this->gl_context == nullptr ||
			!wglMakeCurrent(this->device_context, this->gl_context))
		{
			throw std::runtime_error("This driver refused an OpenGL 3.3 core "
				"context. 3.3 is 2010 hardware and is the floor this engine "
				"builds against.");
		}

		// Loaded against the core context and not the bootstrap one:
		// wglGetProcAddress answers for whatever is current, and the 1.1
		// context has none of these.
		load_gl_functions();
	}

	void Renderer::Impl::create_gl_resources()
	{
		const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER_,
			SPRITE_VERTEX_SHADER, "sprite vertex");
		const GLuint pixel_shader = compile_shader(GL_FRAGMENT_SHADER_,
			SPRITE_PIXEL_SHADER, "sprite fragment");

		this->program = glCreateProgram();
		glAttachShader(this->program, vertex_shader);
		glAttachShader(this->program, pixel_shader);

		// BOUND BEFORE LINKING, so the attribute indices are this file's choice
		// rather than the driver's. The alternative is to link, ask where each
		// attribute ended up, and build the vertex array from the answers -
		// which works and is three more round trips to say what these three
		// lines say.
		glBindAttribLocation(this->program, 0, "position");
		glBindAttribLocation(this->program, 1, "colour");
		glBindAttribLocation(this->program, 2, "texcoord");

		glLinkProgram(this->program);

		GLint linked = GL_FALSE;
		glGetProgramiv(this->program, GL_LINK_STATUS_, &linked);
		if (linked == GL_FALSE)
		{
			const std::string log = program_log(this->program);
			throw std::runtime_error(
				"The sprite shaders did not link on this driver:\n" + log);
		}

		glDeleteShader(vertex_shader);
		glDeleteShader(pixel_shader);

		this->transform_uniform =
			glGetUniformLocation(this->program, "pixels_to_clip");

		glUseProgram(this->program);
		glUniform1i(glGetUniformLocation(this->program, "sprite_texture"), 0);

		// The index buffer, which is the same two triangles per sprite for
		// every sprite there will ever be. A run is capped at this many when it
		// is recorded (DrawList::View::draw) rather than grown here; replay()
		// neither splits nor clamps, and would draw whatever it was handed.
		std::vector<unsigned short> index_data;
		index_data.reserve(static_cast<size_t>(DrawList::View::MAX_RUN_SPRITES)
			* INDICES_PER_SPRITE);
		for (int sprite = 0; sprite < DrawList::View::MAX_RUN_SPRITES;
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

		// A VERTEX ARRAY OBJECT IS NOT OPTIONAL IN A CORE PROFILE. There is no
		// default one, so a core context with none bound draws nothing and
		// reports no error - which is the single commonest way a first core
		// profile port renders a black screen.
		glGenVertexArrays(1, &this->vertex_array);
		glBindVertexArray(this->vertex_array);

		glGenBuffers(1, &this->vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER_, this->vertex_buffer);

		glGenBuffers(1, &this->index_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER_, this->index_buffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER_,
			static_cast<GLsizeiptr>(index_data.size() *
				sizeof(unsigned short)),
			index_data.data(), GL_STATIC_DRAW_);

		// The three fields of SpriteVertex, located by offsetof rather than by
		// position, which is why sprite_vertex.h is free to declare them in any
		// order it likes. The attribute indexes are the shader's, bound by name
		// before linking, and are what has to agree with it.
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
			reinterpret_cast<const void*>(offsetof(SpriteVertex, position)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
			reinterpret_cast<const void*>(offsetof(SpriteVertex, colour)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
			reinterpret_cast<const void*>(offsetof(SpriteVertex, texcoord)));

		// Clamped, so a source rectangle at the edge of an atlas cannot bleed
		// the far side of it into a sprite.
		//
		// AND GL_NEAREST AND GL_LINEAR ARE THE NON-MIPMAP MIN FILTERS, which is
		// this backend's whole share of the seam's level-zero term and is worth
		// saying because it reads as an omission. The mipmap spellings are
		// GL_NEAREST_MIPMAP_NEAREST and friends; these are deliberately not
		// them. This backend has always answered level zero and the other one
		// sampled the chain until its MaxLOD was set to 0 - they agreed only
		// because nothing in either client ships a chain. d3d11/renderer.cpp
		// carries the argument for which way the seam settled it.
		glGenSamplers(1, &this->point_sampler);
		glSamplerParameteri(this->point_sampler, GL_TEXTURE_MIN_FILTER,
			GL_NEAREST);
		glSamplerParameteri(this->point_sampler, GL_TEXTURE_MAG_FILTER,
			GL_NEAREST);
		glSamplerParameteri(this->point_sampler, GL_TEXTURE_WRAP_S,
			static_cast<GLint>(GL_CLAMP_TO_EDGE_));
		glSamplerParameteri(this->point_sampler, GL_TEXTURE_WRAP_T,
			static_cast<GLint>(GL_CLAMP_TO_EDGE_));

		glGenSamplers(1, &this->linear_sampler);
		glSamplerParameteri(this->linear_sampler, GL_TEXTURE_MIN_FILTER,
			GL_LINEAR);
		glSamplerParameteri(this->linear_sampler, GL_TEXTURE_MAG_FILTER,
			GL_LINEAR);
		glSamplerParameteri(this->linear_sampler, GL_TEXTURE_WRAP_S,
			static_cast<GLint>(GL_CLAMP_TO_EDGE_));
		glSamplerParameteri(this->linear_sampler, GL_TEXTURE_WRAP_T,
			static_cast<GLint>(GL_CLAMP_TO_EDGE_));

		// PREMULTIPLIED ALPHA: the source factor is ONE and not SRC_ALPHA.
		// RenderPixelTests calls this the term most likely to be got wrong, and
		// it is the same two words in every rasteriser's descriptor: ONE and
		// INV_SRC_ALPHA on both Direct3D backends, ONE and
		// ONE_MINUS_SRC_ALPHA on Vulkan, and the call below here.
		glEnable(GL_BLEND);
		glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA_, GL_ONE,
			GL_ONE_MINUS_SRC_ALPHA_);
		glBlendEquation(GL_FUNC_ADD_);

		// No depth buffer to test against, and no back face to find: a sprite's
		// winding never changes, because a flip mirrors texture coordinates and
		// leaves the corners where they were.
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		// NO glEnable(GL_TEXTURE_2D), which is what a port from an older GL
		// reaches for and is an error in a core profile - texturing there is
		// decided by the shader sampling something, not by a capability bit.
		// The call is not merely useless: it raises GL_INVALID_ENUM, and GL
		// keeps errors queued, so the next thing to check glGetError blames
		// itself for it. That is exactly how this backend first failed.
		glActiveTexture(GL_TEXTURE0_);
	}

	void Renderer::Impl::replay(const DrawList::View& view, int drawable_height)
	{
		if (view.runs.empty())
		{
			return;
		}

		glBufferData(GL_ARRAY_BUFFER_,
			static_cast<GLsizeiptr>(view.vertices.size() *
				sizeof(SpriteVertex)),
			view.vertices.data(), GL_STREAM_DRAW_);

		for (const DrawList::View::Run& run : view.runs)
		{
			// ONE RECTANGLE, AND BOTH OF THE NEXT TWO CALLS READ IT. GL 3.3
			// core has no float glViewport, so the pixels have to be whole
			// here whatever the seam holds - but which whole pixels is the
			// engine's answer (viewport.h), not this file's, and the
			// projection below must divide by the same numbers the rasteriser
			// was given. Truncating for glViewport and dividing by the
			// un-truncated float is what this used to do, and it scaled every
			// sprite in a fractional pane by the ratio between the two.
			const RectangleI pixels = run.viewport.pixel_rect();

			// GL'S WINDOW ORIGIN IS AT THE BOTTOM LEFT AND THE SEAM'S IS AT THE
			// TOP, so a viewport's y is measured from the other end. This is
			// the only place in the backend where that conversion happens - the
			// vertex positions do not need it, because the clip-space y flip in
			// the shader and GL's origin cancel exactly.
			//
			// AND THIS IS THE ONE PLACE A BACKEND DECIDES WHERE A PIXEL GOES,
			// which is why the height it subtracts from is the framebuffer's
			// own (Impl::drawable_size) and not a number the shell last
			// mentioned. Anything else displaces the whole frame, every pane
			// and every glyph, by the difference between the two.
			glViewport(static_cast<GLint>(pixels.x),
				static_cast<GLint>(drawable_height - (pixels.y + pixels.height)),
				static_cast<GLsizei>(pixels.width),
				static_cast<GLsizei>(pixels.height));

			const float pane_width = static_cast<float>(pixels.width);
			const float pane_height = static_cast<float>(pixels.height);
			glUniform4f(this->transform_uniform,
				pane_width > 0.0f ? 2.0f / pane_width : 0.0f,
				pane_height > 0.0f ? -2.0f / pane_height : 0.0f,
				-1.0f, 1.0f);

			glBindTexture(GL_TEXTURE_2D, run.texture);
			glBindSampler(0, this->sampler(run.filter));

			glDrawElementsBaseVertex(GL_TRIANGLES,
				static_cast<GLsizei>(run.sprites * INDICES_PER_SPRITE),
				GL_UNSIGNED_SHORT, nullptr,
				static_cast<GLint>(run.first_sprite * VERTICES_PER_SPRITE));
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

		this->impl_->reported_width = width;
		this->impl_->reported_height = height;
		this->impl_->create_context(static_cast<HWND>(native_window));

		this->impl_->views.clear();
		this->impl_->views.reserve(static_cast<size_t>(view_capacity));
		for (int i = 0; i < view_capacity; i++)
		{
			this->impl_->views.push_back(std::make_unique<DrawList::View>());
			this->impl_->views.back()->owner = this->impl_.get();
			this->impl_->views.back()->reset();
		}

		this->impl_->create_gl_resources();
	}

	bool Renderer::window_size_changed(int width, int height)
	{
		// BEFORE THERE IS A DEVICE THERE IS NOTHING TO REBUILD, and the seam
		// says the answer is false (renderer.h). Reached by a shell that gets a
		// WM_SIZE between creating its window and creating its device, which is
		// an ordering Win32 allows and nothing here forbids -
		// tests/render/renderer_seam_tests.cpp holds all five backends to it.
		if (this->impl_->gl_context == nullptr)
		{
			return false;
		}

		if (width == this->impl_->reported_width &&
			height == this->impl_->reported_height)
		{
			return false;
		}

		// NOTHING IS REBUILT, WHICH IS THE WHOLE DIFFERENCE. A swap chain has
		// buffers of a fixed size and a resize destroys and remakes them; the
		// default framebuffer of a WGL context follows its window on its own.
		// The answer is still "yes, re-run the layout", because that is what
		// the shell asked.
		//
		// SO THE ONLY THING THIS WRITES IS THE ANSWER TO ITS OWN NEXT CALL.
		// Nothing that places a pixel reads these two, and they are compared
		// against the shell's number rather than the window's on purpose: the
		// shell suppresses every WM_SIZE for the duration of a drag and then
		// forwards GetClientRect on WM_EXITSIZEMOVE, so a backend that answered
		// this against the live client rect would answer "nothing changed" to
		// the one message that ends a resize, where the other three answer true
		// (the null backend has no client rect to disagree with). Two backends
		// disagreeing about the shell's signal is the same class of bug as two
		// disagreeing about a pixel.
		this->impl_->reported_width = width;
		this->impl_->reported_height = height;

		// AND THE FRAME IN PROGRESS IS RESTARTED, WHICH THIS BACKEND DOES NOT
		// NEED AND DOES ANYWAY.
		//
		// renderer.h makes it a term of the seam that a resize arriving
		// mid-frame drops what every view has recorded and reopens them on the
		// buffer that now exists. On the two Direct3D backends that is a
		// correctness fix: their recordings name a render target the resize
		// destroys, and executing one afterwards is a crash or a refusal. Here
		// nothing is destroyed - a WGL context's default framebuffer follows
		// its window - so this backend could keep every vertex it has recorded
		// and replay it happily at the new size.
		//
		// It does not, because the difference would be observable. A caller
		// that draws, is resized under it, and submits would see its drawing
		// survive on this backend and vanish on the other four, from one call,
		// with nothing in the seam to say which to expect - and all four say so
		// in their own files, d3d12 and vulkan in the same capitals. The
		// paragraph above says
		// what that class of disagreement is worth: two backends disagreeing
		// about the shell's signal is the same class of bug as two disagreeing
		// about a pixel. So the cheaper answer is thrown away deliberately, and
		// what a client can rely on is the same sentence everywhere.
		//
		// The clear is here for the same reason: the other four hand back a
		// cleared buffer, and the region a widened window just exposed would
		// otherwise hold whatever was in it.
		const Vector2I drawable = this->impl_->drawable_size();

		glViewport(0, 0, static_cast<GLsizei>(drawable.x),
			static_cast<GLsizei>(drawable.y));
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		for (std::unique_ptr<DrawList::View>& view : this->impl_->views)
		{
			view->reset();
			view->viewport = Viewport(0.0f, 0.0f,
				static_cast<float>(drawable.x),
				static_cast<float>(drawable.y));
		}

		return true;
	}

	void Renderer::set_device_notify(DeviceNotify* device_notify)
	{
		// Stored and never called. A WGL context is not lost the way a Direct3D
		// device is, so there is no loss to report - see the class comment on
		// Renderer::Impl.
		this->impl_->notify = device_notify;
	}

	void Renderer::set_resources(const RenderResources* resources)
	{
		this->impl_->resources = resources;
	}

	void Renderer::begin_frame()
	{
		this->impl_->frame_submitted = false;

		const Vector2I drawable = this->impl_->drawable_size();

		glViewport(0, 0, static_cast<GLsizei>(drawable.x),
			static_cast<GLsizei>(drawable.y));
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		for (std::unique_ptr<DrawList::View>& view : this->impl_->views)
		{
			view->reset();
			view->viewport = Viewport(0.0f, 0.0f,
				static_cast<float>(drawable.x),
				static_cast<float>(drawable.y));
		}

		this->impl_->view_count = 0;
	}

	void Renderer::end_frame()
	{
		SwapBuffers(this->impl_->device_context);
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

		// Lowering it past a view something has already drawn into strands that
		// recording. The two backends that record into a command list have a
		// harder version of this problem - the commands are inside a deferred
		// context on d3d11 and an ID3D12GraphicsCommandList on d3d12 - but the
		// answer the seam gives is the same, so it is the same throw on all five.
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
		// ONCE PER FRAME, AND A SECOND CALL ADDS NOTHING (renderer.h). Cleared
		// by begin_frame, which is the only thing that starts a frame.
		if (this->impl_->frame_submitted)
		{
			return;
		}
		this->impl_->frame_submitted = true;

		glUseProgram(this->impl_->program);
		glBindVertexArray(this->impl_->vertex_array);
		glBindBuffer(GL_ARRAY_BUFFER_, this->impl_->vertex_buffer);
		glActiveTexture(GL_TEXTURE0_);

		// Once, for the whole frame. Asking the window per view would let two
		// panes of one picture be placed against two different heights if the
		// window changed size between them.
		const int drawable_height = this->impl_->drawable_size().y;

		// In view order, which is the only ordering guarantee the seam makes -
		// and here it is the only ordering there is, because a view's recording
		// is a vector rather than a command list.
		for (int i = 0; i < this->impl_->view_count; i++)
		{
			DrawList::View& view = *this->impl_->views[static_cast<size_t>(i)];
			view.close_run();
			this->impl_->replay(view, drawable_height);
		}
	}

	Vector2F Renderer::back_buffer_size() const
	{
		// The window's, not the shell's. The seam promises the size of the
		// buffer read_back_buffer copies out, and here that buffer is the
		// window's client area whatever anyone last said about it.
		const Vector2I drawable = this->impl_->drawable_size();
		return { static_cast<float>(drawable.x),
			static_cast<float>(drawable.y) };
	}

	void Renderer::read_back_buffer(std::vector<unsigned char>& pixels)
	{
		// Same source as back_buffer_size, which is what makes the seam's
		// "exactly width * height * 4 bytes" true. Sized from anything else,
		// a window that had shrunk would have glReadPixels reading rows that
		// are not in the framebuffer, which GL leaves undefined.
		const Vector2I drawable = this->impl_->drawable_size();
		const size_t width = static_cast<size_t>(drawable.x);
		const size_t height = static_cast<size_t>(drawable.y);
		pixels.resize(width * height * 4);

		glReadBuffer(GL_BACK);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);

		std::vector<unsigned char> upside_down(pixels.size());
		glReadPixels(0, 0, static_cast<GLsizei>(width),
			static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE,
			upside_down.data());

		// FLIPPED, BECAUSE GL READS FROM THE BOTTOM. The seam promises the top
		// row first, and glReadPixels starts at the window's origin - which in
		// GL is the bottom left. No swizzle, though: the seam promises RGBA and
		// that is what was asked for, where the other three rasterisers' buffers
		// are all B8G8R8A8 and all three swap on the way out. This backend is the
		// only one that turns its rows over and the only one that does not swap.
		for (size_t y = 0; y < height; y++)
		{
			const unsigned char* source =
				upside_down.data() + (height - 1 - y) * width * 4;
			std::memcpy(pixels.data() + y * width * 4, source, width * 4);
		}
	}

	// Debug markers, and this backend has none.
	//
	// KHR_debug's glPushDebugGroup is the equivalent and is not wired up: it is
	// an extension, it is a debugging convenience rather than anything a frame
	// depends on, and a marker that does nothing is honest where a marker that
	// silently needs an extension is not. The seam's three calls exist because
	// ONE backend has PIX to talk to (T1) - d3d11, and it is the only one of the
	// five that forwards them. d3d12, vulkan and null discard them exactly as
	// this does, so the no-op is the ordinary case and not this backend's
	// exception. renderer.h says what a marker is entitled to assume.
	void Renderer::begin_marker(const wchar_t* name)
	{
		std::ignore = name;
	}

	void Renderer::end_marker()
	{

	}

	void Renderer::set_marker(const wchar_t* name)
	{
		std::ignore = name;
	}
}
