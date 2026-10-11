// The browser's context: WebGL2 on the page's canvas. wgl_context.cpp is the
// other context, and engine/CMakeLists.txt compiles exactly one of the two.
//
// WebGL2 is GLES 3.0, and everything the rest of this folder calls is in it:
// Emscripten links each entry point directly, so there is no loader here, and
// the three things a 3.3 core context has that ES lacks are answered for both
// contexts at once (gl_functions.h lists where). What is left is making the
// context, saying which dialect to compile, and asking for S3TC by the name a
// browser gives it.

#include "engine/render/gl/backend.h"

#include "engine/math/vector2i.h"

#include <emscripten/html5.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

using namespace mattmath;

namespace labrador
{
	// --- Renderer::Impl's context half ---------------------------------------

	// The canvas and the context made on it.
	//
	// THE CANVAS IS NAMED, NOT HELD. What create_device is handed is the html5
	// target the web window resolves to the page's canvas, "!canvas", in the
	// place where Win32 hands over an HWND (engine/app/web/window.cpp), and
	// every call below that needs the canvas takes the same string. The context
	// is an integer handle into Emscripten's own table.
	//
	// A LOST CONTEXT IS NOT REPORTED. A browser can take a WebGL context
	// away - a GPU reset, too many contexts on one page - and from then on
	// every call is a no-op and the canvas stays blank until the page is
	// reloaded. Nothing here listens for webglcontextlost, so DeviceNotify is
	// not called; restoring the context through the path renderer.h settles
	// for Direct3D and Vulkan is docs/port/web.md's M6.
	struct Renderer::Impl::Context
	{
		Context() = default;
		~Context()
		{
			if (this->handle != 0)
			{
				emscripten_webgl_destroy_context(this->handle);
			}
		}

		Context(const Context&) = delete;
		Context& operator=(const Context&) = delete;

		std::string target;
		EMSCRIPTEN_WEBGL_CONTEXT_HANDLE handle = 0;
	};

	namespace
	{
		// The GLES dialect, and the qualifiers it has no defaults for.
		//
		// HIGHP RATHER THAN MEDIUMP. A fragment
		// shader in GLES has no default float precision, and the texture
		// coordinate it interpolates addresses an atlas: at mediump, which may
		// be half precision, a coordinate near the right edge of a 2048-texel
		// atlas cannot name every texel, so glyphs would sample their
		// neighbours on exactly the devices that honour the qualifier. highp
		// is required of a GLES 3.0 fragment shader, so it costs no reach. The
		// sampler is highp for the same reason: texture() returns the
		// sampler's precision, and a sampler's default in a fragment shader is
		// lowp.
		const char* const WEB_SHADER_PRELUDE =
			"#version 300 es\n"
			"precision highp float;\n"
			"precision highp sampler2D;\n";

		// The two strings a browser masks by default, and the unmasked names
		// WEBGL_debug_renderer_info answers instead - "WebKit WebGL" says
		// nothing about whether a frame is drawn by a GPU.
		const GLenum UNMASKED_VENDOR_WEBGL = 0x9245;
		const GLenum UNMASKED_RENDERER_WEBGL = 0x9246;
	}

	Renderer::Impl::Impl() = default;
	Renderer::Impl::~Impl() = default;

	Vector2I Renderer::Impl::drawable_size() const
	{
		// The drawing buffer, which the web window sets once a frame from the
		// canvas's CSS box times devicePixelRatio. Asked of the context rather
		// than of the canvas element, because the buffer is what gets drawn
		// into and read back.
		int width = 0;
		int height = 0;
		if (this->context == nullptr ||
			emscripten_webgl_get_drawing_buffer_size(this->context->handle,
				&width, &height) != EMSCRIPTEN_RESULT_SUCCESS)
		{
			return { 0, 0 };
		}
		return { width, height };
	}

	void Renderer::Impl::present()
	{
		// NOTHING, AND THAT IS THE BROWSER'S CONTRACT RATHER THAN A GAP. A
		// canvas is composited when the animation frame callback that drew it
		// returns, and the browser owns the pacing. preserveDrawingBuffer is
		// false, so what was drawn is gone after that - which is why
		// read_back_buffer has to run in the same task that drew, before the
		// page composites. RenderPixelTests in a browser does it by running
		// every case inside one synchronous main().
	}

	void Renderer::Impl::create_context(void* native_window)
	{
		if (native_window == nullptr)
		{
			throw std::invalid_argument("Renderer::create_device was given no "
				"canvas. In a browser the window's handle is the canvas's "
				"html5 target, and it is null once the window has closed.");
		}

		std::unique_ptr<Context> made = std::make_unique<Context>();
		made->target = static_cast<const char*>(native_window);

		// Every term is stated, rather than left at Emscripten's defaults,
		// because each is part of what the seam promises about a pixel.
		EmscriptenWebGLContextAttributes attributes;
		emscripten_webgl_init_context_attributes(&attributes);
		attributes.majorVersion = 2;
		attributes.minorVersion = 0;
		// Opaque. A canvas with alpha is composited over the page, so
		// wherever a frame left alpha below one the page would show through
		// it; a window shows no such thing, and neither should a canvas.
		attributes.alpha = false;
		attributes.premultipliedAlpha = true;
		// No depth, no stencil, no multisampling: the same back buffer the
		// desktop context asks for, and nothing the sprite path reads.
		attributes.depth = false;
		attributes.stencil = false;
		attributes.antialias = false;
		attributes.preserveDrawingBuffer = false;
		attributes.failIfMajorPerformanceCaveat = false;
		// The extensions this backend uses are asked for by name below, so
		// the list is the code's rather than Emscripten's.
		attributes.enableExtensionsByDefault = false;

		made->handle = emscripten_webgl_create_context(made->target.c_str(),
			&attributes);
		// Zero is the only failure: the handle is unsigned, and Emscripten
		// reports no reason beyond the browser console.
		if (made->handle == 0)
		{
			throw std::runtime_error("This browser would not give the canvas "
				"a WebGL2 context. WebGL2 is in every current desktop browser; "
				"it can be turned off, and a GPU the browser has blocked takes "
				"it away.");
		}
		if (emscripten_webgl_make_context_current(made->handle) !=
			EMSCRIPTEN_RESULT_SUCCESS)
		{
			throw std::runtime_error(
				"The canvas's WebGL2 context could not be made current.");
		}

		// S3TC, BY THE BROWSER'S NAME FOR IT. It is the same three formats as
		// the desktop's GL_EXT_texture_compression_s3tc and has to be enabled,
		// not only present, before a compressed upload is accepted. Desktop
		// browsers have it; most phones do not, and texture_factory.cpp is
		// where a device without it is refused.
		this->block_compression = emscripten_webgl_enable_extension(
			made->handle, "WEBGL_compressed_texture_s3tc");

		if (emscripten_webgl_enable_extension(made->handle,
			"WEBGL_debug_renderer_info"))
		{
			this->vendor_name = UNMASKED_VENDOR_WEBGL;
			this->renderer_name = UNMASKED_RENDERER_WEBGL;
		}

		// No interval to ask for or read back: the browser presents at the
		// display's rate and offers no control over it.
		this->device_info.present_mode = "browser_animation_frame";
		this->shader_prelude = WEB_SHADER_PRELUDE;

		this->context = std::move(made);
	}
}
