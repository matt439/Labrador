#pragma once

#include "engine/core/handle.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

#include <memory>
#include <string>
#include <vector>
#include "engine/render/colour.h"
#include "engine/render/camera.h"
#include "engine/render/viewport.h"

// The renderer seam.
//
// WHAT THE SEAM IS FOR. Two things, and only two: headless tests, and an
// eventual second platform. Before this file, nine of ArtAttackEngine's
// translation units were untestable on one include - <SpriteBatch.h> at the
// top of engine/core/game_object.h - which is why tests/ has an assets, a
// core, a math, a collision and a ui folder and had no render one. Neither
// purpose needs two backends live in one process.
//
// CONCRETE CLASS, NOT AN ABSTRACT BASE. Renderer is a class with one
// implementation chosen at build time, not an interface with a vtable.
//   - T8 (PHILOSOPHY.md:136-148): a customisation point that taxes the frame
//     loop is a customisation point that goes. A virtual draw_sprite is that
//     tax in the module that draws thousands of paint tiles per frame, and it
//     is a *new* tax: the per-sprite cost is one out-of-line call that builds
//     four vertices and appends them to a batch, and a direct call through
//     this seam is the same shape. An indirect branch through a vtable, in
//     that loop, is not.
//   - T5: a compile-time choice fails at link, not at run time. Asking for a
//     backend that was not built is a missing symbol.
//   - If a real client ever needs runtime selection, promoting a concrete
//     class to an interface is mechanical and no call site changes. That
//     option is held, not spent - the same escalation ARCHITECTURE.md:122-126
//     describes for promoting a folder to a library.
//
// HOW ONE HEADER SERVES TWO BACKENDS. Renderer holds a pimpl and DrawList
// holds a raw pointer to per-view state the backend owns; each backend
// defines Renderer::Impl and DrawList::View in its own translation unit under
// engine/render/<backend>/. DrawList stays trivially copyable, so passing one
// costs nothing, and the per-draw cost is a single out-of-line call.
//
// WHAT IS DELIBERATELY ABSENT. No ID3D11* type, no DirectX:: type, no
// batch object, no sampler-state pointer, no device accessor. Creating a
// texture from a file is a resource factory's job, not a renderer's, and
// RenderResources already speaks in handles - so only the handle's payload
// type changes when the backend does.

namespace artattack
{
	// The table a DrawList resolves handles against. Declared, not included:
	// render_resources.h includes this file, for the handle types below.
	class RenderResources;

	// A handle is an index into the table that produced it (handle.h), and
	// these say which table.
	//
	// TEXTURE IS A PHANTOM AND FONT IS NOT, WHICH IS NOT AN INCONSISTENCY.
	// Texture never gains a definition: the backend decides what one is, and
	// nothing outside engine/render/<backend>/ needs to know, which is the
	// whole point of the seam. A Font was the same thing once and is not any
	// more - it is engine data over a TextureHandle (engine/render/font.h),
	// exactly as a SpriteSheet is, because a glyph table and a pen are
	// arithmetic and not a device resource. Declared and not included here so
	// that this file still costs a caller nothing.
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

	// CONSTRAINT: sampler state lives inside the seam.
	//
	// Two objects used to cache CommonStates::PointClamp() as a raw
	// ID3D11SamplerState* handed in at construction and hold it across device
	// loss, which frees the CommonStates that owns it. The fix is not to
	// document the loan. It is that the API never names a sampler object at
	// all: the caller says what it wants the filtering to look like and the
	// backend owns the state that produces it, recreated with the device like
	// everything else.
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
	// CONSTRAINT: the unit of work is a view, not an object.
	//
	// This is the single most important thing about the seam and the thing the
	// old code got wrong in two places. The render workers do NOT own disjoint
	// slices of the object list - the parallelism axis is views, so every
	// worker enters draw() on the SAME object at the same time. That is why
	// draw() is const, and it is why the const pass had to happen first.
	//
	// Level fanned out one worker per player and MenuPage fanned out one worker
	// per widget, indexing deferred contexts and sprite batches by widget
	// ordinal - which capped every menu at however many contexts the shell
	// happened to create. Two hand-written fan-outs, already diverged, one of
	// them wrong. There is one now, and it is here.
	//
	// A DrawList is obtained from Renderer::view(i) and is valid until the next
	// Renderer::submit(). It is a handle, not an owner: copying one is free and
	// copies refer to the same recording.
	class DrawList
	{
	public:
		DrawList() = default;

