#pragma once

#include "engine/core/handle.h"
#include "engine/math/matt_math.h"

#include <memory>
#include <string_view>

// The renderer seam: declarations and the reasoning behind them, and no
// implementation yet.
//
// This file exists before the work that fills it in because the seam is the
// only change in the plan measured in weeks, and a seam that misses a
// constraint gets cut twice. Everything below is either a decision that has
// been made or a question that is still open, and it says which.
//
// WHAT THE SEAM IS FOR. Two things, and only two: headless tests, and an
// eventual second platform. Today nine of ArtAttackEngine's translation units
// are untestable on one include - <SpriteBatch.h> at the top of
// engine/core/game_object.h - and tests/ has an assets, a core, a math and a
// collision folder because those are the modules that do not transitively
// reach it. Neither purpose needs two backends live in one process.
//
// CONCRETE CLASS, NOT AN ABSTRACT BASE. Renderer is a class with one
// implementation chosen at build time, not an interface with a vtable.
//   - T8 (PHILOSOPHY.md:136-148): a customisation point that taxes the frame
//     loop is a customisation point that goes. A virtual draw_sprite is that
//     tax in the module that draws thousands of paint tiles per frame, and it
//     is a *new* tax: today's per-sprite cost is one out-of-line call into
//     DirectXTK's SpriteBatch::Draw, and a direct call through this seam is
//     the same shape. An indirect branch through a vtable, in that loop, is
//     not.
//   - T5: a compile-time choice fails at link, not at run time. Asking for a
//     backend that was not built is a missing symbol.
//   - Both purposes are served by build-time selection: a null implementation
//     linked into tests/render/, and a second backend added as a sibling
//     folder under engine/render/.
//   - If a real client ever needs runtime selection, promoting a concrete
//     class to an interface is mechanical and no call site changes. That
//     option is held, not spent - the same escalation ARCHITECTURE.md:122-126
//     describes for promoting a folder to a library.
//
// HOW ONE HEADER SERVES TWO BACKENDS. Renderer holds a pimpl; each backend
// defines Renderer::Impl in its own translation unit under
// engine/render/<backend>/. DrawList is a non-owning handle to per-view state
// the backend owns, so it stays trivially copyable and passing one costs
// nothing. The per-draw cost is a single out-of-line call, which is what
// SpriteBatch::Draw already is.
//
// WHAT IS DELIBERATELY ABSENT. No ID3D11* type, no DirectX:: type, no
// SpriteBatch, no sampler-state pointer, no device accessor. GetD3DDevice is
// not a renderer concern - creating a texture from a file is a resource
// factory's job, and RenderResources already speaks in handles
// (render_resources.h:38), so only the handle's payload type changes when the
// backend does.
//
// DEFINITION OF DONE, AND IT IS A GREP.
//   1. samples/minimal/states/hello_state.cpp contains no ID3D11 identifier.
//   2. It contains no <SpriteBatch.h> and no DirectX:: name.
//   3. It contains no *this->app_->dt().
//   4. samples/minimal/CMakeLists.txt names no Microsoft::DirectXTK.
// The sample is 85 lines and it currently teaches Direct3D 11 rather than this
// engine. Port it first: it is the acceptance test for whether the seam is
// real, not the last thing to be updated once the engine compiles.

namespace artattack
{
	// Phantom payloads. A handle is an index into the table that produced it
	// (handle.h), and these say which table. The backend defines what a Texture
	// and a Font actually are; nothing outside engine/render/<backend>/ needs
	// to know, which is the whole point.
	class Texture;
	class Font;

	using TextureHandle = Handle<Texture>;
	using FontHandle = Handle<Font>;

	// Replaces DirectX::SpriteEffects across the sprite chain.
	enum class SpriteFlip
	{
		none,
		horizontal,
		vertical,
		both,
	};

