#include "engine/render/d3d12/backend.h"

#include <memory>
#include <string>

namespace labrador
{
	namespace
	{
		// A handle is an index and nothing else (handle.h), so crossing the seam
		// is a reinterpretation of which table the index is into - not a lookup.
		// The public type says Texture, the registry's says D3d12Texture, and
		// only this folder is allowed to know they are the same slot.
		Handle<D3d12Texture> texture_slot(TextureHandle texture)
		{
			return Handle<D3d12Texture>(texture.index());
		}
	}

	// --- RenderResources::Impl -----------------------------------------------

	void RenderResources::Impl::add_texture(const std::string& name,
		std::unique_ptr<D3d12Texture> texture)
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
		// the number a released D3d12Texture is still holding names a slot in a
		// heap that no longer exists.
		if (!this->textures.contains(name))
		{
			return -1;
		}

		return this->textures.get(name)->slot();
	}

	const D3d12Texture* RenderResources::Impl::texture(
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
	// RESOURCE, which changes nothing here and is worth knowing at the one
	// place that empties it. release_all_textures drops the resources and keeps
	// the names, exactly as everywhere else; the slots those textures held in
	// the shader-visible heap are not returned one at a time, because the heap
	// itself is a device resource and Renderer::Impl remakes it whole - see
	// create_device_dependent_resources, which resets the allocator to zero for
	// the reload to take again.
	//
	// WHICH IS A PRECONDITION ON THE SEAM AND IS NOW WRITTEN THERE. The heap is
	// remade by device creation, so a release that is NOT followed by one
	// leaves this backend's bump allocator where it was and a reload takes
	// fresh slots: a slot per name, out of 256, per round trip. Nothing in this
	// repository does that - the one caller is the shell's device-lost handler
	// - and render_resources.h states it rather than this file growing a free
	// list to make it safe, for the reason given there.
	//
	// A SLOT STOPS BEING NEEDED TWO WAYS AND ONLY ONE OF THEM IS THAT. The
	// other is a texture re-added under a name that already has one, which
	// Registry::add treats as the ordinary case and which is the reason
	// descriptor_slot below exists: the load path asks the table what slot the
	// name holds and writes its new view into that one. Without it a re-load
	// took a fresh slot and abandoned the old, and nothing here would ever have
	// noticed, because a D3d12Texture's slot number is a plain int with no
	// destructor.

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
