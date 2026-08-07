#include "samples/minimal/states/hello_state.h"

#include <SpriteBatch.h>

using namespace DirectX;
using namespace MattMath;

namespace
{
	// Asset names, matching content/manifest.json. They are spelt once and
	// resolved to handles once (PHILOSOPHY T7, T8) - nothing below looks a
	// resource up by name per frame.
	const std::string font_name = "courier_new_bold_16";

	constexpr float move_speed = 400.0f;
}

HelloState::HelloState(Application* app) : _app(app)
{

}

void HelloState::init()
{
	const Vector2F resolution =
		this->_app->resolution_manager()->get_resolution_vec();
	this->_position = Vector2F(resolution.x * 0.5f - 120.0f,
		resolution.y * 0.5f);

	RenderResources* resources = this->_app->render_resources();

	this->_greeting = std::make_unique<Text>("Hello from the engine.",
		font_name, this->_position, resources, colour_consts::WHITE);

	this->_hint = std::make_unique<Text>(
		"Left stick moves it. B quits.", font_name,
		Vector2F(24.0f, resolution.y - 40.0f), resources,
		colour_consts::DARK_GRAY);
}

void HelloState::update()
{
	const GamePad::State pad = this->_app->gamepad()->GetState(0);
	if (pad.IsConnected())
	{
		if (pad.IsBPressed())
		{
			this->_app->quit();
			return;
		}

		// dt is a pointer because the objects reading it outlive any one
		// frame; the shell rewrites the value behind it every fixed step.
		const float dt = *this->_app->dt();
		this->_position.x += pad.thumbSticks.leftX * move_speed * dt;
		this->_position.y -= pad.thumbSticks.leftY * move_speed * dt;
		this->_greeting->set_position(this->_position);
	}
}

void HelloState::draw()
{
	// Drawing goes through a deferred context so it can be recorded off the
	// main thread; with one string there is nothing to spread, so this uses
	// worker 0 and executes its list immediately. A game with a scene's worth
	// of objects fans the same shape across every context the shell created.
	DX::DeviceResources* device = this->_app->device_resources();
	ID3D11DeviceContext* deferred = device->get_deferred_context(0);
	SpriteBatch* sprite_batch = this->_app->sprite_batches()->at(0);

	sprite_batch->Begin(SpriteSortMode_Deferred);
	this->_greeting->draw(sprite_batch);
	this->_hint->draw(sprite_batch);
	sprite_batch->End();

	ID3D11CommandList* commands = nullptr;
	if (FAILED(deferred->FinishCommandList(FALSE, &commands)))
	{
		throw std::runtime_error("HelloState::draw - FinishCommandList failed");
	}

	device->GetD3DDeviceContext()->ExecuteCommandList(commands, FALSE);
	commands->Release();
}
