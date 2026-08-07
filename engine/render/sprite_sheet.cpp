#include "engine/render/sprite_sheet.h"

using namespace DirectX;
using namespace mattmath;

SpriteSheet::SpriteSheet(ID3D11ShaderResourceView* texture,
	NameTable<SpriteFrame> sprite_frames,
	NameTable<AnimationStrip> animation_strips) :
	sprite_frames_(std::move(sprite_frames)),
	animation_strips_(std::move(animation_strips)),
	texture_(texture)
{

}

void SpriteSheet::set_texture(ID3D11ShaderResourceView* texture)
{
	this->texture_ = texture;
}

SpriteSheet::frame_handle SpriteSheet::resolve_sprite_frame(
	const std::string& name) const
{
	return this->sprite_frames_.resolve(name);
}

SpriteSheet::strip_handle SpriteSheet::resolve_animation_strip(
	const std::string& name) const
{
	return this->animation_strips_.resolve(name);
}

const SpriteFrame& SpriteSheet::get_sprite_frame(frame_handle frame) const
{
	return this->sprite_frames_.get(frame);
}

const AnimationStrip& SpriteSheet::get_animation_strip(strip_handle strip) const
{
	return this->animation_strips_.get(strip);
}

void SpriteSheet::draw(SpriteBatch* sprite_batch,
	frame_handle frame,
	const Vector2F& position,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	float scale,
	SpriteEffects effects,
	float layer_depth) const
{
	sprite_batch->Draw(
		this->texture_,
		position.get_xm_vector(),
		this->get_sprite_frame(frame).get_source_rectangle(),
		color.get_xm_vector(),
		rotation,
		origin.get_xm_vector(),
		scale,
		effects,
		layer_depth);
}

void SpriteSheet::draw(SpriteBatch* sprite_batch,
	frame_handle frame,
	const RectangleI& destination_rectangle,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) const
{
	sprite_batch->Draw(
		this->texture_,
		destination_rectangle.get_win_rect(),
		this->get_sprite_frame(frame).get_source_rectangle(),
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
		this->texture_,
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
		this->texture_,
		destination_rectangle.get_win_rect(),
		source_rect,
		color.get_xm_vector(),
		rotation,
		origin.get_xm_vector(),
		effects,
		layer_depth);
}
