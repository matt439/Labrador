#include "engine/render/vulkan/backend.h"

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
		// The public type says Texture, the registry's says VulkanTexture, and
		// only this folder is allowed to know they are the same slot.
		Handle<VulkanTexture> texture_slot(TextureHandle texture)
		{
			return Handle<VulkanTexture>(texture.index());
		}
	}

	// --- VulkanTexture -------------------------------------------------------

	VulkanTexture::~VulkanTexture()
	{
		// THE DEVICE IS STILL HERE BECAUSE THIS OBJECT IS HOLDING IT, which is
		// the whole of what the shared_ptr buys and the reason it is worth one
		// atomic per texture. render_resources.h states that a RenderResources
		// outlives the Renderer it was filled against; on this backend that
		// would otherwise mean vkDestroyImage against a VkDevice that has been
		// destroyed - the one shape of undefined behaviour this API gives no
		// diagnostic for, because there is nothing left to raise one.
		if (!this->owner_ || this->owner_->device == VK_NULL_HANDLE)
		{
			return;
		}

		VkDevice device = this->owner_->device;

		if (this->view_ != VK_NULL_HANDLE)
		{
			vkDestroyImageView(device, this->view_, nullptr);
			this->view_ = VK_NULL_HANDLE;
		}
		if (this->image_ != VK_NULL_HANDLE)
		{
			vkDestroyImage(device, this->image_, nullptr);
			this->image_ = VK_NULL_HANDLE;
		}
		if (this->memory_ != VK_NULL_HANDLE)
		{
			// The allocation, which this API never frees with the thing that
			// was bound to it. Every other backend behind this seam has one
			// object here and this one has two.
			vkFreeMemory(device, this->memory_, nullptr);
			this->memory_ = VK_NULL_HANDLE;
		}
	}

	Vector2F VulkanTexture::size() const
	{
		return Vector2F(static_cast<float>(this->width_),
			static_cast<float>(this->height_));
	}

	// --- RenderResources::Impl -----------------------------------------------

	void RenderResources::Impl::add_texture(const std::string& name,
		std::unique_ptr<VulkanTexture> texture)
	{
		this->textures.add(name, std::move(texture));
	}

	void RenderResources::Impl::release_all_textures()
	{
		this->textures.release_all();
	}

	const VulkanTexture* RenderResources::Impl::texture(
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
	// NO descriptor_slot HERE, WHICH IS THE INTERESTING DIFFERENCE FROM THE
	// D3D12 FILE RATHER THAN THE ONLY ONE. That backend keeps a fixed-size
	// shader-visible heap and has to hand a re-loaded name back the slot it
	// already held, or a client re-walking its manifest runs the heap out. Here
	// a descriptor set is allocated per run out of a pool the frame resets
	// (renderer.cpp, submit), so a texture holds no descriptor at all and
	// re-loading one costs exactly the image it makes and the image it
	// destroys. The pixel test that found the D3D12 leak - three hundred loads
	// under one name - passes here without this backend having to know it
	// exists.
	//
	// The other differences are this API not owning anything: a fourteen-line
	// descriptor_slot is missing, and in its place stand a ~VulkanTexture that
	// frees three handles against a device it holds a reference to, a size()
	// the quad arithmetic asks for, and the include and using-declaration that
	// come with returning one. Counting them as one line was a claim
	// texture_factory.cpp then cited as authority.

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
