#include "engine/render/gl/backend.h"
#include "engine/math/vector2f.h"

#include <memory>
#include <string>

using namespace mattmath;

namespace labrador
{
	namespace
	{
		// A handle is an index and nothing else (handle.h), so crossing the seam
		// is a reinterpretation of which table the index is into - not a lookup.
		// The public type says Texture, the registry's says GlTexture, and only
		// this folder is allowed to know they are the same slot.
		Handle<GlTexture> texture_slot(TextureHandle texture)
		{
			return Handle<GlTexture>(texture.index());
		}
	}

	// --- GlTexture -----------------------------------------------------------

	GlTexture::~GlTexture()
	{
		if (this->name_ != 0)
		{
			glDeleteTextures(1, &this->name_);
		}
	}

	Vector2F GlTexture::size() const
	{
		return Vector2F(static_cast<float>(this->width_),
			static_cast<float>(this->height_));
	}

	// --- RenderResources::Impl -----------------------------------------------

	void RenderResources::Impl::add_texture(const std::string& name,
		std::unique_ptr<GlTexture> texture)
	{
		this->textures.add(name, std::move(texture));
	}

	void RenderResources::Impl::release_all_textures()
	{
		this->textures.release_all();
	}

	const GlTexture* RenderResources::Impl::texture(TextureHandle texture) const
	{
		return this->textures.get(texture_slot(texture));
	}

	// --- RenderResources -----------------------------------------------------
	//
	// The same short list engine/render/d3d11/render_resources.cpp keeps, for
	// the same reason, and that is now the whole of the overlap between the two
	// files rather than a page of it: each of these either touches the texture
	// table or needs Impl complete to make or destroy one. Everything else the
	// class declares is compiled once, in engine/render/render_resources.cpp.

	RenderResources::RenderResources() : impl_(std::make_unique<Impl>())
	{

	}

	RenderResources::~RenderResources() = default;
	RenderResources::RenderResources(RenderResources&&) noexcept = default;
	RenderResources& RenderResources::operator=(RenderResources&&) noexcept
		= default;

	TextureHandle RenderResources::resolve_texture(
		const std::string& texture_name) const
	{
		return TextureHandle(
			this->impl_->textures.resolve(texture_name).index());
	}

	void RenderResources::release_device_resources()
	{
		this->impl_->release_all_textures();
	}
}
