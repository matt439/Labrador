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
// eventual second platform. Before this file, nine of LabradorEngine's
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
//     option is held, not spent - the same escalation ARCHITECTURE.md
//     describes under Modules, "Promoting a concrete class to an interface",
//     for promoting a folder to a library. Cited by section rather than by
//     line, because the line moved once already.
//
// HOW ONE HEADER SERVES FIVE BACKENDS. Renderer holds a pimpl and DrawList
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

namespace labrador
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
		//
		// IT CHOOSES BETWEEN TEXELS OF LEVEL ZERO AND NEVER BETWEEN LEVELS.
		// A minified draw samples level zero however many levels the texture
		// carries, under either filter. That is a term of this seam and not a
		// backend's habit: the two answered it differently - one sampled the
		// chain, the other did not - and agreed in practice only because no
		// file in either client has ever carried a second level.
		//
		// LEVEL ZERO IS THE ANSWER BECAUSE THE OTHER ONE IS NOT THE ENGINE'S TO
		// GIVE. A mip level is chosen per pixel from screen-space derivatives,
		// and both APIs let an implementation approximate that computation, so
		// a chain would put "which texel" in the same class as the things the
		// closing section of this file says a backend never decides. Chains are
		// read and uploaded - a .dds that has one is not rejected and its bytes
		// reach the device - and nothing samples from them.
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
		// void* rather than HWND because this is the file every backend
		// implements, and a window handle is not a graphics type: it is this
		// platform's window. The backend casts it back - or, in the null
		// backend's case, ignores it, which is what lets a client run on a
		// machine with no display.
		void create_device(void* native_window, int width, int height,
			int view_capacity);

		// Returns whether anything was rebuilt, which is the signal the shell
		// wants for "re-run the layout".
		//
		// CONSTRAINT: IT MAY ARRIVE IN THE MIDDLE OF A FRAME, AND A FRAME IN
		// PROGRESS IS RESTARTED RATHER THAN REFUSED.
		//
		// This used to say nothing, and three backends answered it three ways
		// - which was every backend with a buffer to rebuild when the rule was
		// written, the null one having landed five days earlier and having
		// nothing to answer with:
		// OpenGL carried on, because its default framebuffer follows the window
		// and there is nothing to rebuild; D3D11 threw DXGI_ERROR_INVALID_CALL
		// out of ResizeBuffers, because its views' deferred contexts still held
		// the render target; and D3D12 destroyed the back buffer and then
		// executed command lists that still named it, which is a dead process
		// rather than an error. One question, one answer that was fine, one
		// that was loud and one that was fatal.
		//
		// IT IS NOT A RULE THE CALLER COULD KEEP EVEN IF THIS FILE STATED ONE,
		// AND THE MECHANISM IS SYNCHRONOUS RATHER THAN PUMPED. A resize reaches
		// the shell as a window message, and a window message can be SENT -
		// straight to the window procedure, on the calling thread, with no
		// queue and no pump in between. engine/app/window.cpp's resize_client
		// is a SetWindowPos, which does exactly that; Application::
		// set_resolution is its one caller, and set_fullscreen reaches the same
		// SetWindowPos through enter_fullscreen/leave_fullscreen rather than
		// through it; and Application::render
		// runs the state's whole draw walk between begin_frame and submit. So a
		// client that changes resolution from inside its own drawing lands here
		// with a frame open, in one call stack, and no amount of care at the
		// call site would have told it so. "Do not call this between begin_frame
		// and submit" would be a prohibition on something the caller does not
		// control, which is the kind of rule T6 says to make impossible rather
		// than to document.
		//
		// TWO OTHER MECHANISMS WERE NAMED HERE AND NEITHER EXISTS, which is
		// worth recording because both were checkable against files this
		// paragraph cited by path. "window.cpp renders from WM_PAINT" - it
		// does, but only under `if (self->in_sizemove_)`, and it forwards
		// WM_SIZE only under `!in_sizemove_`; the conditions are complements,
		// so in exactly the state where the shell renders from a message it
		// throws every resize away, which the back_buffer_size paragraph below
		// says itself. (WM_PAINT also puts a frame inside a message rather than
		// a message inside a frame.) And "a vsync Present is entitled to pump" -
		// the only Present is inside end_frame, which is after submit, so
		// whatever window a pumping Present opens, it opens outside the interval
		// this paragraph is about. The term is right; the reasoning under it was
		// not, and a false justification is worse than none because it reads as
		// though the case had been considered.
		//
		// WHAT RESTARTED MEANS, EXACTLY, because a caller may be holding a
		// DrawList when this lands and that list must not become a trap:
		//
		//  - Everything recorded into every view this frame is dropped. It was
		//    recorded against a buffer that no longer exists.
		//  - The views the frame declared are reopened against the new buffer,
		//    which is cleared as begin_frame would clear it. A DrawList the
		//    caller is holding stays valid and draws into the new frame.
		//  - view_count is unchanged, because the layout is the shell's to
		//    decide and this call is what tells it to decide again.
		//
		// So the frame that was in progress contributes nothing, exactly as a
		// frame begun and never submitted does (begin_frame below), and the
		// caller sees one frame's drawing lost rather than an exception it has
		// nowhere to catch.
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
		//
		// AND THE TEARDOWN ORDER IS FIXED THE OTHER WAY AGAIN. The table
		// outlives the renderer, because on TWO backends it holds resources the
		// GPU may still be reading - an ID3D12Resource on one and a VkImage on
		// the other - and the only wait for them is inside this class's
		// destructor. render_resources.h states it beside
		// release_device_resources, where the resources in question are.
		void set_resources(const RenderResources* resources);

		// begin_frame clears the back buffer and resets every view's
		// recording; end_frame presents.
		//
		// A FRAME BEGUN AND NEVER SUBMITTED CONTRIBUTES NOTHING TO THE NEXT
		// ONE, which is a statement about what "resets" means and is worth
		// making because the five backends have four different things to
		// reset. THREE of them hold a frame in vectors - gl, null and vulkan -
		// where dropping it is clearing the vector; the D3D11 one holds it in a
		// deferred context, which keeps what was recorded into it until
		// something takes the command list away, so it has to drain as well as
		// forget; the D3D12 one holds an open command list, which cannot be
		// reset and whose allocator cannot be reset under it, so it has to be
		// closed before its memory can be reused and what it holds goes nowhere;
		// and the Vulkan one is the fourth kind, because clearing its vectors is
		// not the whole of it - the command pool and the descriptor pools are
		// reset too, and the tracked image layout goes back to what the last
		// submit left, since the barriers that moved it were in what was thrown
		// away. A client
		// reaches this by catching an exception out of its own draw walk and
		// carrying on - and so does a device event, which surfaces as a throw
		// from a worker mid-frame.
		void begin_frame();
		void end_frame();

		// The scene declares how many views this frame has, then fills them.
		// Throws std::out_of_range above the capacity create_device was given,
		// and std::out_of_range for a view index outside the current count -
		// asking for a view nobody set up used to return a fullscreen pane and
		// draw a whole extra pass (T6).
		//
		// AND std::logic_error FOR A COUNT LOWERED PAST A VIEW SOMETHING HAS
		// ALREADY DRAWN INTO, which is a different mistake and gets a different
		// type. The recording is stranded rather than absent: on three backends
		// - gl, null and vulkan - it is a vector nothing will replay, on the
		// D3D11 one a deferred context holding commands submit() will not reach,
		// and on the D3D12 one a closed command list submit() will not put in
		// its array. All five throw it, which is what makes it a term of the
		// seam rather than one backend's caution.
		// create_device throws std::invalid_argument for a view capacity below
		// one, before it touches a window.
		void set_view_count(int count);
		int view_count() const;
		DrawList view(int index) const;

		// CONSTRAINT: whatever a view recorded into, closing it is the seam's
		// job and not a caller's.
		//
		// Draws every view in view order, which is the only ordering guarantee
		// made here, and leaves nothing of the frame behind. What that costs
		// depends on the backend and is deliberately not described on this line:
		// three of the five replay a vector, and the two that do not each
		// execute a command list per view by a different route. The D3D11 one runs a
		// protocol (record, FinishCommandList, ExecuteCommandList, Release) that
		// was hand-written in four places, each of which had to pre-size a
		// vector, pre-fill it with null and Release every non-null entry, three
		// caller obligations stated nowhere in the tree; two of the four call
		// sites already disagreed about RestoreContextState. There is one copy now and it is
		// not the caller's; engine/render/d3d11/backend.h is where it is
		// described. The D3D12 one hands the finished lists to its queue as one
		// array in view order, in a single ExecuteCommandLists - the one submit
		// shape a fourth backend actually introduced, and the cheapest of the
		// five to describe. The fifth introduced none: engine/render/vulkan/
		// replays vectors like the OpenGL one, because a VkCommandPool may not
		// be used from two threads at once and a view's vertices are already
		// built on the CPU before any backend sees them.
		//
		// Called once per frame, between begin_frame and end_frame.
		void submit();

		// Back-buffer size in pixels. Replaces DeviceResources::GetOutputSize
		// and GetScreenViewport, which is all the layout arithmetic in
		// engine/render/ ever wanted from the device.
		//
		// THE SIZE OF THE BUFFER, NOT OF THE LAST THING ANYONE SAID. The two
		// agree whenever the shell is keeping up, and the one state where they
		// need not is a drag-resize, during which the shell discards every
		// WM_SIZE and still asks for frames (engine/app/window.cpp). What a
		// backend answers then is whatever it is really drawing into: a swap
		// chain does not follow its window, so both Direct3D backends answer
		// the size they were told and let Present stretch; a WGL context's
		// default framebuffer is the window's client area, so the GL backend
		// answers the window. read_back_buffer below is sized from this and a
		// viewport is placed inside it, so a backend that answered from a cache
		// would have both of those disagree with the pixels it just drew.
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
		// RGBA REGARDLESS OF WHAT THE BACKEND STORES, so that the assertions
		// another backend has to pass are the same bytes and not the same bytes
		// after a per-backend swizzle. Whether that costs anything is the
		// backend's business and is written down in the backend: three of the
		// four rasterisers' buffers are BGRA and all three swap on the way out -
		// both Direct3D ones and the Vulkan one, whose colour target is
		// B8G8R8A8_UNORM for exactly that reason - the D3D12 one additionally
		// unpads a row pitch its API rounds up to 256 bytes, and the GL one is
		// asked for as RGBA and only flipped, because GL reads from the bottom.
		// One swap and no flip on three, one flip and no swap on the fourth.
		//
		// A BACKEND MAY REFUSE, and one does. There is nothing for the null
		// backend to copy - it records what it was asked to draw and never
		// rasterises it - so it throws std::logic_error saying so, which is the
		// honest answer and the reason RenderPixelTests is not built in that
		// configuration at all rather than built and skipped.
		//
		// BETWEEN submit() AND end_frame(), because on the backends where a
		// present discards what it presented there is nothing left to read
		// afterwards. That is a flip-model swap chain's behaviour and not a
		// term of this seam: the Vulkan backend draws into an image the engine
		// owns and blits it into a swapchain image at present, so reading after
		// end_frame would answer there - and a contract that holds on one
		// backend and not another is not a contract. The interval is the
		// narrow one every backend can keep, and
		// tests/render/pixel_tests.cpp's "a frame may be read back and then
		// presented" walks its far end.
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
		// (engine/render/d3d11/device_resources.h says which and why - it is the
		// only one of the five that HAD such a wrapper to strip, being the only
		// one this repository did not write. The D3D12 and Vulkan ones were
		// written to this seam from the start, so they never had accessors
		// nobody called; a device can be lost on three of the five, and the
		// settled note at the foot of this file says so).
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

