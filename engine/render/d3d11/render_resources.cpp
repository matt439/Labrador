#include "engine/render/d3d11/backend.h"
#include "engine/math/vector2f.h"

#include <memory>
#include <string>

using namespace DirectX;
using namespace mattmath;

namespace artattack
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

	ID3D11ShaderResourceView* RenderResources::Impl::texture(
		TextureHandle texture) const
	{
		return this->textures.get(texture_slot(texture));
	}

	// No slot reinterpretation: a Font is what the public handle already names,
	// which is what stopped being true of a texture and never was of a sheet.
	const Font* RenderResources::Impl::font(FontHandle font) const
	{
		return this->fonts.get(font);
	}

	ID3D11ShaderResourceView* RenderResources::Impl::texture(
		const std::string& name) const
	{
		return this->textures.get(name);
	}

	// --- RenderResources -----------------------------------------------------
	//
	// The front half is backend-free logic, but it is defined here because Impl
	// is only a complete type in this folder. A second backend supplies its own
	// copy of this file, which is the same shape as renderer.cpp beside it -
	// and it is a thinner copy than it was, because the three font queries below
	// now forward to arithmetic in engine/render/font.cpp rather than performing
	// any.

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

	// Textures, and this backend has no second kind. The fonts and the sheets
	// stay: each one's device half is a texture handle, and that slot is
	// refilled by the reload rather than remade.
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
