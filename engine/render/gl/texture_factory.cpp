#include "engine/render/resource_factory.h"

#include "engine/render/gl/backend.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace labrador
{
	namespace
	{
		// The S3TC internal formats, which are an extension rather than core GL
		// on both contexts, and are the one thing standing between this
		// backend and a context without them: the content.
		const GLenum COMPRESSED_RGBA_S3TC_DXT1 = 0x83F1;
		const GLenum COMPRESSED_RGBA_S3TC_DXT3 = 0x83F2;
		const GLenum COMPRESSED_RGBA_S3TC_DXT5 = 0x83F3;

		// NAMED IN THE THROW, BECAUSE IT IS THE ANSWER. Forty-one of the
		// forty-three .dds between this repository and its client are block
		// compressed, so a context without S3TC has no art and no text - and
		// the useful thing to say is which extension is missing, not that a
		// texture failed. Which context answered, and under which name, is
		// the context's (Renderer::Impl::block_compression).
		GLenum compressed_format(bool available, TextureFormat format,
			const std::string& name)
		{
			if (!available)
			{
				throw std::runtime_error("Texture '" + name + "' is block "
					"compressed and this OpenGL context has no S3TC - "
					"GL_EXT_texture_compression_s3tc on a desktop driver, "
					"WEBGL_compressed_texture_s3tc in a browser. Desktop "
					"drivers and desktop browsers provide it; most phones do "
					"not, and 41 of the 43 .dds this engine loads are in it.");
			}

			switch (format)
			{
			case TextureFormat::bc1_unorm: return COMPRESSED_RGBA_S3TC_DXT1;
			case TextureFormat::bc2_unorm: return COMPRESSED_RGBA_S3TC_DXT3;
			case TextureFormat::bc3_unorm:
			default:                       return COMPRESSED_RGBA_S3TC_DXT5;
			}
		}

		// The uncompressed layouts this backend uploads, and both are given to
		// GL as RGBA.
		//
		// BGRA IS SWAPPED ON THE CPU, ON BOTH CONTEXTS. GL_BGRA has been core
		// since 1.2 and would make b8g8r8a8 a straight upload on the desktop,
		// but it is not an external format GLES accepts, and WebGL2 is GLES.
		// LineSweeper's one texture is b8g8r8a8, so the browser has to swap;
		// the desktop swaps too, because a path only the browser took is one
		// RenderPixelTests never saw. The cost is a pass over the bytes on the
		// load path, and it reaches only the two .dds in either client that are
		// not block compressed.
		void check_uncompressed(TextureFormat format, const std::string& name)
		{
			switch (format)
			{
			case TextureFormat::r8g8b8a8_unorm:
			case TextureFormat::b8g8r8a8_unorm:
				return;
			default: break;
			}

			throw std::runtime_error("Texture '" + name + "' is in a pixel "
				"format this backend cannot upload (texture_format.h). "
				"b4g4r4a4 is the one this reaches: no file in either client "
				"uses it, and GL's nearest packing puts the channels in a "
				"different order, so it is a conversion rather than a "
				"constant and is not written until something needs it.");
		}

		// The pixels of a b8g8r8a8 texture, every level, as r8g8b8a8. The
		// level offsets are unchanged, because the swap moves no byte to
		// another pixel.
		std::vector<unsigned char> rgba_from_bgra(
			const std::vector<unsigned char>& bgra)
		{
			std::vector<unsigned char> rgba(bgra);
			for (std::size_t i = 0; i + 3 < rgba.size(); i += 4)
			{
				std::swap(rgba[i], rgba[i + 2]);
			}
			return rgba;
		}

		struct TextureOwner
		{
			GLuint name = 0;
			~TextureOwner() { glDeleteTextures(1, &name); }
		};
	}

	void add_texture_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& name,
		const TextureData& texture)
	{
		// The renderer is named only so that this cannot be called before the
		// context exists - there is no per-device handle to fetch, because in
		// GL the current context is ambient rather than an argument.
		if (renderer.impl()->context == nullptr)
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

		const bool compressed = is_block_compressed(texture.format);
		GLenum compressed_upload = 0;
		if (compressed)
		{
			compressed_upload = compressed_format(
				renderer.impl()->block_compression, texture.format, name);
		}
		else
		{
			check_uncompressed(texture.format, name);
		}

		// Swapped before anything is allocated, so a refusal above and an
		// allocation failure here both leave GL as they found it.
		const std::vector<unsigned char> swapped =
			texture.format == TextureFormat::b8g8r8a8_unorm
			? rgba_from_bgra(texture.pixels)
			: std::vector<unsigned char>();
		const std::vector<unsigned char>& pixels =
			swapped.empty() ? texture.pixels : swapped;

		TextureOwner pending;
		glGenTextures(1, &pending.name);
		glBindTexture(GL_TEXTURE_2D, pending.name);

		// The level range, stated rather than left at the default. GL's default
		// max level is 1000, and a texture whose chain stops earlier than the
		// sampler expects is incomplete - which draws black rather than
		// failing, and is the classic way a single-level texture disappears.
		//
		// It is belt and braces since the seam settled on level zero, because
		// this backend's min filters are the non-mipmap ones and so ask for no
		// level this could be wrong about. It stays because completeness is a
		// property of the texture rather than of whoever samples it, and the
		// day a sampler here changes is not the day to rediscover that.
		glTexParameteri(GL_TEXTURE_2D, static_cast<GLenum>(
			GL_TEXTURE_BASE_LEVEL_), 0);
		glTexParameteri(GL_TEXTURE_2D, static_cast<GLenum>(
			GL_TEXTURE_MAX_LEVEL_),
			static_cast<GLint>(texture.levels.size()) - 1);

		// Rows are packed, which is what both readers produce and is not GL's
		// default of four-byte alignment.
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		for (size_t i = 0; i < texture.levels.size(); i++)
		{
			const TextureLevel& level = texture.levels[i];
			const void* bytes = pixels.data() + level.offset;

			if (compressed)
			{
				glCompressedTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(i),
					compressed_upload,
					static_cast<GLsizei>(level.width),
					static_cast<GLsizei>(level.height), 0,
					static_cast<GLsizei>(level.size), bytes);
			}
			else
			{
				glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(i), GL_RGBA8,
					static_cast<GLsizei>(level.width),
					static_cast<GLsizei>(level.height), 0,
					GL_RGBA, GL_UNSIGNED_BYTE,
					bytes);
			}
		}

		glBindTexture(GL_TEXTURE_2D, 0);

		if (glGetError() != GL_NO_ERROR)
		{
			throw std::runtime_error("The driver rejected texture '" + name +
				"' at " + std::to_string(texture.width) + "x" +
				std::to_string(texture.height) + ".");
		}

		std::unique_ptr<GlTexture> owned = std::make_unique<GlTexture>(pending.name,
			texture.width, texture.height);
		pending.name = 0;
		resources.impl()->add_texture(name, std::move(owned));
	}
}