		// Whether this list can be drawn into. A default-constructed one cannot;
		// every list from Renderer::view() can.
		bool valid() const { return this->view_ != nullptr; }

		// Restricts subsequent draws to this viewport, in back-buffer pixels.
		// Absorbs ViewportManager::apply_player_viewport, whose three lines of
		// RSSetViewports/SetViewport were the only backend part of that class -
		// the layout arithmetic beside it is pure and stays where it is.
		void set_viewport(const Viewport& viewport);

		// World to view, for every draw recorded after it.
		//
		// DECIDED HERE, and it was the largest open question on this file: the
		// camera lives on the list rather than in every draw signature. Every
		// drawable used to take one and do the conversion itself, and the whole
		// of that conversion was nine lines across three files
		// (texture_object.cpp, text_object.cpp, animation_object.cpp) - every
		// other draw() in the engine and the game carried a Camera parameter
		// only to hand it down. Twenty override sites lost a parameter.
		//
		// It is per range and not per view because one view genuinely has two:
		// the paint-shooter draws the world through the player's camera and the
		// HUD over it in screen space, into the same viewport, on the same
		// frame. Changing it mid-list is therefore ordinary, not exceptional.
		//
		// The identity is Camera::DEFAULT_CAMERA, which is what a list starts
		// with, so a caller that only ever wanted screen space never mentions
		// a camera at all.
		void set_camera(const Camera& camera);

		// Applies to every draw recorded after it. Changing it mid-list is
		// legal and costs a flush, so group by filter if it matters.
		void set_filter(TextureFilter filter);

		// CONSTRAINT: sort depth is per draw, not per object.
		//
		// layer_depth is a parameter here for the same reason the tint and the
		// flip are: the same sprite drawn into two views at two depths must be
		// expressible. TextureObject::draw_with took every other varying
		// quantity as a local and then read this->layer_depth() off the shared
		// object on the last line, inside the function built to take locals.
		// Under the view fan-out that is the one member every worker reads
		// while another view wants a different value, and it is inexpressible
		// rather than merely racy.
		//
		// destination is in world space; the list's current camera maps it.
		void draw_sprite(TextureHandle texture,
			const mattmath::RectangleI& source,
			const mattmath::RectangleF& destination,
			const Colour& tint,
			float rotation,
			const mattmath::Vector2F& origin,
			SpriteFlip flip,
			float layer_depth);

		// CONSTRAINT: the text entry point is wide.
		//
		// It was DirectXTK that made this urgent - its narrow DrawString
		// converted through a utfBuffer owned by the shared SpriteFont, from a
		// const method, under a fan-out where every worker draws the whole HUD,
		// which is a data race and not a preference. That library is gone from
		// the font path and the rule stays, because the reason underneath it
		// never was DirectXTK's: a glyph table is keyed by code unit, so a
		// narrow overload is a conversion, and a conversion on the draw path is
		// either an allocation per string per view per frame or a buffer shared
		// between workers. Wide in, wide all the way down; there is no narrow
		// overload to fall back to and there should not be one.
		//
		// A wstring and not a wstring_view, which is what this said first, and
		// the reason has changed with the walk underneath it. Font::for_each_
		// glyph takes a view and would be happy with one - but every caller in
		// the tree holds a wstring, TextObject measures the same string it
		// stores, and a view here would buy nothing while inviting a caller to
		// hand over a temporary that outlives nothing.
		void draw_text(FontHandle font,
			const std::wstring& text,
			const mattmath::Vector2F& position,
			const Colour& tint,
			float scale,
			float rotation,
			const mattmath::Vector2F& origin,
			float layer_depth);

