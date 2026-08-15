#include "engine/render/resource_factory.h"

#include "engine/render/gl/backend.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"

#include <memory>
#include <stdexcept>
#include <string>

namespace artattack
{
	namespace
	{
		// The S3TC internal formats, which are an extension rather than core GL
		// and are the whole of what stands between this backend and an ES one.
		const GLenum COMPRESSED_RGBA_S3TC_DXT1 = 0x83F1;
		const GLenum COMPRESSED_RGBA_S3TC_DXT3 = 0x83F2;
		const GLenum COMPRESSED_RGBA_S3TC_DXT5 = 0x83F3;

		// Checked once, on the first compressed texture, and remembered.
		//
		// NAMED IN THE THROW, BECAUSE IT IS THE ANSWER. Forty-three of the
		// forty-five images between this repository and its client are block
		// compressed, so a context without S3TC has no art and no text - and
		// the useful thing to say is which extension is missing, not that a
		// texture failed.
		bool s3tc_available()
		{
			static const bool available =
				has_gl_extension("GL_EXT_texture_compression_s3tc");
			return available;
		}

		GLenum compressed_format(TextureFormat format, const std::string& name)
		{
			if (!s3tc_available())
			{
				throw std::runtime_error("Texture '" + name + "' is block "
					"compressed and this OpenGL context has no "
					"GL_EXT_texture_compression_s3tc. Desktop drivers all "
					"provide it; GLES 3.0 does not, and 43 of the 45 images "
					"this engine loads are in it.");
			}

			switch (format)
			{
			case TextureFormat::bc1_unorm: return COMPRESSED_RGBA_S3TC_DXT1;
			case TextureFormat::bc2_unorm: return COMPRESSED_RGBA_S3TC_DXT3;
			case TextureFormat::bc3_unorm:
			default:                       return COMPRESSED_RGBA_S3TC_DXT5;
			}
		}

		// The uncompressed layouts, as the pair glTexImage2D wants: what the
		// bytes are, and what to store them as.
		//
		// GL_BGRA IS CORE SINCE 1.2 and is what makes the commonest .dds in
		// this tree a straight upload rather than a swizzle on the CPU. A
		// backend that had to swap the channels itself would be doing per-pixel
		// work on the load path for every texture, every run.
		GLenum source_format(TextureFormat format, const std::string& name)
		{
			switch (format)
			{
			case TextureFormat::r8g8b8a8_unorm: return GL_RGBA;
			case TextureFormat::b8g8r8a8_unorm: return GL_BGRA_;
			default: break;
			}

			throw std::runtime_error("Texture '" + name + "' is in a pixel "
				"format this backend cannot upload (texture_format.h). "
				"b4g4r4a4 is the one this reaches: no file in either client "
				"uses it, and GL's nearest packing puts the channels in a "
				"different order, so it is a conversion rather than a "
				"constant and is not written until something needs it.");
		}
	}

	void add_texture_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& name,
		const TextureData& texture)
	{
		// The renderer is named only so that this cannot be called before the
		// context exists - there is no per-device handle to fetch, because in
		// GL the current context is ambient rather than an argument.
		if (renderer.impl()->gl_context == nullptr)
		{
			throw std::runtime_error("Texture '" + name + "' was loaded before "
				"create_device made a context.");
		}

		// GL QUEUES ERRORS AND HANDS THEM OUT ONE AT A TIME, so an error raised
		// by anything earlier is still waiting and the check below would report
		// it as this texture's. Drained first, which is the difference between
		// a message naming the real problem and one naming the last innocent
		// caller.
		while (glGetError() != GL_NO_ERROR)
		{

		}

		GLuint name_gl = 0;
		glGenTextures(1, &name_gl);
		glBindTexture(GL_TEXTURE_2D, name_gl);

		// The level range, stated rather than left at the default. GL's default
		// max level is 1000, and a texture whose chain stops earlier than the
		// sampler expects is incomplete - which draws black rather than
		// failing, and is the classic way a single-level texture disappears.
		glTexParameteri(GL_TEXTURE_2D, static_cast<GLenum>(
			GL_TEXTURE_BASE_LEVEL_), 0);
		glTexParameteri(GL_TEXTURE_2D, static_cast<GLenum>(
			GL_TEXTURE_MAX_LEVEL_),
			static_cast<GLint>(texture.levels.size()) - 1);

		// Rows are packed, which is what both readers produce and is not GL's
		// default of four-byte alignment.
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		const bool compressed = is_block_compressed(texture.format);
		for (size_t i = 0; i < texture.levels.size(); i++)
		{
			const TextureLevel& level = texture.levels[i];
			const void* bytes = texture.pixels.data() + level.offset;

			if (compressed)
			{
				glCompressedTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(i),
					compressed_format(texture.format, name),
					static_cast<GLsizei>(level.width),
					static_cast<GLsizei>(level.height), 0,
					static_cast<GLsizei>(level.size), bytes);
			}
			else
			{
				glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(i), GL_RGBA8,
					static_cast<GLsizei>(level.width),
					static_cast<GLsizei>(level.height), 0,
					source_format(texture.format, name), GL_UNSIGNED_BYTE,
					bytes);
			}
		}

		glBindTexture(GL_TEXTURE_2D, 0);

		if (glGetError() != GL_NO_ERROR)
		{
			glDeleteTextures(1, &name_gl);
			throw std::runtime_error("The driver rejected texture '" + name +
				"' at " + std::to_string(texture.width) + "x" +
				std::to_string(texture.height) + ".");
		}

		resources.impl()->add_texture(name,
			std::make_unique<GlTexture>(name_gl, texture.width,
				texture.height));
	}
}
