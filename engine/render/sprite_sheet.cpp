#include "engine/render/sprite_sheet.h"
#include <stdexcept>

using namespace DirectX;
using namespace MattMath;

SpriteSheet::SpriteSheet(ID3D11ShaderResourceView* texture,
	std::map<std::string, SpriteFrame> sprite_frames,
	std::map<std::string, std::unique_ptr<AnimationStrip>> animation_strips) :
	_sprite_frames(std::move(sprite_frames)),
	_animation_strips(std::move(animation_strips)),
	_texture(texture)
{

}

void SpriteSheet::set_texture(ID3D11ShaderResourceView* texture)
{
	this->_texture = texture;
}

const AnimationStrip* SpriteSheet::get_animation_strip(const std::string& name) const
{
	const auto it = this->_animation_strips.find(name);
	if (it == this->_animation_strips.end())
	{
		throw std::out_of_range(
			"SpriteSheet::get_animation_strip - no strip named '" + name + "'");
	}
	return it->second.get();
}

const SpriteFrame& SpriteSheet::get_sprite_frame(const std::string& name) const
{
	const auto it = this->_sprite_frames.find(name);
	if (it == this->_sprite_frames.end())
	{
		throw std::out_of_range(
			"SpriteSheet::get_sprite_frame - no frame named '" + name + "'");
	}
	return it->second;
}

void SpriteSheet::draw(SpriteBatch* sprite_batch,
	const std::string& frame_name,
	const Vector2F& position,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	float scale,
	SpriteEffects effects,
	float layer_depth) const
{
	sprite_batch->Draw(
		this->_texture,
		position.get_xm_vector(),
		this->get_sprite_frame(frame_name).get_source_rectangle(),
		color.get_xm_vector(),
		rotation,
		origin.get_xm_vector(),
		scale,
		effects,
		layer_depth);
}

void SpriteSheet::draw(SpriteBatch* sprite_batch,
	const std::string& frame_name,
	const RectangleI& destination_rectangle,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) const
{
	sprite_batch->Draw(
		this->_texture,
		destination_rectangle.get_win_rect(),
		this->get_sprite_frame(frame_name).get_source_rectangle(),
		color.get_xm_vector(),
		rotation,
		origin.get_xm_vector(),
		effects,
		layer_depth);
}

void SpriteSheet::draw(SpriteBatch* sprite_batch,
	const RECT* source_rect,
	const Vector2F& position,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	float scale,
	SpriteEffects effects,
	float layer_depth) const
{
	sprite_batch->Draw(
		this->_texture,
		position.get_xm_vector(),
		source_rect,
		color.get_xm_vector(),
		rotation,
		origin.get_xm_vector(),
		scale,
		effects,
		layer_depth);
}

void SpriteSheet::draw(SpriteBatch* sprite_batch,
	const RECT* source_rect,
	const RectangleI& destination_rectangle,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) const
{
	sprite_batch->Draw(
		this->_texture,
		destination_rectangle.get_win_rect(),
		source_rect,
		color.get_xm_vector(),
		rotation,
		origin.get_xm_vector(),
		effects,
		layer_depth);
}
