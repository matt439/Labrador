#include "engine/render/null/backend.h"

#include <memory>
#include <string>

namespace labrador
{
	namespace
	{
		// A handle is an index and nothing else (handle.h), so crossing the seam
		// is a reinterpretation of which table the index is into - not a
		// lookup. The public type says Texture, the registry's says
		// NullTexture, and only this folder is allowed to know they are the
		// same slot.
		Handle<NullTexture> texture_slot(TextureHandle texture)
		{
			return Handle<NullTexture>(texture.index());
		}
	}

	// --- RenderResources::Impl -----------------------------------------------

	void RenderResources::Impl::add_texture(const std::string& name,
		std::unique_ptr<NullTexture> texture)
	{
		this->textures.add(name, std::move(texture));
	}

	void RenderResources::Impl::release_all_textures()
	{
		this->textures.release_all();
	}

	const NullTexture* RenderResources::Impl::texture(TextureHandle texture) const
	{
		return this->textures.get(texture_slot(texture));
	}

	// --- RenderResources -----------------------------------------------------
	//
	// The same short list as every backend; render_resources.h states the
	// rule. The texture table's resource type is the struct declared in
	// backend.h, and that is real - a null texture is still a texture, and
	// which table a TextureHandle indexes is this folder's business exactly as
	// it is in the four that have a device.

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
