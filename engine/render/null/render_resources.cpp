#include "engine/render/null/backend.h"
#include "engine/math/vector2f.h"

#include <memory>
#include <string>

using namespace mattmath;

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
	// The same short list the other four keep. THIS BACKEND IS WHERE THE COST
	// OF THE OLD ARRANGEMENT WAS LEGIBLE: a third copy of a page of forwarding
	// calls, in a backend that has no graphics API at all, none of it about one.
	// Its own comment used to say exactly that and then keep the copy. What it
	// kept is a texture table whose resource type is the struct declared just
	// above it in backend.h, and that is real - a null texture is still a
	// texture, and which table a TextureHandle indexes is this folder's
	// business here exactly as it is in the two that have a device.

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
