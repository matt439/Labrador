#include "engine/render/sprite_sheet.h"
#include "engine/assets/json_loader.h"
#include <stdexcept>

using namespace DirectX;
using namespace MattMath;
using namespace rapidjson;

SpriteSheet::SpriteSheet(ID3D11ShaderResourceView* texture) :
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

std::map<std::string, SpriteFrame>
	SpriteSheet::decode_sprite_frames_json(const rapidjson::Value& json)
{
	std::map<std::string, SpriteFrame> sprite_frames;
	for (auto& frame : json.GetArray())
	{
		std::string name = frame["name"].GetString();
		Vector2F origin = Vector2F::ZERO;
		bool rotated = false;
		RectangleI source_rectangle(
			frame["position"]["x"].GetInt(),
			frame["position"]["y"].GetInt(),
			frame["size"]["w"].GetInt(),
			frame["size"]["h"].GetInt());
		if (frame.HasMember("origin"))
		{
			origin.x = frame["origin"]["x"].GetFloat();
			origin.y = frame["origin"]["y"].GetFloat();
		}
		if (frame.HasMember("rotated"))
		{
			rotated = frame["rotated"].GetBool();
		}
		sprite_frames[name] = SpriteFrame(source_rectangle, origin,
			rotated);
	}
	return sprite_frames;
}

std::map<std::string, std::unique_ptr<AnimationStrip>>
	SpriteSheet::decode_animation_strips_json(const rapidjson::Value& json)
{
	std::map<std::string, std::unique_ptr<AnimationStrip>> animation_strips;
	for (auto& strip : json.GetArray())
	{
		std::string name = strip["name"].GetString();
		auto first_frame = RectangleI(
			strip["first_frame"]["x"].GetInt(),
			strip["first_frame"]["y"].GetInt(),
			strip["first_frame"]["w"].GetInt(),
			strip["first_frame"]["h"].GetInt());
		int frame_count = strip["frame_count"].GetInt();
		float frame_time = strip["frame_time"].GetFloat();
		bool looping = strip["looping"].GetBool();
		animation_strips[name] = std::make_unique<AnimationStrip>(
			first_frame, frame_count, frame_time, looping);
	}
	return animation_strips;
}

void SpriteSheet::load_from_json(const char* json_path)
{
	Document d = json_loader::parse_file(json_path);

	Value& sprite_frames = d["sprite_frames"];
	Value& animation_strips = d["animation_strips"];

	this->_sprite_frames = this->decode_sprite_frames_json(sprite_frames);
	this->_animation_strips =
		this->decode_animation_strips_json(animation_strips);

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