	private:
		friend class Renderer;

		// Per-view recording state. Defined by the backend, never by a caller.
		class View;

		explicit DrawList(View* view) : view_(view) {}

		View* view_ = nullptr;
	};

	// Told when the device goes away and comes back.
	//
	// Not a graphics type: a game object that has to rebuild something after a
	// device loss implements this and knows nothing about what was lost.
	class DeviceNotify
	{
	public:
		virtual void on_device_lost() = 0;
		virtual void on_device_restored() = 0;

	protected:
		~DeviceNotify() = default;
	};

	// The frame.
	//
	// One per process. Owns the device, the swap chain, the per-view recording
	// state, the sampler states, and the command-list lifetime - which is to
	// say, everything the game used to hand-write.
	class Renderer
	{
	public:
		Renderer();
		~Renderer();

		Renderer(Renderer&&) noexcept;
		Renderer& operator=(Renderer&&) noexcept;
		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		// The shell hands the renderer its window, and says how many views the
		// frame may ever fan out to - which sizes the recording state, because
		// a backend that records into per-thread contexts has to make them
		// before any frame starts.
		//
		// void* rather than HWND because this is the file a second backend
		// implements, and a window handle is not a graphics type: it is this
		// platform's window. The backend casts it back.
		void create_device(void* native_window, int width, int height,
			int view_capacity);

		// Returns whether anything was rebuilt, which is the signal the shell
		// wants for "re-run the layout".
		bool window_size_changed(int width, int height);

		// Borrowed; the shell owns it and outlives the renderer.
		void set_device_notify(DeviceNotify* device_notify);

		// The table the draw lists resolve handles against. Borrowed, like the
		// notify above.
		//
		// draw_sprite takes a TextureHandle, and the only thing that can turn
		// one back into whatever this backend calls a texture is the table that
		// issued it. The alternative was for the caller to resolve first and
		// hand the resolved resource in - which is the backend's own type, in a
		// game file's hands, which is the seam undone on the first call.
		//
		// Separate from create_device because the order is fixed the other way:
		// the resource factory needs a device before it can load anything into
		// a table, so the table cannot exist when the device is made.
		void set_resources(const RenderResources* resources);

		// begin_frame clears the back buffer and resets every view's
		// recording; end_frame presents.
		void begin_frame();
		void end_frame();

		// The scene declares how many views this frame has, then fills them.
		// Throws std::out_of_range above the capacity create_device was given,
		// and std::out_of_range for a view index outside the current count -
		// asking for a view nobody set up used to return a fullscreen pane and
		// draw a whole extra pass (T6).
		void set_view_count(int count);
		int view_count() const;
		DrawList view(int index) const;

		// CONSTRAINT: command-list lifetime is RAII and inside the seam.
		//
		// Records every view's list, executes them in view order and releases
		// them. This protocol - record, FinishCommandList, ExecuteCommandList,
		// Release - was hand-written in four places, each of which had to
		// pre-size a vector, pre-fill it with null and Release every non-null
		// entry, three caller obligations stated nowhere in the tree. Two of
		// the four already disagreed about RestoreContextState. There is one
		// copy now and it is not the caller's.
		//
		// Called once per frame, between begin_frame and end_frame.
		void submit();

		// Back-buffer size in pixels. Replaces DeviceResources::GetOutputSize
		// and GetScreenViewport, which is all the layout arithmetic in
		// engine/render/ ever wanted from the device.
		mattmath::Vector2F back_buffer_size() const;

