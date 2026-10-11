// The WGL context: an OpenGL 3.3 core context on a Win32 window, and the
// loader that makes its entry points callable. web_context.cpp is the other
// context, and engine/CMakeLists.txt compiles exactly one of the two.

#include "engine/render/gl/backend.h"

#include "engine/math/vector2i.h"
#include "engine/render/gl/gl_functions.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using namespace mattmath;

namespace labrador
{
#define LABRADOR_GL_DEFINE(result, name, parameters)                          \
	result (APIENTRY* name) parameters = nullptr;
	LABRADOR_GL_FUNCTIONS(LABRADOR_GL_DEFINE)
#undef LABRADOR_GL_DEFINE

	namespace
	{
		// wglGetProcAddress and nothing else.
		//
		// A LOADER THAT FALLS BACK TO GetProcAddress ON opengl32.dll IS THE
		// USUAL SHAPE, and it is here for a reason worth stating rather than
		// copying: wglGetProcAddress is only required to answer for entry
		// points that are *not* in opengl32's own export table, and some
		// drivers return null for the 1.1 ones. Nothing in the list this file
		// loads is from 1.1 - those are called directly and linked from
		// opengl32.lib - so the fallback would never fire and is not here.
		void* load_entry_point(const char* name)
		{
			void* address = reinterpret_cast<void*>(wglGetProcAddress(name));
			const std::intptr_t value = reinterpret_cast<std::intptr_t>(address);
			// ICDs may use these failure sentinels instead of a null pointer.
			return value == 1 || value == 2 || value == 3 || value == -1
				? nullptr : address;
		}

		bool has_wgl_extension(HDC device_context, std::string_view extension)
		{
			using ExtensionsArb = const char*(WINAPI*)(HDC);
			using ExtensionsExt = const char*(WINAPI*)();
			const ExtensionsArb extensions_arb =
				reinterpret_cast<ExtensionsArb>(
					load_entry_point("wglGetExtensionsStringARB"));
			const ExtensionsExt extensions_ext =
				reinterpret_cast<ExtensionsExt>(
					load_entry_point("wglGetExtensionsStringEXT"));
			const char* names = extensions_arb != nullptr
				? extensions_arb(device_context)
				: (extensions_ext != nullptr ? extensions_ext() : nullptr);
			if (names == nullptr)
			{
				return false;
			}

			std::string_view remaining(names);
			while (!remaining.empty())
			{
				const std::size_t end = remaining.find(' ');
				if (remaining.substr(0, end) == extension)
				{
					return true;
				}
				if (end == std::string_view::npos)
				{
					break;
				}
				remaining.remove_prefix(end + 1);
			}
			return false;
		}
	}

	int configure_swap_interval(HDC device_context, int interval)
	{
		using SwapInterval = BOOL(WINAPI*)(int);
		using CurrentSwapInterval = int(WINAPI*)();
		if (!has_wgl_extension(device_context, "WGL_EXT_swap_control"))
		{
			throw std::runtime_error("The OpenGL renderer requires "
				"WGL_EXT_swap_control to configure its presentation interval.");
		}
		const SwapInterval swap_interval = reinterpret_cast<SwapInterval>(
			load_entry_point("wglSwapIntervalEXT"));
		const CurrentSwapInterval current_swap_interval =
			reinterpret_cast<CurrentSwapInterval>(
				load_entry_point("wglGetSwapIntervalEXT"));
		if (swap_interval == nullptr || current_swap_interval == nullptr)
		{
			throw std::runtime_error("The OpenGL driver advertises "
				"WGL_EXT_swap_control but lacks its setter or getter.");
		}
		if (swap_interval(interval) == FALSE)
		{
			const DWORD error = GetLastError();
			throw std::runtime_error("wglSwapIntervalEXT(" +
				std::to_string(interval) + ") failed with Windows error " +
				std::to_string(error) + ".");
		}
		return current_swap_interval();
	}

