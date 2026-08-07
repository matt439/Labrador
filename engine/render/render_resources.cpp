#include "engine/render/render_resources.h"

using namespace DirectX;

// Every getter here forwards to a Registry, so the three-part contract - the
// key must exist, the resource must not have been released by a reset_all_*
// call, and a failure must name what was missing - is written once rather than
// once per cache.

ID3D11ShaderResourceView* RenderResources::get_texture(
	const std::string& texture_name) const
{
	return this->_textures.get(texture_name);
}

void RenderResources::add_texture(const std::string& texture_name,
	ID3D11ShaderResourceView* texture)
{
	this->_textures.add(texture_name, texture);
}

SpriteFont* RenderResources::get_sprite_font(const std::string& font_name) const
{
	return this->_sprite_fonts.get(font_name);
}

void RenderResources::add_sprite_font(const std::string& font_name,
	std::unique_ptr<SpriteFont> font)
{
	this->_sprite_fonts.add(font_name, std::move(font));
}

SpriteSheet* RenderResources::get_sprite_sheet(
	const std::string& sprite_sheet_name) const
{
	return this->_sprite_sheets.get(sprite_sheet_name);
}

void RenderResources::add_sprite_sheet(const std::string& sprite_sheet_name,
	std::unique_ptr<SpriteSheet> sprite_sheet)
{
	this->_sprite_sheets.add(sprite_sheet_name, std::move(sprite_sheet));
}

void RenderResources::reset_all_sprite_fonts()
{
	this->_sprite_fonts.clear();
}

void RenderResources::reset_all_textures()
{
	this->_textures.clear();
}
