#include "samples/minimal/states/hello_state.h"

#include <SpriteBatch.h>

using namespace DirectX;
using namespace mattmath;
using namespace artattack;

namespace
{
	// Asset names, matching content/manifest.json. They are spelt once and
	// resolved to handles once (PHILOSOPHY T7, T8) - nothing below looks a
	// resource up by name per frame.
	const std::string font_name = "courier_new_bold_16";

	constexpr float move_speed = 400.0f;
}

HelloState::HelloState(Application* app) : app_(app)
{

}

void HelloState::init()
{
	const Vector2F resolution =
		this->app_->resolution_manager()->resolution_vec();
	this->position_ = Vector2F(resolution.x * 0.5f - 120.0f,
		resolution.y * 0.5f);

	RenderResources* resources = this->app_->render_resources();

	this->greeting_ = std::make_unique<Text>(L"Hello from the engine.",
		font_name, this->position_, resources, colour_consts::WHITE);

	this->hint_ = std::make_unique<Text>(
		L"Left stick moves it. B quits.", font_name,
		Vector2F(24.0f, resolution.y - 40.0f), resources,
		colour_consts::DARK_GRAY);
}

void HelloState::update()
{
	const GamePad::State pad = this->app_->gamepad()->GetState(0);
	if (pad.IsConnected())
	{
		if (pad.IsBPressed())
		{
			this->app_->quit();
			return;
		}

		// dt is a pointer because the objects reading it outlive any one
		// frame; the shell rewrites the value behind it every fixed step.
		const float dt = *this->app_->dt();
		this->position_.x += pad.thumbSticks.leftX * move_speed * dt;
		this->position_.y -= pad.thumbSticks.leftY * move_speed * dt;
		this->greeting_->set_position(this->position_);
	}
}

void HelloState::draw()
{
	// Drawing goes through a deferred context so it can be recorded off the
	// main thread; with one string there is nothing to spread, so this uses
	// worker 0 and executes its list immediately. A game with a scene's worth
	// of objects fans the same shape across every context the shell created.
	artattack::DeviceResources* device = this->app_->device_resources();
	ID3D11DeviceContext* deferred = device->deferred_context(0);
	SpriteBatch* sprite_batch = this->app_->sprite_batches()->at(0);

	sprite_batch->Begin(SpriteSortMode_Deferred);
	this->greeting_->draw(sprite_batch);
	this->hint_->draw(sprite_batch);
	sprite_batch->End();

	ID3D11CommandList* commands = nullptr;
	if (FAILED(deferred->FinishCommandList(FALSE, &commands)))
	{
		throw std::runtime_error("HelloState::draw - FinishCommandList failed");
	}

	device->GetD3DDeviceContext()->ExecuteCommandList(commands, FALSE);
	commands->Release();
}
