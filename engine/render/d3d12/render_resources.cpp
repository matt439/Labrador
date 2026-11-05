#include "engine/render/d3d12/backend.h"

#include <memory>
#include <string>

namespace labrador
{
	namespace
	{
		// A handle is an index and nothing else (handle.h), so crossing the seam
		// is a reinterpretation of which table the index is into - not a lookup.
		// The public type says Texture, the registry's says D3D12Texture, and
		// only this folder is allowed to know they are the same slot.
		Handle<D3D12Texture> texture_slot(TextureHandle texture)
		{
			return Handle<D3D12Texture>(texture.index());
		}
	}

	// --- RenderResources::Impl -----------------------------------------------

	void RenderResources::Impl::add_texture(const std::string& name,
		std::unique_ptr<D3D12Texture> texture)
	{
		this->textures.add(name, std::move(texture));
	}

	void RenderResources::Impl::release_all_textures()
	{
		this->textures.release_all();
	}

	int RenderResources::Impl::descriptor_slot(const std::string& name) const
	{
		// contains() rather than a find, because a released slot has to read as
		// absent here: after a device loss the heap is remade and
		// create_device_dependent_resources resets the allocator to zero, so
		// the number a released D3D12Texture is still holding names a slot in a
		// heap that no longer exists.
		if (!this->textures.contains(name))
		{
			return -1;
		}

		return this->textures.get(name)->slot();
	}

	const D3D12Texture* RenderResources::Impl::texture(
		TextureHandle texture) const
	{
		return this->textures.get(texture_slot(texture));
	}

	// --- RenderResources -----------------------------------------------------
	//
	// The same short list the other four backends keep, for the same reason:
	// each of these either touches the texture table or needs Impl complete to
	// make or destroy one. Everything else the class declares is compiled once,
	// in engine/render/render_resources.cpp.
	//
	// AND THIS BACKEND'S TEXTURE TABLE HOLDS A DESCRIPTOR SLOT AS WELL AS A
	// RESOURCE. release_all_textures drops the resources and keeps the names,
	// exactly as everywhere else; the slots are not returned one at a time,
	// because Renderer::Impl remakes the shader-visible heap whole with the
	// device and resets its allocator (create_device_dependent_resources).
	// That a release is followed by device creation is the seam's
	// precondition (render_resources.h); a name re-added reuses its slot
	// through descriptor_slot (backend.h).

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
