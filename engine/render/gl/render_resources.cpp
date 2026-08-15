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

	void RenderResources::Impl::add_font(const std::string& name,
		std::unique_ptr<Font> font)
	{
		this->fonts.add(name, std::move(font));
	}

	void RenderResources::Impl::add_sprite_sheet(const std::string& name,
		std::unique_ptr<SpriteSheet> sprite_sheet)
	{
		this->sprite_sheets.add(name, std::move(sprite_sheet));
	}

	void RenderResources::Impl::release_all_textures()
	{
		this->textures.release_all();
	}

	const GlTexture* RenderResources::Impl::texture(TextureHandle texture) const
	{
		return this->textures.get(texture_slot(texture));
	}

	const Font* RenderResources::Impl::font(FontHandle font) const
	{
		return this->fonts.get(font);
	}

	// --- RenderResources -----------------------------------------------------
	//
	// Word for word what engine/render/d3d11/render_resources.cpp has below its
	// own divider, because none of it is about a graphics API - it is defined
	// here only because Impl is a complete type in this folder and nowhere else.
	// That duplication is the price of the pimpl and it is a page of forwarding
	// calls; the alternative, a virtual table on the resource store, is a cost
	// per lookup on the frame path (T8) to save a page of code once per backend.

	RenderResources::RenderResources() : impl_(std::make_unique<Impl>())
	{

	}

	RenderResources::~RenderResources() = default;
	RenderResources::RenderResources(RenderResources&&) noexcept = default;
	RenderResources& RenderResources::operator=(RenderResources&&) noexcept
		= default;

	void RenderResources::add_sprite_sheet(const std::string& sprite_sheet_name,
		std::unique_ptr<SpriteSheet> sprite_sheet)
	{
		this->impl_->add_sprite_sheet(sprite_sheet_name,
			std::move(sprite_sheet));
	}

	void RenderResources::add_font(const std::string& font_name,
		std::unique_ptr<Font> font)
	{
		this->impl_->add_font(font_name, std::move(font));
	}

	void RenderResources::release_device_resources()
	{
		this->impl_->release_all_textures();
	}

	TextureHandle RenderResources::resolve_texture(
		const std::string& texture_name) const
	{
		return TextureHandle(
			this->impl_->textures.resolve(texture_name).index());
	}

	FontHandle RenderResources::resolve_sprite_font(
		const std::string& font_name) const
	{
		return this->impl_->fonts.resolve(font_name);
	}

	RenderResources::SpriteSheetHandle RenderResources::resolve_sprite_sheet(
		const std::string& sprite_sheet_name) const
	{
		return this->impl_->sprite_sheets.resolve(sprite_sheet_name);
	}

	const SpriteSheet* RenderResources::sprite_sheet(
		SpriteSheetHandle sprite_sheet) const
	{
		return this->impl_->sprite_sheets.get(sprite_sheet);
	}

	const SpriteSheet* RenderResources::sprite_sheet(
		const std::string& sprite_sheet_name) const
	{
		return this->impl_->sprite_sheets.get(sprite_sheet_name);
	}

	Vector2F RenderResources::measure_text(FontHandle font,
		const std::wstring& text) const
	{
		return this->impl_->font(font)->measure(text);
	}

	bool RenderResources::can_render(FontHandle font,
		std::wstring_view text) const
	{
		return this->first_unrenderable(font, text) == std::wstring_view::npos;
	}

	size_t RenderResources::first_unrenderable(FontHandle font,
		std::wstring_view text) const
	{
		return this->impl_->font(font)->first_unrenderable(text);
	}
}
