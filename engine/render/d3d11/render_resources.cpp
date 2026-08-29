#include "engine/render/d3d11/backend.h"
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
		// The public type says Texture, the registry's says
		// ID3D11ShaderResourceView, and only this folder is allowed to know they
		// are the same slot.
		Handle<ID3D11ShaderResourceView> texture_slot(TextureHandle texture)
		{
			return Handle<ID3D11ShaderResourceView>(texture.index());
		}
	}

	// --- RenderResources::Impl -----------------------------------------------

	void RenderResources::Impl::add_texture(const std::string& name,
		ID3D11ShaderResourceView* texture)
	{
		this->textures.add(name, texture);
	}

	void RenderResources::Impl::release_all_textures()
	{
		this->textures.release_all();
	}

	ID3D11ShaderResourceView* RenderResources::Impl::texture(
		TextureHandle texture) const
	{
		return this->textures.get(texture_slot(texture));
	}

	ID3D11ShaderResourceView* RenderResources::Impl::texture(
		const std::string& name) const
	{
		return this->textures.get(name);
	}

	// --- RenderResources -----------------------------------------------------
	//
	// The front half, and it is a short list on purpose. Carrying the whole
	// public surface here means a page of forwarding calls written out once per
	// backend, because Impl is a complete type only inside a backend folder.
	// Two of the three resource tables hold engine data, so they are the
	// class's own members and their methods are compiled once in
	// engine/render/render_resources.cpp.
	//
	// What stayed did so because each one either touches the texture table or
	// needs Impl complete to make or destroy one. That is the rule
	// render_resources.h states, and it now decides a file list rather than
	// being restated in three comments.

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