	void load_gl_functions()
	{
#define LABRADOR_GL_LOAD(result, name, parameters)                            \
		name = reinterpret_cast<result (APIENTRY*) parameters>(                \
			load_entry_point(#name));                                          \
		if (name == nullptr)                                                   \
		{                                                                      \
			throw std::runtime_error(std::string("This driver has no ") +      \
				#name + ". The renderer needs an OpenGL 3.3 core context, "    \
				"and the context it was given does not provide one of the " +  \
				std::to_string(gl_function_count()) +                          \
				" entry points in gl_functions.h.");                           \
		}
		LABRADOR_GL_FUNCTIONS(LABRADOR_GL_LOAD)
#undef LABRADOR_GL_LOAD
	}

	bool has_gl_extension(const char* extension)
	{
		GLint count = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS_, &count);

		for (GLint i = 0; i < count; i++)
		{
			const GLubyte* name =
				glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i));
			if (name != nullptr &&
				std::strcmp(reinterpret_cast<const char*>(name),
					extension) == 0)
			{
				return true;
			}
		}
		return false;
	}

	// --- Renderer::Impl's context half ---------------------------------------

	// The window, its device context and the GL context made on it, released
	// together. A destructor rather than three lines in Impl's, so that a
	// create_context that throws part-way releases what it had made and
	// leaves Impl::context null - which is what everything else reads as
	// "there is no device yet".
	struct Renderer::Impl::Context
	{
		Context() = default;
		~Context()
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

		Context(const Context&) = delete;
		Context& operator=(const Context&) = delete;

		HWND window = nullptr;
		HDC device_context = nullptr;
		HGLRC gl_context = nullptr;
	};

	namespace
	{
		// wglCreateContextAttribsARB, which is itself an extension and so has
		// to be fetched through a context that already exists. That is the
		// bootstrap every WGL program does: a 1.1 context to ask the question,
		// a 3.3 core context to answer it, and the first one deleted.
		using CreateContextAttribs = HGLRC(WINAPI*)(HDC, HGLRC, const int*);

		// The desktop dialect. The body in sprite_shader.h is written to the
		// intersection of this and GLES 3.00, so this line is all it needs.
		const char* const DESKTOP_SHADER_PRELUDE = "#version 330 core\n";
	}

	Renderer::Impl::Impl() = default;
	Renderer::Impl::~Impl() = default;

	Vector2I Renderer::Impl::drawable_size() const
	{
		RECT client = {};
		if (this->context == nullptr ||
			!GetClientRect(this->context->window, &client))
		{
			return { 0, 0 };
		}
		return { static_cast<int>(client.right - client.left),
			static_cast<int>(client.bottom - client.top) };
	}

	void Renderer::Impl::present()
	{
		SwapBuffers(this->context->device_context);
	}

	// A WGL context is not taken away the way a WebGL2 one is, so neither
	// question ever has a second answer here. Both are asked from the drawing
	// path all the same, because that path is the browser's too.
	bool Renderer::Impl::context_lost() const
	{
		return false;
	}

	bool Renderer::Impl::take_restored_context()
	{
		return false;
	}

	std::string Renderer::Impl::device_string(GLenum name) const
	{
		const GLubyte* value = glGetString(name);
		return value == nullptr
			? std::string()
			: std::string(reinterpret_cast<const char*>(value));
	}

	void Renderer::Impl::create_context(void* native_window)
	{
		std::unique_ptr<Context> made = std::make_unique<Context>();
		made->window = static_cast<HWND>(native_window);
		made->device_context = GetDC(made->window);
		if (made->device_context == nullptr)
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

		const int format = ChoosePixelFormat(made->device_context, &wanted);
		if (format == 0)
		{
			throw std::runtime_error("This window cannot be given a 32-bit "
				"double-buffered OpenGL pixel format.");
		}

		// WHAT WAS ASKED FOR IS NOT NECESSARILY WHAT WAS CHOSEN.
		// ChoosePixelFormat returns the CLOSEST format it has and is under no
		// obligation to match the request: a driver with no 8-bit alpha
		// answers with one that has none, successfully, and every term this
		// seam promises about a pixel then changes underneath it - what the
		// blend equation's destination alpha is, what read_back_buffer's
		// fourth channel means, and how many distinct values a channel holds
		// at all. The frame would look nearly right and the pixel contract
		// would fail for a reason no assertion in it could name.
		//
		// The Direct3D backends cannot have this problem: they name
		// DXGI_FORMAT_B8G8R8A8_UNORM and device creation fails if they cannot
		// have it. Nor can Vulkan, which draws into a B8G8R8A8_UNORM image it
		// creates itself, whatever its swapchain negotiates, and blits that
		// image, which converts. This is the same refusal, written out,
		// because GL's selection is a negotiation and D3D's is a demand (T6).
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
		if (DescribePixelFormat(made->device_context, format, sizeof(given),
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
		this->software_context = (given.dwFlags & PFD_GENERIC_FORMAT) != 0;

		if (!SetPixelFormat(made->device_context, format, &wanted))
		{
			throw std::runtime_error("This window cannot be given a 32-bit "
				"double-buffered OpenGL pixel format.");
		}

		// The bootstrap. A 1.1 context exists only so that
		// wglCreateContextAttribsARB can be asked for, because an extension
		// entry point is fetched through a context and the context that can
		// create a 3.3 core one is the thing being fetched.
		HGLRC legacy = wglCreateContext(made->device_context);
		if (legacy == nullptr ||
			!wglMakeCurrent(made->device_context, legacy))
		{
			throw std::runtime_error(
				"This machine has no OpenGL driver at all.");
		}

		CreateContextAttribs create_context_attribs =
			reinterpret_cast<CreateContextAttribs>(
				wglGetProcAddress("wglCreateContextAttribsARB"));
		if (create_context_attribs == nullptr)
		{
			wglMakeCurrent(made->device_context, nullptr);
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
		made->gl_context = create_context_attribs(made->device_context,
			nullptr, attributes);

		wglMakeCurrent(made->device_context, nullptr);
		wglDeleteContext(legacy);

		if (made->gl_context == nullptr ||
			!wglMakeCurrent(made->device_context, made->gl_context))
		{
			throw std::runtime_error("This driver refused an OpenGL 3.3 core "
				"context. 3.3 is 2010 hardware and is the floor this engine "
				"builds against.");
		}

		// Loaded against the core context and not the bootstrap one:
		// wglGetProcAddress answers for whatever is current, and the 1.1
		// context has none of these.
		load_gl_functions();

		// What this context answers. The swap interval is asked for here
		// because it is a property of the context rather than of a frame, and
		// what the driver stored is reported beside what was asked for.
		this->device_info.present_mode = "wgl_swap_interval";
		this->device_info.requested_swap_interval = 1;
		this->device_info.reported_swap_interval = configure_swap_interval(
			made->device_context, *this->device_info.requested_swap_interval);
		this->block_compression =
			has_gl_extension("GL_EXT_texture_compression_s3tc");
		this->shader_prelude = DESKTOP_SHADER_PRELUDE;

		this->context = std::move(made);
	}
}
