#pragma once

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include <Windows.h>
#include <GL/gl.h>
#endif

// The parts of OpenGL this engine uses, and no more.
//
// TWO CONTEXTS SHARE THIS FILE, and it is the one place they differ in
// vocabulary rather than in how a context is made. On Windows the list below
// is a loader; in a browser it is nothing, because Emscripten links every
// GLES 3.0 entry point directly and <GLES3/gl3.h> declares them. The code that
// calls them is the same code either way (this header's #if blocks,
// wgl_context.cpp and web_context.cpp are the only places that know which one
// they are in).
//
// WHY THERE IS A LOADER AT ALL. Windows ships opengl32.dll at version 1.1 and
// nothing later: every entry point added after 1993 has to be fetched from the
// driver through wglGetProcAddress at run time. Every GL program on Windows
// does this; most of them do it by linking a generated loader that declares all
// three thousand entry points of every version and extension there has ever
// been.
//
// THIS ONE IS WRITTEN OUT RATHER THAN GENERATED. The list below is a complete
// and readable answer to "how much of OpenGL does this engine need", which is
// the question that decides whether an ES variant is a `#version` line or a
// rewrite - and a generated loader answers it with a five-megabyte file. Adding
// a function here is one line; nobody has to wonder whether it was already
// there. gl_function_count() is how many there are; no sentence spells it.
//
// THE VERSION IS 3.3 CORE, AUTHORED TO THE GLES 3.0 INTERSECTION, and nothing
// below is outside it. Three things a 3.3 program would reach for are not
// used, and each is answered once for both contexts: a base-vertex draw
// (renderer.cpp points the attributes at a run's first vertex instead), a BGRA
// upload format (texture_factory.cpp swaps BGRA to RGBA on the way in) and
// core S3TC (an extension each context asks for by its own name and reports,
// Renderer::Impl::block_compression).

namespace labrador
{
#if !defined(__EMSCRIPTEN__)
	// The types the post-1.1 signatures need. <GL/gl.h> has none of them,
	// because none of them existed in 1.1. <GLES3/gl3.h> has all three.
	using GLchar = char;
	using GLsizeiptr = ptrdiff_t;
	using GLintptr = intptr_t;
#endif

	// The constants likewise. Each is the value the registry assigns it; a
	// second backend for a second API translates its own vocabulary into the
	// engine's exactly as this one does, and these are that vocabulary's other
	// end. <GLES3/gl3.h> defines every one of them unsuffixed; the suffix is
	// what lets one spelling compile under both headers.
	const GLenum GL_CLAMP_TO_EDGE_ = 0x812F;
	const GLenum GL_TEXTURE_BASE_LEVEL_ = 0x813C;
	const GLenum GL_TEXTURE_MAX_LEVEL_ = 0x813D;
	const GLenum GL_ARRAY_BUFFER_ = 0x8892;
	const GLenum GL_ELEMENT_ARRAY_BUFFER_ = 0x8893;
	const GLenum GL_STATIC_DRAW_ = 0x88E4;
	const GLenum GL_STREAM_DRAW_ = 0x88E0;
	const GLenum GL_FRAGMENT_SHADER_ = 0x8B30;
	const GLenum GL_VERTEX_SHADER_ = 0x8B31;
	const GLenum GL_COMPILE_STATUS_ = 0x8B81;
	const GLenum GL_LINK_STATUS_ = 0x8B82;
	const GLenum GL_INFO_LOG_LENGTH_ = 0x8B84;
	const GLenum GL_TEXTURE0_ = 0x84C0;
	const GLenum GL_NUM_EXTENSIONS_ = 0x821D;
	const GLenum GL_MAJOR_VERSION_ = 0x821B;
	const GLenum GL_MINOR_VERSION_ = 0x821C;
	const GLenum GL_FUNC_ADD_ = 0x8006;
	const GLenum GL_ONE_MINUS_SRC_ALPHA_ = 0x0303;

#if !defined(__EMSCRIPTEN__)
	// The context attributes wglCreateContextAttribsARB takes.
	const int WGL_CONTEXT_MAJOR_VERSION_ARB_ = 0x2091;
	const int WGL_CONTEXT_MINOR_VERSION_ARB_ = 0x2092;
	const int WGL_CONTEXT_PROFILE_MASK_ARB_ = 0x9126;
	const int WGL_CONTEXT_CORE_PROFILE_BIT_ARB_ = 0x00000001;