// SETTLED, AND THIS FILE IS WHERE IT IS RECORDED:
//
//  - AssetKind::reload_device STAYS ON THE LOADER. It was the one open question
//    at the foot of this file, and this is what closed it. What was open: a
//    public std::function on the asset loader that exists only because a
//    Direct3D device can be lost, which two of the four backends behind this
//    seam never call - a WGL context is not lost, and the null one has nothing
//    to lose - so a caller writes a rebuild path for a hazard its
//    configuration may not have. Three places it could go, and the other two
//    are worse. (Four was the count when this was settled, and the fifth
//    landed on the same side of it: Vulkan has VK_ERROR_DEVICE_LOST and
//    engine/render/vulkan/device_resources.cpp answers it, which makes the
//    hazard three of five and this settlement stronger rather than
//    different.)
//
//    NOT ON DeviceNotify, WHICH ALREADY CARRIES THE HALF IT CAN CARRY. That
//    interface is the EVENT - the device went away, it came back - and the
//    shell hears it and calls the loader (engine/app/application.cpp,
//    on_device_restored). What it cannot carry is WHAT TO REBUILD. A rebuild
//    has to refill the slots the old resources sat in rather than make new
//    ones, because a drawable holds a handle, a handle names a slot and a slot
//    belongs to a name (render_resources.h, resolve_texture) - and the only
//    thing in this engine that knows the names is the manifest, which is what
//    the loader keeps. Move the rebuild to DeviceNotify and a game keeps a
//    second list beside the manifest, which engine/assets/resource_loader.h
//    already answers in the paragraph on reload_device_resources: walking the
//    manifest is true by construction where a list kept in step by hand is
//    only true today.
//
//    NOT NOWHERE, which is the option the fourth backend closed and the fifth
//    closed again. A device is lost on three of the five rather than on one,
//    so the hazard is most of this seam's configurations and not a Direct3D 11
//    peculiarity that another backend might leave behind. Vulkan is the
//    backend this paragraph was hedging about when it wrote "a fifth backend
//    might"; it did not.
//
//    AND THE PREMISE THAT MADE IT LOOK MISPLACED IS THE PART THAT WAS WRONG.
//    "A hazard its configuration may not have" is a statement about a build,
//    not about a caller. LABRADOR_RENDER_BACKEND picks a backend at configure
//    time (T5) over one game source, so the same register_kind call compiles
//    into a D3D11 build and into a GL one; what varies is which build ever
//    runs the path, never which source has to write it. A seam whose caller
//    contract changed per backend is the thing this file exists to prevent,
//    and two backends in this tree already pay the mirror of that price and
//    say so at length: gl/renderer.cpp and null/renderer.cpp
//    both drop a frame on a resize that destroys nothing of theirs, because a
//    caller must not be able to tell which backend it has. reload_device is
//    the same shape one module over - free where it never runs, and observable
//    by its absence where it does.
//
//    WHAT CHANGED WHEN IT WAS SETTLED, because "it stays where it was" is
//    otherwise indistinguishable from nobody having looked. The criterion a
//    caller applies is written on AssetKind itself now - does the GPU hold
//    this asset, never can this backend lose a device - which that comment did
//    not say, and which left the wrong reading available to anyone who read it
//    while building the GL or the null preset. And what a restore does is
//    pinned rather than only described: tests/assets/resource_loader_tests.cpp
//    asserts the order, the skipping of the kinds the GPU does not hold, and
//    that a second manifest replaces what is replayed. It needs no device, so
//    it runs in all five configurations - which is where a contract that is
//    not a backend's belongs.
//
//  - The shape of a backend is three translation units every backend has -
//    renderer.cpp, render_resources.cpp and texture_factory.cpp, all in
//    engine/render/<backend>/ - plus whatever that backend needs to build its
//    shader, which for one of the five is nothing and for three of them is the
//    same file, plus at most one more for the API itself. Only null stops at
//    three: d3d11, d3d12 and vulkan each add device_resources.cpp and gl adds
//    gl_functions.cpp, and all four are the same kind of file, which is the
//    part of an API that is not about drawing. The third file is where they
//    diverge most and is the honest measure of what a port owes for content -
//    115 lines on d3d11, 310 on d3d12, 168 on gl, 48 on null and 378 on
//    vulkan - because it turns already-decoded bytes into a texture, and how
//    much work that is depends on how much the API will take unchanged. THE
//    LONGEST OF THEM IS THE ONE WHOSE API TAKES THE LEAST: D3D11 is handed
//    the bytes and copies them itself, where D3D12 wants a resource, a
//    staging buffer, a footprint per mip level, a copy on a command list, a
//    barrier and a wait - which is the argument for the fourth backend in one
//    file. VULKAN IS THE LONGEST OF
//    THE FIVE, and the extra is not the copy: that one takes the engine's own
//    tightly packed bytes, where a D3D12 one pads every row to 256 bytes and
//    has to be asked to what. It is that nothing in this API owns anything -
//    an allocation to bind to the image, a memory type chosen for it by hand,
//    and a handler putting all three back on every path that can throw,
//    because there is no ComPtr. Path-building and
//    file-reading are in engine/render/resource_factory.cpp, written once for
//    everybody.
//
//    AND THE SHADER IS NOT A BACKEND'S AT ALL, which is where this paragraph
//    used to put it. engine/render/sprite.hlsl is one file compiled three
//    times, at a profile each backend picks and into a byte array each keeps
//    to itself, because the source is character for character the same and a
//    second copy of it would be a file that can silently disagree with the
//    first. What a backend owns there is the profile and how it binds b0 - a
//    constant buffer on one, four root constants on the next, a uniform
//    buffer at a shifted descriptor binding on the third - and ONE difference
//    reaches the shader, which that file says twice and this line used to
//    deny: the declaration order of VertexIn's three members is an ABI term
//    on the Vulkan backend, because dxc assigns SPIR-V locations in
//    declaration order and a Vulkan pipeline binds attributes by number
//    rather than by semantic. Two of the three go through fxc into DXBC and one
//    through the Vulkan SDK's dxc into SPIR-V, which is a second compiler for
//    one unchanged source rather than a second source.
//
//    THE SECOND FILE IS WHERE THEY DIVERGE LEAST, and it used to be the file
//    where they diverged not at all: render_resources.cpp carried the whole
//    public RenderResources surface in every backend, a page of forwarding
//    calls kept identical by proofreading. It is a constructor, a destructor,
//    two moves and two lookups now. The rest is engine/render/render_
//    resources.cpp - a fourth file with a backend sibling's name and no
//    backend in it - because two of the three resource tables hold engine data
//    and only the third was ever hiding anything.
//
//  - What is on which side of the line. Every decision that shows on screen is
//    the engine's: which glyph goes where (font.h), what a .dds and a
//    .spritefont say (dds_file.h, sprite_font_file.h), and where a sprite's
//    four corners land and what they sample (sprite_geometry.h). What a
//    backend supplies is a device, a texture from bytes (texture_data.h), a
//    vertex buffer, a shader that multiplies each vertex by one constant, and
//    the states that make the blend premultiplied. NOTHING A BACKEND DOES
//    DECIDES WHERE A PIXEL GOES, which is what lets the four backends that
//    have a rasteriser pass the same assertions - over three hundred of them,
//    over thirty cases, and the number is not the point: what it buys is
//    that the file asserting them says "the renderer", never "this renderer".
//
//    IT IS FOUR RUNS AND ONE SET OF IMAGES, and the second half of that used
//    to be missing. This paragraph read "IT IS TWO RUNS, NOT ONE COMPARISON,
//    and that is the standing limit", because an assertion holds ONE backend to
//    a relationship and hand-copied implementations can get the same
//    relationship wrong in the same direction without any run noticing.
//    Every frame a case reads back is now also compared byte for byte against
//    a PNG of it in tests/render/golden/, and those images are what hold the
//    backends to each other rather than each to a sentence
//    (tests/render/golden_image.h carries the argument). Fifty frames,
//    six of which fill more than one view - the machinery the backends share
//    least, one a deferred context per view, one a command list per view and
//    the third and the fourth a vector. On one machine's GPU, d3d11, d3d12,
//    gl and vulkan reproduce all fifty exactly.
//
//    WHAT IS STILL SEPARATE RUNS is the running of it. LABRADOR_RENDER_BACKEND
//    picks a backend at configure time (T5), so these are still processes that
//    never meet and the checked-in set is what passes between them. Two of the
//    four now happen on the build machine as well as on a developer's, which
//    is the reason the fourth backend is a Direct3D one: a runner has no GPU
//    and Direct3D gets a device there anyway, where OpenGL falls back to GDI
//    1.1 and Vulkan has no in-box fallback at all - its software
//    implementations are installed rather than shipped - so CI rasterises the
//    whole contract twice against one set of images. And
//    two terms sit outside it, both stated where they are decided rather than
//    here. The size of a frame read back while the window has moved under an
//    unresized swap chain differs by backend because back_buffer_size above
//    says it must, so no image could hold that frame - and two more, which
//    resize to a smaller buffer mid-frame, are outside the set by choice rather
//    than by contract, because a golden set is one image per case at one size.
//    Harness::end_not_comparable holds all three and separates the reasons. And
//    a per-channel
//    allowance of 8 is what lets one set serve both a developer's hardware
//    adapter and whatever a build machine offers; golden_image.cpp carries the
//    measurement that set it, the reason it is not zero, and the reason it no
//    longer names CI's rasteriser.
//
//    tests/render/renderer_seam_tests.cpp is the part that runs in all five
//    configurations, being everything the seam answers without a device. See
//    docs/review/backend-equivalence/TEST-GAP.md, which proposed the images.
//
//  - BOTH PURPOSES AT THE TOP OF THIS FILE ARE NOW FILLED, and neither is a
//    claim any more. engine/render/gl/ is OpenGL 3.3 core and passes
//    RenderPixelTests; engine/render/null/ has no graphics API at all and
//    records what it was asked to draw, which is what lets a test assert which
//    sprites a frame submitted on a machine with no driver. Writing either
//    changed nothing above the seam.
//
//    The one thing the port found: OpenGL has no deferred contexts, so a view
//    there records into memory and submit() replays it on the thread that owns
//    the context. The seam admitted that without a word of it reaching a
//    caller, because the unit of work is a view and a view's vertices are
//    already built on the CPU before any backend sees them - and the null
//    backend then took the same shape one step further, recording and never
//    replaying at all.
//
//  - AND A THIRD PURPOSE NOBODY HAD WRITTEN DOWN, which engine/render/d3d12/
//    is the answer to. Every API behind this seam hid the CPU/GPU boundary
//    until that one: D3D11 renames a mapped buffer for you and tracks what is
//    still in flight, OpenGL's driver does the same, and the null backend has
//    no GPU to be out of step with. So nothing had ever asked whether the
//    methods on this class still describe a frame when the ENGINE owns the
//    fence - when a command allocator may not be reused until the GPU says so,
//    a vertex page written this frame is still being read next frame, and a
//    texture upload is a copy somebody has to wait for.
//
//    THEY DO, AND NOT ONE SIGNATURE MOVED. What the port added is one line
//    inside a backend's begin_frame, waiting on a fence before anything resets
//    an allocator, which no caller can see. What it changed up here is a
//    sentence rather than a signature: "begin_frame resets every view's
//    recording" gained a third kind of thing to reset - a command list that is
//    still open, which cannot be reset and whose allocator cannot be reset
//    under it. That paragraph is above, on begin_frame, and it now names all
//    three; it used to say "the three backends have three different things to
//    reset" and that sentence went stale on the day the fourth landed, having
//    been written when the count of backends and the count of things happened
//    to be the same number.
//
//  - DirectXTK is no longer on the render path at all. It remains bought for
//    audio and for the gamepad reader, which are seams of their own and are
//    not this file's business.