	// CONSTRAINT 3: sampler state lives inside the seam.
	//
	// Today two objects cache CommonStates::PointClamp() as a raw
	// ID3D11SamplerState* handed in at construction (level.h:149,
	// level_builder.h:39) and hold it across device loss, which frees the
	// CommonStates that owns it. The fix is not to document the loan. It is
	// that the API never names a sampler object at all: the caller says what it
	// wants the filtering to look like and the backend owns the state that
	// produces it, recreated with the device like everything else.
	//
	// point is what the paint-shooter wants everywhere - it is a pixel-art
	// game and linear filtering is what makes tile seams shimmer.
	enum class TextureFilter
	{
		point,
		linear,
	};

	// A recording target for one view.
	//
	// CONSTRAINT 1: the unit of work is a view, not an object.
	//
	// This is the single most important thing about the seam and the thing the
	// current code gets wrong in two places. The render workers do NOT own
	// disjoint slices of the object list - the parallelism axis is views, so
	// every worker enters draw() on the SAME object at the same time. That is
	// why draw() is const, and it is why the const pass had to happen first.
	//
	// PHILOSOPHY.md:371-372 currently says workers own disjoint slices. It is
	// wrong, and E3 amends it - it is wrong about the destination, not just
	// about the present.
	//
	// Level fans out one worker per player (level.cpp:401 in the pre-A5 tree)
	// and MenuPage fans out one worker per widget (menu_page.cpp:143), indexing
	// deferred contexts and sprite batches by widget ordinal - which caps every
	// menu at however many contexts the shell happened to create. Two
	// hand-written fan-outs, already diverged, one of them wrong. After the seam
	// there is one, and it is here.
	//
	// A DrawList is obtained from Renderer::view(i) and is valid until the next
	// Renderer::submit(). It is a handle, not an owner: copying one is free and
	// copies refer to the same recording.
	class DrawList
	{
	public:
		// Restricts subsequent draws to this viewport, in back-buffer pixels.
		// Absorbs ViewportManager::apply_player_viewport, whose three lines of
		// RSSetViewports/SetViewport are the only backend part of that class -
		// camera_adjusted_player_viewport_rect and the layout arithmetic beside
		// it are pure and stay where they are.
		void set_viewport(const mattmath::Viewport& viewport);

		// Applies to every draw recorded after it. Changing it mid-list is
		// legal and costs a flush, so group by filter if it matters.
		void set_filter(TextureFilter filter);

		// CONSTRAINT 2: sort depth is per draw, not per object.
		//
		// layer_depth is a parameter here for the same reason the tint and the
		// flip are: the same sprite drawn into two views at two depths must be
		// expressible. TextureObject::draw_with (texture_object.cpp:92-111)
		// takes every other varying quantity as a local and then reads
		// this->layer_depth() off the shared object on the last line, inside
		// the function built to take locals. Under the view fan-out that is
		// the one member every worker reads while another view wants a
		// different value, and it is inexpressible rather than merely racy.
		//
		// This is PHILOSOPHY.md:367-370's parameter list, with the source
		// rectangle carried by the frame handle rather than passed separately,
		// because a sprite sheet frame is exactly a texture plus a source
		// rectangle and the two are resolved together.
		void draw_sprite(TextureHandle texture,
			const mattmath::RectangleI& source,
			const mattmath::RectangleF& destination,
			const mattmath::Colour& tint,
			float rotation,
			const mattmath::Vector2F& origin,
			SpriteFlip flip,
			float layer_depth);

		// CONSTRAINT 6: the text entry point is wide.
		//
		// Landed already, in A3. DirectXTK's narrow DrawString and
		// MeasureString convert through SpriteFont::Impl::ConvertUTF8, which
		// lazily allocates and may reallocate a utfBuffer owned by the shared
		// SpriteFont - from a const method, under a fan-out where every worker
		// draws the whole HUD. Wide in, wide all the way down; there is no
		// narrow overload to fall back to and there should not be one.
		void draw_text(FontHandle font,
			std::wstring_view text,
			const mattmath::Vector2F& position,
			const mattmath::Colour& tint,
			float scale,
			float rotation,
			const mattmath::Vector2F& origin,
			float layer_depth);
	};

	// The frame.
	//
	// One per process. Owns the device, the swap chain, the per-view recording
	// state, the sampler states, and the command-list lifetime - which is to
	// say, everything the game currently hand-writes.
	class Renderer
	{
	public:
		Renderer();
		~Renderer();

