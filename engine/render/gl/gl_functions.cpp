#include "engine/render/gl/gl_functions.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

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
}
