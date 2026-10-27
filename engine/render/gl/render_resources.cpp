#include "engine/render/gl/backend.h"
#include "engine/math/vector2f.h"

#include <memory>
#include <string>

using namespace labrador;

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
		// ONLY A NAME THE CURRENT CONTEXT OWNS IS DELETED. A texture made
		// before a WebGL2 context was lost is released after the restore
		// (Renderer::Impl::restore_device tells the shell to), and by then its
		// object died with the old context: glIsTexture answers false for it
		// without complaint, where a delete is an error the browser reports.
		// On WGL a name is never stale. A texture released while the context
		// is current is that context's own, and one released at shutdown,
		// after ~Renderer has deleted the context (render_resources.h says why
		// the table outlives the renderer), finds no context current, so the
		// question answers false and nothing is deleted - which a delete would
		// not have done either, the context having taken its names with it.
		if (this->name_ != 0 && glIsTexture(this->name_) == GL_TRUE)
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
	// the same reason: each of these either touches the texture
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
