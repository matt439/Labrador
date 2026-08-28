#pragma once

#include <Windows.h>
#include <GL/gl.h>

// The parts of OpenGL this engine uses, and no more.
//
// WHY THERE IS A LOADER AT ALL. Windows ships opengl32.dll at version 1.1 and
// nothing later: every entry point added after 1993 has to be fetched from the
// driver through wglGetProcAddress at run time. Every GL program on Windows
// does this; most of them do it by linking a generated loader that declares all
// three thousand entry points of every version and extension there has ever
// been.
//
// THIS ONE DECLARES THIRTY-SIX, and that is the reason it is written out rather
// than generated. The list below is a complete and readable answer to "how much
// of OpenGL does this engine need", which is the question that decides whether
// an ES variant is a `#version` line or a rewrite - and a generated loader
// answers it with a five-megabyte file. Adding a function here is one line;
// nobody has to wonder whether it was already there.
//
// THAT SENTENCE SAID FORTY-ONE FOR LONGER THAN THE LIST HELD FORTY-ONE, and so
// did the loader's error message and ARCHITECTURE's tree - three copies of a
// number nothing checked. gl_function_count() below is the number now; this
// sentence is a description of it and the message no longer spells one at all.
//
// THE VERSION IS 3.3 CORE, authored to the GLES 3.0 intersection. Three things
// below are outside that intersection: glDrawElementsBaseVertex, which ES 3.0
// has as glDrawElementsBaseVertexOES; GL_BGRA_ eighteen lines down, which is
// not an accepted external format in ES at all and is the EXT_texture_format_
// BGRA8888 token there; and the S3TC formats in texture_factory.cpp, which ES
// has not got and which are named there as the one real obstacle to an ES
// build. The first two are a token and a suffix; the third is the content.

namespace labrador
{
	// The types the post-1.1 signatures need. <GL/gl.h> has none of them,
	// because none of them existed in 1.1.
	using GLchar = char;
	using GLsizeiptr = ptrdiff_t;
	using GLintptr = intptr_t;

	// The constants likewise. Each is the value the registry assigns it; a
	// second backend for a second API translates its own vocabulary into the
	// engine's exactly as this one does, and these are that vocabulary's other
	// end.
	const GLenum GL_CLAMP_TO_EDGE_ = 0x812F;
	const GLenum GL_BGRA_ = 0x80E1;
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

	// The context attributes wglCreateContextAttribsARB takes, which is the one
	// piece of WGL that is itself an extension.
	const int WGL_CONTEXT_MAJOR_VERSION_ARB_ = 0x2091;
	const int WGL_CONTEXT_MINOR_VERSION_ARB_ = 0x2092;
	const int WGL_CONTEXT_PROFILE_MASK_ARB_ = 0x9126;
	const int WGL_CONTEXT_CORE_PROFILE_BIT_ARB_ = 0x00000001;

	// The list, in one place, expanded three ways: declared here, defined in
	// gl_functions.cpp, and loaded there. Adding an entry point means adding
	// one line and nothing else, which is the whole reason for the macro.
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
	X(void, glDrawElementsBaseVertex, (GLenum mode, GLsizei count,             \
		GLenum type, const void* indices, GLint base_vertex))                  \
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

	// Fetches every entry point above from the current context.
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

	// Whether the current context advertises `extension`, by its registry name
	// - "GL_EXT_texture_compression_s3tc" and so on.
	//
	// Through glGetStringi rather than by searching glGetString(GL_EXTENSIONS),
	// which returns null in a core profile. The old spelling is the one every
	// tutorial still shows and the reason a core-profile port fails at its
	// first extension check.
	bool has_gl_extension(const char* extension);
}
