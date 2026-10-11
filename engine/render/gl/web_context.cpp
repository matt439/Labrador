// The browser's context: WebGL2 on the page's canvas. wgl_context.cpp is the
// other context, and engine/CMakeLists.txt compiles exactly one of the two.
//
// WebGL2 is GLES 3.0, and everything the rest of this folder calls is in it:
// Emscripten links each entry point directly, so there is no loader here, and
// the three things a 3.3 core context has that ES lacks are answered for both
// contexts at once (gl_functions.h lists where). What is left is making the
// context, saying which dialect to compile, asking for S3TC by the name a
// browser gives it, hearing when the browser takes the context away, and
// telling the host page so.

#include "engine/render/gl/backend.h"

#include "engine/math/vector2i.h"

#include <emscripten/em_js.h>
#include <emscripten/html5.h>

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

using namespace mattmath;

// The page's two optional hooks for a lost context, asked for by name on
// Module exactly as the window's onQuit and onError are, and called only when
// the page set them. Global rather than in labrador::, because EM_JS declares
// a C function, and prefixed for the same reason.
EM_JS(void, labrador_tell_page_context_lost, (), {
	if (typeof Module.onContextLost === "function") {
		Module.onContextLost();
	}
});

EM_JS(void, labrador_tell_page_context_restored, (), {
	if (typeof Module.onContextRestored === "function") {
		Module.onContextRestored();
	}
});

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
	// A LOST CONTEXT IS HEARD AND THEN WAITED OUT. A browser can take a WebGL
	// context away, and from then on every call is a no-op and the canvas is
	// blank. It gives the context back only if the page asked it to, which
	// is what returning true from the lost callback does - Emscripten calls
	// preventDefault on the event for it - and a page that does not ask keeps
	// a blank canvas until it is reloaded. So the callback asks, and notes
	// that a loss happened. Nothing listens for webglcontextrestored: whether
	// the context is lost is a question the context answers at any moment
	// (emscripten_is_webgl_context_lost), and take_restored_context asks it
	// once a frame. The callback does nothing else, because it runs from a
	// DOM event between frames, where nothing in the engine is expecting to
	// be called and an exception would have no catch to reach; backend.h says
	// why the shell is told only once the context is back.
	//
	// The same handle survives the loss. Emscripten keeps its own table of
	// GL objects across it, and a name from before the loss still maps to an
	// object the restored context does not own, which a delete would complain
	// about (GlTexture's destructor says how that is avoided).
	struct Renderer::Impl::Context
	{
		Context() = default;
		~Context()
		{
			if (this->handle != 0)
			{
				// Unhooked first, because the callback is handed this object
				// and must not outlive it. Emscripten's destroy removes every
				// html5 handler on the canvas today, this one with them, but
				// that is what its code does rather than what its API says.
				emscripten_set_webglcontextlost_callback(this->target.c_str(),
					nullptr, false, nullptr);
				emscripten_webgl_destroy_context(this->handle);
			}
		}

		Context(const Context&) = delete;
		Context& operator=(const Context&) = delete;

		std::string target;
		EMSCRIPTEN_WEBGL_CONTEXT_HANDLE handle = 0;

		// Set by the lost callback, cleared by take_restored_context.
		bool lost = false;

		// Set when the page is told of a loss, cleared when it is told of the
		// restore. Apart from `lost` because a rebuild can fail on a context
		// lost again, which sets `lost` a second time for one visible loss.
		bool page_told_lost = false;
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

		// The unmasked names WEBGL_debug_renderer_info answers, and the
		// renderer string a browser gives in their place when it masks the
		// plain pair. Chrome and Safari answer RENDERER with this, which says
		// nothing about whether a frame is drawn by a GPU; Firefox answers it
		// with a sanitised adapter name and deprecates the extension, warning
		// on the console when it is enabled.
		const GLenum UNMASKED_VENDOR_WEBGL = 0x9245;
		const GLenum UNMASKED_RENDERER_WEBGL = 0x9246;
		const char* const MASKED_RENDERER = "WebKit WebGL";
	}

	namespace
	{
		bool on_context_lost(int event_type, const void* reserved,
			void* user_data)
		{
			std::ignore = event_type;
			std::ignore = reserved;
			static_cast<Renderer::Impl::Context*>(user_data)->lost = true;
			// True asks the browser to restore the context later.
			return true;
		}

		// The extensions this backend uses, asked for by name. At creation and
		// again after a restore: an extension is enabled on a context, and a
		// restored context is a new one behind the same handle.
		//
		// S3TC, BY THE BROWSER'S NAME FOR IT. It is the same three formats as
		// the desktop's GL_EXT_texture_compression_s3tc and has to be enabled,
		// not only present, before a compressed upload is accepted. Desktop
		// browsers have it; most phones do not, and texture_factory.cpp is
		// where a device without it is handed decoded pixels instead.
		//
		// THE DEBUG RENDERER INFO ONLY WHERE THE PLAIN NAMES ARE MASKED. The
		// context's own RENDERER is read first, and the extension is enabled
		// only when that is the generic string or nothing, so a browser that
		// already names its adapter there is never asked for an extension it
		// has deprecated. Both halves are decided again after a restore, for
		// the same reason S3TC is: the answer belongs to the new context.
		void enable_extensions(Renderer::Impl& impl)
		{
			const EMSCRIPTEN_WEBGL_CONTEXT_HANDLE handle = impl.context->handle;

			impl.block_compression = emscripten_webgl_enable_extension(handle,
				"WEBGL_compressed_texture_s3tc");

			const std::string plain = impl.device_string(GL_RENDERER);
			const bool masked = plain.empty() || plain == MASKED_RENDERER;
			if (masked && emscripten_webgl_enable_extension(handle,
				"WEBGL_debug_renderer_info"))
			{
				impl.vendor_name = UNMASKED_VENDOR_WEBGL;
				impl.renderer_name = UNMASKED_RENDERER_WEBGL;
			}
			else
			{
				impl.vendor_name = GL_VENDOR;
				impl.renderer_name = GL_RENDERER;
			}
		}
	}

	Renderer::Impl::Impl() = default;
	Renderer::Impl::~Impl() = default;

	bool Renderer::Impl::context_lost() const
	{
		return this->context != nullptr &&
			emscripten_is_webgl_context_lost(this->context->handle);
	}

	std::string Renderer::Impl::device_string(GLenum name) const
	{
		// getParameter on the context, not glGetString: Emscripten keeps one
		// cache of those for the whole page and never clears it, so after a
		// restore it would describe the context that was lost. The answer is
		// the browser's own wording - GL_VERSION reads "WebGL 2.0 (OpenGL ES
		// 3.0 ...)", which describe_device already expects.
		char* value = emscripten_webgl_get_parameter_utf8(name);
		if (value == nullptr)
		{
			return std::string();
		}
		std::string copied(value);
		std::free(value);
		return copied;
	}

	bool Renderer::Impl::take_restored_context()
	{
		if (this->context == nullptr || !this->context->lost ||
			emscripten_is_webgl_context_lost(this->context->handle))
		{
			return false;
		}
		this->context->lost = false;
		enable_extensions(*this);
		return true;
	}

	// FROM A FRAME, NOT FROM THE EVENT. The page's hook is the page's code,
	// and what it does - show a message, or call back into the module - is
	// not something the lost callback could run safely between frames. So
	// the callback only notes the loss, and the first begin_frame after it
	// says so: a frame later at most, which no visitor can tell apart.
	void Renderer::Impl::tell_host_lost()
	{
		if (this->context == nullptr || !this->context->lost ||
			this->context->page_told_lost)
		{
			return;
		}
		this->context->page_told_lost = true;
		labrador_tell_page_context_lost();
	}

	void Renderer::Impl::tell_host_restored()
	{
		if (this->context == nullptr || !this->context->page_told_lost)
		{
			return;
		}
		this->context->page_told_lost = false;
		labrador_tell_page_context_restored();
	}

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

		emscripten_set_webglcontextlost_callback(made->target.c_str(),
			made.get(), false, on_context_lost);

		this->context = std::move(made);
		enable_extensions(*this);

		// No interval to ask for or read back: the browser presents at the
		// display's rate and offers no control over it.
		this->device_info.present_mode = "browser_animation_frame";
		this->shader_prelude = WEB_SHADER_PRELUDE;
	}
}
