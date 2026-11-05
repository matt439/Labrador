#include "engine/render/d3d11/backend.h"

#include <memory>
#include <string>
#include <utility>

namespace labrador
{
	namespace
	{
		// A handle is an index and nothing else (handle.h), so crossing the seam
		// is a reinterpretation of which table the index is into - not a lookup.
		// The public type says Texture, the registry's says
		// D3D11Texture, and only this folder is allowed to know they are the
		// same slot.
		Handle<D3D11Texture> texture_slot(TextureHandle texture)
		{
			return Handle<D3D11Texture>(texture.index());
		}
	}

	// --- RenderResources::Impl -----------------------------------------------

	void RenderResources::Impl::add_texture(const std::string& name,
		std::unique_ptr<D3D11Texture> texture)
	{
		this->textures.add(name, std::move(texture));
	}

	void RenderResources::Impl::release_all_textures()
	{
		this->textures.release_all();
	}

	const D3D11Texture* RenderResources::Impl::texture(
		TextureHandle texture) const
	{
		return this->textures.get(texture_slot(texture));
	}

	// --- RenderResources -----------------------------------------------------
	//
	// The front half, and a short list: these need Impl complete or touch the
	// texture table (render_resources.h). Everything else is compiled once in
	// engine/render/render_resources.cpp.

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

	// Textures, and this backend has no second kind. The fonts and the sheets
	// stay: each one's device half is a texture handle, and that slot is
	// refilled by the reload rather than remade.
	void RenderResources::release_device_resources()
	{
		this->impl_->release_all_textures();
	}
}