		Renderer(Renderer&&) noexcept;
		Renderer& operator=(Renderer&&) noexcept;
		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		// Absorbs Application::clear() (application.cpp:309-333) and
		// DeviceResources::Present(). begin_frame clears the back buffer and
		// resets every view's recording; end_frame presents.
		void begin_frame();
		void end_frame();

		// The scene declares how many views this frame has, then fills them.
		// The count comes off the game's view list, which is why Scene cannot
		// be built until it owns one explicitly (PLAN.md C1): today the count
		// is player_objects_->size(), each viewport comes off a Player and each
		// camera comes off the same Player, so folding the player list into a
		// single registered-object list would delete the renderer's only source
		// of view information.
		void set_view_count(int count);
		int view_count() const;
		DrawList view(int index) const;

		// CONSTRAINT 4: command-list lifetime is RAII and inside the seam.
		//
		// Records every view's list, executes them in view order and releases
		// them. Today this protocol - record, FinishCommandList,
		// ExecuteCommandList, Release - is hand-written in four places
		// (level.cpp:494 and :558, menu_page.cpp:120 and :164-165,
		// hello_state.cpp:77-84), each of which must pre-size a vector,
		// pre-fill it with null and Release every non-null entry, three caller
		// obligations stated nowhere in the tree. Two of the four already
		// disagree about RestoreContextState (TRUE in one place, FALSE in
		// another). After this there is one copy and it is not the caller's.
		//
		// Called once per frame, between begin_frame and end_frame.
		void submit();

		// CONSTRAINT 5: font metrics are an engine type, answerable headlessly.
		//
		// TextObject's *constructor* measures (text_object.cpp:25 -> :112-118),
		// so text is unconstructible without a device, not merely undrawable -
		// which is why the countdown box is a hardcoded 400x600 guess
		// (level.h:40-41) sitting 94px off centre while the real measurement
		// exists and nothing reads it (E2).
		//
		// The null backend answers this from font metrics read off disk. That
		// is the whole reason it is on Renderer rather than behind the device:
		// a test that constructs a TextObject and asserts on its bounds must
		// work, and today it cannot.
		mattmath::Vector2F measure(FontHandle font,
			std::wstring_view text) const;

		// Back-buffer size in pixels. Replaces DeviceResources::GetOutputSize
		// and GetScreenViewport, which is all the layout arithmetic in
		// engine/render/ ever wanted from the device.
		mattmath::Vector2F back_buffer_size() const;

		// Debug markers. The only three of DeviceResources' seventeen graphics
		// accessors that survive into the seam unchanged; twelve of the other
		// fourteen have no caller anywhere in the repository.
		void begin_marker(const wchar_t* name);
		void end_marker();
		void set_marker(const wchar_t* name);

	private:
		class Impl;
		std::unique_ptr<Impl> impl_;
	};
}

// STILL OPEN, and B1 decides each of these against the sample first:
//
//  - Where TextObject gets its measurement. measure() needs a Renderer, and
//    TextObject measures in its constructor. Either the constructor takes a
//    Renderer&, or measurement moves to first use and text_bounds() stops
//    being free. The second is a per-cull cost on the path that culls, so it
//    is probably the first - but the sample is what proves it.
//
//  - Whether set_view_count/view(i) is the right shape, or whether a view is a
//    value the scene hands in (viewport plus camera) and DrawList comes back.
//    The second reads better and makes the view list explicit, which is what
//    C1 needs anyway. Settle it while porting MenuPage, which is the caller
//    that currently gets the unit of work wrong.
//
//  - Where the camera goes. Every draw today takes a Camera and converts world
//    to view coordinates inside the drawable. That conversion is per-sprite
//    arithmetic the seam could own once per view instead, which would remove
//    Camera from every draw signature in the engine. It is the largest
//    remaining simplification and it is not free to get wrong, so it is filed
//    here rather than assumed.
//
//  - The null backend's home and how CMake selects it. engine/render/null/
//    linked into tests/render/, or an option on ArtAttackEngine. The first
//    keeps one library configuration and is probably right.
