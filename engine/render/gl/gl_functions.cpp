#include "engine/render/gl/gl_functions.h"

#include <cstring>
#include <stdexcept>
#include <string>

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
			return reinterpret_cast<void*>(wglGetProcAddress(name));
		}
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
				"and the context it was given does not provide one of the "    \
				"forty-one entry points in gl_functions.h.");                  \
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
