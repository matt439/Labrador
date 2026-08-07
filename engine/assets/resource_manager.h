#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include "engine/assets/registry.h"
#include "engine/audio/sound_bank.h"
#include "engine/render/sprite_sheet.h"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <SpriteFont.h>
#include <memory>
#include <string>

// COM spells the accessor Get(), so teach the registry to unwrap a ComPtr
// here, where COM is already in scope.
template <typename Resource>
struct RegistryHandle<Microsoft::WRL::ComPtr<Resource>>
{
	static Resource* pointer(const Microsoft::WRL::ComPtr<Resource>& handle)
	{
		return handle.Get();
	}
};

// The engine's asset cache: named resources in, borrowed pointers out.
//
// Every kind is a Registry, so all of them share one lookup contract and the
// caches differ only in what they hold. A game with resource types of its own
// keeps them in its own Registry rather than here - the engine loads, caches
// and hands back; what a resource means is the game's business.
class ResourceManager
{
public:
	ResourceManager() = default;

	// The getters are const and non-mutating: they are called per-draw from
	// thread-pool workers, so they must not touch the maps. Each throws
	// std::out_of_range naming the resource if it is absent or released.
	ID3D11ShaderResourceView* get_texture(const std::string& texture_name) const;

	void add_texture(const std::string& texture_name,
		ID3D11ShaderResourceView* texture);

	DirectX::SpriteFont* get_sprite_font(const std::string& font_name) const;

	void add_sprite_font(const std::string& font_name,
		std::unique_ptr<DirectX::SpriteFont> font);

	SpriteSheet* get_sprite_sheet(const std::string& sprite_sheet_name) const;

	void add_sprite_sheet(const std::string& sprite_sheet_name,
		std::unique_ptr<SpriteSheet> sprite_sheet);

	void add_sound_bank(const std::string& sound_bank_name,
		std::unique_ptr<SoundBank> sound_bank);

	SoundBank* get_sound_bank(const std::string& sound_bank_name) const;

	void reset_all_sprite_fonts();
	void reset_all_textures();
	void reset_all_sounds();

private:
	Registry<ID3D11ShaderResourceView,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> _textures{ "Texture" };

	Registry<DirectX::SpriteFont> _sprite_fonts{ "SpriteFont" };
	Registry<SpriteSheet> _sprite_sheets{ "SpriteSheet" };
	Registry<SoundBank> _sound_banks{ "SoundBank" };
};
#endif // !RESOURCE_MANAGER_H
