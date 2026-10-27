#include "samples/linesweeper/web/pacing_state.h"

#include "engine/app/application.h"
#include "engine/render/label.h"
#include "samples/linesweeper/presentation/arrangement.h"
#include "samples/linesweeper/presentation/board_view.h"
#include "samples/linesweeper/presentation/particles.h"
#include "samples/linesweeper/presentation/top_out_banner.h"
#include "samples/linesweeper/web/pacing_fixture.h"

#include <memory>
#include <utility>

namespace linesweeper
{
	PacingState::PacingState(labrador::Application* app, PacingSnapshot* snapshot,
		bool frozen) : app_(app), snapshot_(snapshot), world_(pacing_world()),
		frozen_(frozen)
	{
	}

	void PacingState::arrange_screen()
	{
		const Arrangement arrangement =
			arrange(this->app_->resolution_manager()->resolution_vec());
		this->scene_.clear_views();
		this->scene_.add_view(arrangement.viewport, arrangement.camera);
	}

	void PacingState::init()
	{
		labrador::RenderResources* const resources = this->app_->render_resources();
		std::ignore = this->scene_.add(std::make_unique<BoardView>(&this->world_, resources,
			&standard_palette));
		this->particles_ = this->scene_.add(std::make_unique<ParticleField>(
			&this->world_, &this->last_tick_, resources->resolve_texture("white"),
			&this->app_->step_position()));
		std::ignore = this->scene_.add(std::make_unique<TopOutBanner>(&this->world_, resources));
		std::ignore = this->scene_.add(std::make_unique<labrador::Label>(
			L"keyboard   arrows move   Z X rotate   space drops   C holds",
			"jetbrains_mono_bold_16", labrador::Vector2F(344.0f, landscape_height - 60.0f),
			resources, labrador::Colour::dark_gray));
		std::ignore = this->scene_.add(std::make_unique<labrador::Label>(
			L"pad        d-pad moves   A B rotate   Y drops       X holds",
			"jetbrains_mono_bold_16", labrador::Vector2F(344.0f, landscape_height - 36.0f),
			resources, labrador::Colour::dark_gray));
		this->scene_.end_tick();
		this->arrange_screen();
		this->scene_.update(0.0f);
		this->scene_.end_tick();
	}

	void PacingState::update(float dt)
	{
		this->arrange_screen();
		const std::uint64_t completed = this->app_->frame_timings().frame;
		if (this->snapshot_->top_out_frame == 0 && ++this->warmup_updates_ >= 300)
		{
			start_pacing_top_out(this->world_);
			this->snapshot_->top_out_frame = completed + 1;
		}

		// All catch-up updates before this tick's draw see the same completed
		// frame. Hold the birth positions until that first burst was rendered;
		// later evolving frames advance normally. Frozen mode never advances.
		const bool first_burst_pending = this->snapshot_->top_out_frame != 0 &&
			completed < this->snapshot_->top_out_frame;
		this->scene_.update(this->frozen_ || first_burst_pending ? 0.0f : dt);
		this->scene_.end_tick();
		if (this->snapshot_->top_out_frame != 0 && (first_burst_pending || this->frozen_))
		{
			require_pacing_particles(*this->particles_);
		}
		this->snapshot_->live_particles = this->particles_->live();
		this->snapshot_->dropped_particles = this->particles_->dropped();
	}

	void PacingState::draw(labrador::Renderer& renderer) const
	{
		this->scene_.draw(renderer);
	}
}
