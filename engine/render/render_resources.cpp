#include "engine/render/render_resources.h"

using namespace DirectX;

// Every accessor here forwards to a Registry, so the contract - resolve once
// by name, read many times by handle, and fail loudly naming what was missing
// either way - is written once rather than once per cache.

RenderResources::TextureHandle RenderResources::resolve_texture(
	const std::string& texture_name) const
{
	return this->textures_.resolve(texture_name);
}

RenderResources::FontHandle RenderResources::resolve_sprite_font(
	const std::string& font_name) const
{
	return this->sprite_fonts_.resolve(font_name);
}

RenderResources::SpriteSheetHandle RenderResources::resolve_sprite_sheet(
	const std::string& sprite_sheet_name) const
{
	return this->sprite_sheets_.resolve(sprite_sheet_name);
}

ID3D11ShaderResourceView* RenderResources::texture(
	TextureHandle texture) const
{
	return this->textures_.get(texture);
}

SpriteFont* RenderResources::sprite_font(FontHandle font) const
{
	return this->sprite_fonts_.get(font);
}

SpriteSheet* RenderResources::sprite_sheet(
	SpriteSheetHandle sprite_sheet) const
{
	return this->sprite_sheets_.get(sprite_sheet);
}

ID3D11ShaderResourceView* RenderResources::texture(
	const std::string& texture_name) const
{
	return this->textures_.get(texture_name);
}

SpriteFont* RenderResources::sprite_font(const std::string& font_name) const
{
	return this->sprite_fonts_.get(font_name);
}

SpriteSheet* RenderResources::sprite_sheet(
	const std::string& sprite_sheet_name) const
{
	return this->sprite_sheets_.get(sprite_sheet_name);
}

void RenderResources::add_texture(const std::string& texture_name,
	ID3D11ShaderResourceView* texture)
{
	this->textures_.add(texture_name, texture);
}

void RenderResources::add_sprite_font(const std::string& font_name,
	std::unique_ptr<SpriteFont> font)
{
	this->sprite_fonts_.add(font_name, std::move(font));
}

void RenderResources::add_sprite_sheet(const std::string& sprite_sheet_name,
	std::unique_ptr<SpriteSheet> sprite_sheet)
{
	this->sprite_sheets_.add(sprite_sheet_name, std::move(sprite_sheet));
}

void RenderResources::reset_all_sprite_fonts()
{
	this->sprite_fonts_.release_all();
}

void RenderResources::reset_all_textures()
{
	this->textures_.release_all();
}
