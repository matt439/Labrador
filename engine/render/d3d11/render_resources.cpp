#include "engine/render/d3d11/backend.h"

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

		Handle<SpriteFont> font_slot(FontHandle font)
		{
			return Handle<SpriteFont>(font.index());
		}
	}

	// --- RenderResources::Impl -----------------------------------------------

	void RenderResources::Impl::add_texture(const std::string& name,
		ID3D11ShaderResourceView* texture)
	{
		this->textures.add(name, texture);
	}

	void RenderResources::Impl::add_sprite_font(const std::string& name,
		std::unique_ptr<SpriteFont> font)
	{
		this->sprite_fonts.add(name, std::move(font));
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

	void RenderResources::Impl::release_all_sprite_fonts()
	{
		this->sprite_fonts.release_all();
	}

	ID3D11ShaderResourceView* RenderResources::Impl::texture(
		TextureHandle texture) const
	{
		return this->textures.get(texture_slot(texture));
	}

	SpriteFont* RenderResources::Impl::sprite_font(FontHandle font) const
	{
		return this->sprite_fonts.get(font_slot(font));
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
	// copy of this file, which is the same shape as renderer.cpp beside it.

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

	FontHandle RenderResources::resolve_sprite_font(
		const std::string& font_name) const
	{
		return FontHandle(this->impl_->sprite_fonts.resolve(font_name).index());
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
		const XMVECTOR size =
			this->impl_->sprite_font(font)->MeasureString(text.c_str());
		return { XMVectorGetX(size), XMVectorGetY(size) };
	}
}