		// Copies the back buffer out: 8-bit RGBA, row-major, top row first,
		// exactly width * height * 4 bytes. `pixels` is resized to fit.
		//
		// THIS IS WHAT MAKES THE FIRST PURPOSE AT THE TOP OF THIS FILE REAL.
		// The seam exists for headless tests and a second platform, and until
		// this method there was no way for a test to observe a single thing the
		// renderer had drawn - so every term of the pixel contract (what the
		// blend equation is, which way y runs, what `origin` is measured in,
		// what happens to a fractional destination) was decided by whichever
		// library the backend happened to call and written down nowhere. A seam
		// whose output nothing can read cannot be held to a contract, however
		// many backends fill it.
		//
		// RGBA REGARDLESS OF WHAT THE BACKEND STORES, so that the assertions a
		// second backend has to pass are the same bytes and not the same bytes
		// after a per-backend swizzle. This backend's buffer is BGRA and the
		// conversion happens here.
		//
		// BETWEEN submit() AND end_frame(). Presenting discards the back
		// buffer's contents, so after end_frame there is nothing left to read.
		// Not const: reading the GPU's memory means staging a copy through the
		// device, which is a write.
		//
		// NOT A FRAME-PATH CALL and not meant to become one (T8). It stalls on
		// the GPU by construction.
		void read_back_buffer(std::vector<unsigned char>& pixels);

		// Debug markers. The only capability of the backend's device wrapper
		// that reaches the seam unchanged - everything else a frame needs is
		// expressed above in the engine's own terms. The accessors nothing
		// called are not merely unexposed here; they are gone from the backend
		// (engine/render/<backend>/device_resources.h says which and why).
		void begin_marker(const wchar_t* name);
		void end_marker();
		void set_marker(const wchar_t* name);

		// The device, for the resource factory that has to create textures and
		// fonts against it. Declared in the backend's own header rather than
		// here, so that reaching for it is a deliberate include of
		// engine/render/<backend>/ and not something a game file can do by
		// accident.
		class Impl;
		Impl* impl() const { return this->impl_.get(); }

	private:
		std::unique_ptr<Impl> impl_;
	};
}

// STILL OPEN:
//
//  - The null backend. How CMake selects a backend is settled - the option is
//    ARTATTACK_RENDER_BACKEND and a backend is a folder of three translation
//    units - so adding engine/render/null/ is now a matter of writing it.
//    What it buys that the two real ones do not: RenderPixelTests needs a
//    driver on the GL side and WARP on the D3D11 one, and a null backend is
//    what would let a scene's drawing be asserted with neither. It would
//    record what it was asked to draw rather than drawing it, which the
//    engine-side geometry makes cheap: the vertices are already built before
//    a backend sees them.
//
// SETTLED, AND THIS FILE IS WHERE IT IS RECORDED:
//
//  - The shape of a backend is three translation units - renderer.cpp,
//    render_resources.cpp and texture_factory.cpp, all in
//    engine/render/<backend>/ - plus whatever that backend needs to build its
//    shader. The third is thirty lines: it turns already-decoded bytes into a
//    texture and adds it to the table. Path-building and file-reading are in
//    engine/render/resource_factory.cpp, written once for everybody.
//
//  - What is on which side of the line. Every decision that shows on screen is
//    the engine's: which glyph goes where (font.h), what a .dds and a
//    .spritefont say (dds_file.h, sprite_font_file.h), and where a sprite's
//    four corners land and what they sample (sprite_geometry.h). What a
//    backend supplies is a device, a texture from bytes (texture_data.h), a
//    vertex buffer, a shader that multiplies by two constants, and the states
//    that make the blend premultiplied. NOTHING A BACKEND DOES DECIDES WHERE A
//    PIXEL GOES, which is what lets two of them pass the same 128 assertions.
//
//  - The claim at the top of this file, that the seam serves "an eventual
//    second platform", is no longer a claim. engine/render/gl/ is OpenGL 3.3
//    core and passes RenderPixelTests, and writing it changed nothing above
//    the seam. The one thing it found: OpenGL has no deferred contexts, so a
//    view there records into memory and submit() replays it on the thread that
//    owns the context. The seam admitted that without a word of it reaching a
//    caller, because the unit of work is a view and a view's vertices are
//    already built on the CPU before any backend sees them.
//
//  - DirectXTK is no longer on the render path at all. It remains bought for
//    audio and for the gamepad reader, which are seams of their own and are
//    not this file's business.