	// The list, in one place, expanded three ways: declared here, defined in
	// wgl_context.cpp, and loaded there with wglGetProcAddress. A browser
	// build does not expand it at all, because <GLES3/gl3.h> declares these
	// entry points and Emscripten links them. Adding an entry point means
	// adding one line and nothing else, which is the whole reason for the
	// macro.
#define LABRADOR_GL_FUNCTIONS(X)                                              \
	X(void, glGenBuffers, (GLsizei n, GLuint* buffers))                        \
	X(void, glDeleteBuffers, (GLsizei n, const GLuint* buffers))               \
	X(void, glBindBuffer, (GLenum target, GLuint buffer))                      \
	X(void, glBufferData, (GLenum target, GLsizeiptr size, const void* data,   \
		GLenum usage))                                                         \
	X(void, glGenVertexArrays, (GLsizei n, GLuint* arrays))                    \
	X(void, glDeleteVertexArrays, (GLsizei n, const GLuint* arrays))           \
	X(void, glBindVertexArray, (GLuint array))                                 \
	X(void, glEnableVertexAttribArray, (GLuint index))                         \
	X(void, glVertexAttribPointer, (GLuint index, GLint size, GLenum type,     \
		GLboolean normalized, GLsizei stride, const void* pointer))            \
	X(GLuint, glCreateShader, (GLenum type))                                   \
	X(void, glDeleteShader, (GLuint shader))                                   \
	X(void, glShaderSource, (GLuint shader, GLsizei count,                     \
		const GLchar* const* string, const GLint* length))                     \
	X(void, glCompileShader, (GLuint shader))                                  \
	X(void, glGetShaderiv, (GLuint shader, GLenum name, GLint* value))         \
	X(void, glGetShaderInfoLog, (GLuint shader, GLsizei size,                  \
		GLsizei* length, GLchar* log))                                         \
	X(GLuint, glCreateProgram, (void))                                         \
	X(void, glDeleteProgram, (GLuint program))                                 \
	X(void, glAttachShader, (GLuint program, GLuint shader))                   \
	X(void, glBindAttribLocation, (GLuint program, GLuint index,               \
		const GLchar* name))                                                   \
	X(void, glLinkProgram, (GLuint program))                                   \
	X(void, glGetProgramiv, (GLuint program, GLenum name, GLint* value))       \
	X(void, glGetProgramInfoLog, (GLuint program, GLsizei size,                \
		GLsizei* length, GLchar* log))                                         \
	X(void, glUseProgram, (GLuint program))                                    \
	X(GLint, glGetUniformLocation, (GLuint program, const GLchar* name))       \
	X(void, glUniform1i, (GLint location, GLint value))                        \
	X(void, glUniform4f, (GLint location, GLfloat x, GLfloat y, GLfloat z,     \
		GLfloat w))                                                            \
	X(void, glActiveTexture, (GLenum texture))                                 \
	X(void, glCompressedTexImage2D, (GLenum target, GLint level,               \
		GLenum internal_format, GLsizei width, GLsizei height, GLint border,   \
		GLsizei size, const void* data))                                       \
	X(void, glGenSamplers, (GLsizei n, GLuint* samplers))                      \
	X(void, glDeleteSamplers, (GLsizei n, const GLuint* samplers))             \
	X(void, glBindSampler, (GLuint unit, GLuint sampler))                      \
	X(void, glSamplerParameteri, (GLuint sampler, GLenum name, GLint value))   \
	X(void, glBlendFuncSeparate, (GLenum source_rgb, GLenum destination_rgb,   \
		GLenum source_alpha, GLenum destination_alpha))                        \
	X(void, glBlendEquation, (GLenum mode))                                    \
	X(const GLubyte*, glGetStringi, (GLenum name, GLuint index))

#define LABRADOR_GL_DECLARE(result, name, parameters)                         \
	extern result (APIENTRY* name) parameters;
	LABRADOR_GL_FUNCTIONS(LABRADOR_GL_DECLARE)
#undef LABRADOR_GL_DECLARE

	// How many entry points the list above declares, counted from the list.
	//
	// The compiler answers a question a comment kept getting wrong (T5). It is
	// constexpr because the only caller is a message, and a message should not
	// pay for arithmetic that was decidable at compile time.
	constexpr int gl_function_count()
	{
		int count = 0;
#define LABRADOR_GL_COUNT(result, name, parameters) count++;
		LABRADOR_GL_FUNCTIONS(LABRADOR_GL_COUNT)
#undef LABRADOR_GL_COUNT
		return count;
	}

	// Fetches every entry point above from the current context. WGL only.
	//
	// A CONTEXT MUST BE CURRENT, and a 3.3 core one: wglGetProcAddress answers
	// for whatever context is current when it is asked, so loading against the
	// 1.1 context the bootstrap creates first would return a working pointer
	// for some of these and null for the rest.
	//
	// Throws std::runtime_error naming the first entry point the driver does
	// not have. That is a driver too old for 3.3 core, and the message says
	// which call it stopped at rather than crashing on the first frame through
	// a null pointer.
	void load_gl_functions();

	// Requires this device context's GL context to be current. Requests an
	// interval in video frames and returns the driver's stored interval.
	// WGL_EXT_swap_control is required; absence and setter failure throw.
	// This is API state, not proof of when a compositor displays a frame.
	int configure_swap_interval(HDC device_context, int interval);

	// Whether the current context advertises `extension`, by its registry name
	// - "GL_EXT_texture_compression_s3tc" and so on. WGL only: a
	// browser answers whether an extension is there by enabling it
	// (web_context.cpp), which a desktop context has no call for.
	//
	// Through glGetStringi rather than by searching glGetString(GL_EXTENSIONS),
	// which returns null in a core profile. That spelling is the one many
	// tutorials show and the reason a core-profile port fails at its first
	// extension check.
	bool has_gl_extension(const char* extension);
#endif
}
