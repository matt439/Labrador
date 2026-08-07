#ifndef SPRITESHEET_H
#define SPRITESHEET_H

#include "engine/render/sprite_frame.h"
#include "engine/math/colour.h"
#include "SpriteBatch.h"
#include "rapidjson/document.h"
#include <map>
#include "engine/render/animation_strip.h"

class SpriteSheet
{	
public:
	explicit SpriteSheet(ID3D11ShaderResourceView* texture);

	const AnimationStrip* get_animation_strip(const std::string& name) const;

	// Throws std::out_of_range naming the frame if it is not in the sheet.
	const SpriteFrame& get_sprite_frame(const std::string& name) const;

	void load_from_json(const char* json_path);

	// Points the sheet at a newly created texture after a device restore. The
	// frame and animation-strip tables are device-independent and survive, so
	// every cached SpriteSheet* and AnimationStrip* stays valid.
	void set_texture(ID3D11ShaderResourceView* texture);

	// Every draw overload is const: a single SpriteSheet is shared by every
	// drawable in the level and is entered concurrently by the render workers,
	// so nothing here may mutate the frame or strip maps.
	void draw(DirectX::SpriteBatch* sprite_batch,
		const std::string& frame_name,
		const MattMath::Vector2F& position,
		const MattMath::Colour& color = colour_consts::WHITE,
		float rotation = 0.0f,
		const MattMath::Vector2F& origin =
			MattMath::Vector2F::ZERO,
		float scale = 1.0f,
		DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
		float layer_depth = 0.0f) const;

	void draw(DirectX::SpriteBatch* sprite_batch,
		const std::string& frame_name,
		const MattMath::RectangleI& destination_rectangle,
		const MattMath::Colour& color = colour_consts::WHITE,
		float rotation = 0.0f,
		const MattMath::Vector2F& origin =
			MattMath::Vector2F::ZERO,
		DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
		float layer_depth = 0.0f) const;

	void draw(DirectX::SpriteBatch* sprite_batch,
		const RECT* source_rect,
		const MattMath::Vector2F& position,
		const MattMath::Colour& color = colour_consts::WHITE,
		float rotation = 0.0f,
		const MattMath::Vector2F& origin =
		MattMath::Vector2F::ZERO,
		float scale = 1.0f,
		DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
		float layer_depth = 0.0f) const;

	void draw(DirectX::SpriteBatch* sprite_batch,
		const RECT* source_rect,
		const MattMath::RectangleI& destination_rectangle,
		const MattMath::Colour& color = colour_consts::WHITE,
		float rotation = 0.0f,
		const MattMath::Vector2F& origin =
		MattMath::Vector2F::ZERO,
		DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
		float layer_depth = 0.0f) const;
private:
	std::map<std::string, SpriteFrame> _sprite_frames;
	std::map<std::string, std::unique_ptr<AnimationStrip>> _animation_strips;
	ID3D11ShaderResourceView* _texture = nullptr;

	static std::map<std::string, SpriteFrame>
		decode_sprite_frames_json(const rapidjson::Value& json);
	static std::map<std::string, std::unique_ptr<AnimationStrip>>
		decode_animation_strips_json(const rapidjson::Value& json);
};
#endif // !SPRITESHEET_H
