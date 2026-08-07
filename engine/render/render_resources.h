#ifndef RENDER_RESOURCES_H
#define RENDER_RESOURCES_H

#include "engine/core/registry.h"
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

// Everything drawing reads from, cached by name: the textures the GPU holds,
// the fonts built from them, and the sheets that index them. Named resources
// in, borrowed pointers out - the loader that fills it decides what a name
// means and where the bytes came from.
//
// Not to be confused with DX::DeviceResources, which owns the device and swap
// chain themselves.
class RenderResources
{
public:
	RenderResources() = default;

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

	void reset_all_sprite_fonts();
	void reset_all_textures();

private:
	Registry<ID3D11ShaderResourceView,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> _textures{ "Texture" };

	Registry<DirectX::SpriteFont> _sprite_fonts{ "SpriteFont" };
	Registry<SpriteSheet> _sprite_sheets{ "SpriteSheet" };
};
#endif // !RENDER_RESOURCES_H
