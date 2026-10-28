#pragma once

#include "engine/math/rectanglef.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"

namespace components
{
	// Copyable game-owned playback policy over an engine-owned strip.
	// Resources must outlive this object; the sheet/strip definition must stay
	// unchanged. Texture device-loss reloads preserve that definition and pair.
	class AnimationPreview
	{
	public:
		// Resolve with resources.resolve_sheet_strip(sheet_name, strip_name).
		// The pair must belong to resources. Throws std::out_of_range for an
		// unresolved pair, or std::invalid_argument for an empty strip or a
		// nonpositive/nonfinite frame duration.
		AnimationPreview(const labrador::RenderResources& resources,
			labrador::RenderResources::SheetStrip strip);

		// Rejects negative/nonfinite dt before changing state; zero is a no-op.
		// Loops wrap; one-shots finish after the last frame's full duration and
		// hold that frame. Large steps take bounded work, not one step per frame.
		void update(float dt);
		void restart();

		// Elapsed seconds within this cycle, or clamped to the one-shot duration.
		double elapsed() const;
		int frame_index() const;
		bool finished() const;

		// Several views may draw this same state. Drawing never advances time.
		void draw(labrador::DrawList& draw_list,
			const labrador::RectangleF& destination,
			const labrador::Colour& colour = labrador::Colour::white) const;

	private:
		const labrador::RenderResources* resources_;
		labrador::RenderResources::SheetStrip strip_;
		int frame_count_ = 0;
		double frame_time_ = 0.0;
		double duration_ = 0.0;
		bool looping_ = false;
		double elapsed_ = 0.0;
		int frame_index_ = 0;
		bool finished_ = false;
	};
}
