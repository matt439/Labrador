#pragma once

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
struct RegistryStorage<Microsoft::WRL::ComPtr<Resource>>
{
	static Resource* pointer(const Microsoft::WRL::ComPtr<Resource>& storage)
	{
		return storage.Get();
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
	// A name that has been resolved. Drawables store these, not names: a
	// handle is an index, so reading through one costs a bounds check and a
	// load, where a name costs a std::map descent and a string compare per
	// node - per draw, per drawable, from every render worker.
	using TextureHandle = Handle<ID3D11ShaderResourceView>;
	using FontHandle = Handle<DirectX::SpriteFont>;
	using SpriteSheetHandle = Handle<SpriteSheet>;

	RenderResources() = default;

	// Load-time. Each throws std::out_of_range naming the resource if nothing
	// has loaded it, so a misspelt name in a definition file fails while the
	// level is loading rather than on the frame that first drew it.
	//
	// A handle outlives a device loss. It names a slot, and reload_device_
	// resources() refills slots rather than making new ones, so a drawable
	// resolved before the loss draws the right thing after it - which a cached
	// ID3D11ShaderResourceView* or SpriteFont* would not, both being remade.
	TextureHandle resolve_texture(const std::string& texture_name) const;
	FontHandle resolve_sprite_font(const std::string& font_name) const;
	SpriteSheetHandle resolve_sprite_sheet(
		const std::string& sprite_sheet_name) const;

	// Per-draw, and const and non-mutating because they are called from
	// thread-pool workers. Each throws std::out_of_range if the handle is
	// unresolved or its slot has been released.
	ID3D11ShaderResourceView* get_texture(TextureHandle texture) const;
	DirectX::SpriteFont* get_sprite_font(FontHandle font) const;
	SpriteSheet* get_sprite_sheet(SpriteSheetHandle sprite_sheet) const;

	// The by-name reads, for load-time callers that have a name and want the
	// resource now - the loader seating a texture into its sheet, say. Nothing
	// on the draw path may use these.
	ID3D11ShaderResourceView* get_texture(const std::string& texture_name) const;
	DirectX::SpriteFont* get_sprite_font(const std::string& font_name) const;
	SpriteSheet* get_sprite_sheet(const std::string& sprite_sheet_name) const;

	void add_texture(const std::string& texture_name,
		ID3D11ShaderResourceView* texture);

	void add_sprite_font(const std::string& font_name,
		std::unique_ptr<DirectX::SpriteFont> font);

	void add_sprite_sheet(const std::string& sprite_sheet_name,
		std::unique_ptr<SpriteSheet> sprite_sheet);

	// Device loss. The resources go; the names, and therefore every handle
	// resolved from them, stay. Reading one before the reload refills it
	// throws saying it was released.
	void reset_all_sprite_fonts();
	void reset_all_textures();

private:
	Registry<ID3D11ShaderResourceView,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textures_{ "Texture" };

	Registry<DirectX::SpriteFont> sprite_fonts_{ "SpriteFont" };
	Registry<SpriteSheet> sprite_sheets_{ "SpriteSheet" };
};
