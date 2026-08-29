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

// The renderer seam: one header, five backends, one of them chosen at build
// time (T5).
//
// THIS FILE STATES THE CONTRACT A CALLER IS HELD TO, and nothing else. Why the
// seam has the shape it does - why Renderer is concrete rather than an
// abstract base, how one header serves five backends, what a backend supplies
// and what it may never decide, and what holds the five to one another - is
// engine/render/SEAM.md. Terms below cite it where the reason is longer than
// the rule.

namespace labrador
{
	// The table a DrawList resolves handles against. Declared, not included:
	// render_resources.h includes this file, for the handle types below.
	class RenderResources;

	// A handle is an index into the table that produced it (handle.h), and
	// these say which table.
	//
	// Texture never gains a definition: the backend decides what one is, and
	// nothing outside engine/render/<backend>/ needs to know, which is the whole
	// point of the seam. A Font is engine data over a TextureHandle
	// (engine/render/font.h), exactly as a SpriteSheet is, because a glyph table
	// and a pen are arithmetic and not a device resource. Both are declared and
	// not included here so that this file still costs a caller nothing.
	class Texture;
	class Font;

	using TextureHandle = Handle<Texture>;
	using FontHandle = Handle<Font>;

	enum class SpriteFlip
	{
		none,
		horizontal,
		vertical,
		both,
	};

	// CONSTRAINT: sampler state lives inside the seam. The API never names a
	// sampler object: the caller says what it wants the filtering to look like,
	// and the backend owns the state that produces it, recreated with the device
	// like everything else.
	//
	// point is what a pixel-art client wants everywhere - linear filtering is
	// what makes tile seams shimmer.
	enum class TextureFilter
	{
		point,
		linear,
	};

	// A recording target for one view.
	//
	// CONSTRAINT: the unit of work is a view, not an object.
	//
	// The render workers do NOT own disjoint slices of the object list - the
	// parallelism axis is views, so every worker enters draw() on the SAME
	// object at the same time. That is why draw() is const all the way down.
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
		void set_viewport(const Viewport& viewport);

		// World to view, for every draw recorded after it.
		//
		// It is per range and not per view because one view genuinely has two: a
		// client draws the world through its camera and the HUD over it in screen
		// space, into the same viewport, on the same frame. Changing it mid-list
		// is therefore ordinary, not exceptional.
		//
		// The identity is Camera::DEFAULT_CAMERA, which is what a list starts
		// with, so a caller that only ever wanted screen space never mentions a
		// camera at all.
		void set_camera(const Camera& camera);

		// Applies to every draw recorded after it. Changing it mid-list is legal
		// and costs a flush, so group by filter if it matters.
		//
		// IT CHOOSES BETWEEN TEXELS OF LEVEL ZERO AND NEVER BETWEEN LEVELS. A
		// minified draw samples level zero however many levels the texture
		// carries, under either filter. That is a term of this seam and not a
		// backend's habit. Chains are read and uploaded - a .dds that has one is
		// not rejected and its bytes reach the device - and nothing samples from
		// them. SEAM.md says why the other answer is not the engine's to give.
		void set_filter(TextureFilter filter);

		// CONSTRAINT: sort depth is per draw, not per object.
		//
		// layer_depth is a parameter here for the same reason the tint and the
		// flip are: the same sprite drawn into two views at two depths must be
		// expressible. Under the view fan-out, a depth read off the shared object
		// is the one member every worker reads while another view wants a
		// different value, and it is inexpressible rather than merely racy.
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
		// A glyph table is keyed by code unit, so a narrow overload is a
		// conversion, and a conversion on the draw path is either an allocation
		// per string per view per frame or a buffer shared between workers. Wide
		// in, wide all the way down; there is no narrow overload to fall back to
		// and there should not be one.
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
	// state, the sampler states, and the command-list lifetime.
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
		// frame may ever fan out to - which sizes the recording state, because a
		// backend that records into per-thread contexts has to make them before
		// any frame starts.
		//
		// void* rather than HWND because this is the file every backend
		// implements, and a window handle is not a graphics type: it is this
		// platform's window. The backend casts it back - or, in the null
		// backend's case, ignores it, which is what lets a client run on a
		// machine with no display.
		//
		// Throws std::invalid_argument for a view capacity below one, before it
		// touches a window.
		void create_device(void* native_window, int width, int height,
			int view_capacity);

		// Returns whether anything was rebuilt, which is the signal the shell
		// wants for "re-run the layout".
		//
		// CONSTRAINT: IT MAY ARRIVE IN THE MIDDLE OF A FRAME, AND A FRAME IN
		// PROGRESS IS RESTARTED RATHER THAN REFUSED.
		//
		// IT IS NOT A RULE THE CALLER COULD KEEP EVEN IF THIS FILE STATED ONE.
		// A resize reaches the shell as a window message, and a window message
		// can be SENT - straight to the window procedure, on the calling thread,
		// with no queue and no pump in between. engine/app/window.cpp's
		// resize_client is a SetWindowPos, which does exactly that, and
		// Application::render runs the state's whole draw walk between
		// begin_frame and submit. So a client that changes resolution from inside
		// its own drawing lands here with a frame open, in one call stack, and no
		// amount of care at the call site would have told it so. "Do not call
		// this between begin_frame and submit" would be a prohibition on
		// something the caller does not control, which is the kind of rule T6
		// says to make impossible rather than to document.
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
		//
		// AND BEFORE create_device IT REBUILDS NOTHING AND ANSWERS false, which
		// is a statement about an ordering Win32 allows: a shell can be sent a
		// WM_SIZE between making its window and making its device.
		bool window_size_changed(int width, int height);

		// Borrowed; the shell owns it and outlives the renderer.
		void set_device_notify(DeviceNotify* device_notify);

		// The table the draw lists resolve handles against. Borrowed, like the
		// notify above.
		//
		// draw_sprite takes a TextureHandle, and the only thing that can turn one
		// back into whatever this backend calls a texture is the table that
		// issued it. The alternative was for the caller to resolve first and hand
		// the resolved resource in - which is the backend's own type, in a game
		// file's hands, which is the seam undone on the first call.
		//
		// Separate from create_device because the order is fixed the other way:
		// the resource factory needs a device before it can load anything into a
		// table, so the table cannot exist when the device is made.
		//
		// AND THE TEARDOWN ORDER IS FIXED THE OTHER WAY AGAIN. The table outlives
		// the renderer, because on TWO backends it holds resources the GPU may
		// still be reading - an ID3D12Resource on one and a VkImage on the other
		// - and the only wait for them is inside this class's destructor.
		// render_resources.h states it beside release_device_resources, where the
		// resources in question are.
		void set_resources(const RenderResources* resources);

		// begin_frame clears the back buffer and resets every view's recording;
		// end_frame presents.
		//
		// A FRAME BEGUN AND NEVER SUBMITTED CONTRIBUTES NOTHING TO THE NEXT ONE,
		// which is a statement about what "resets" means. A client reaches that
		// by catching an exception out of its own draw walk and carrying on - and
		// so does a device event, which surfaces as a throw from a worker
		// mid-frame. What each of the five has to reset to keep it is in SEAM.md.
		void begin_frame();
		void end_frame();

		// The scene declares how many views this frame has, then fills them.
		// Throws std::out_of_range above the capacity create_device was given,
		// and std::out_of_range for a view index outside the current count -
		// asking for a view nobody set up must not quietly return a fullscreen
		// pane and draw a whole extra pass (T6).
		//
		// AND std::logic_error FOR A COUNT LOWERED PAST A VIEW SOMETHING HAS
		// ALREADY DRAWN INTO, which is a different mistake and gets a different
		// type: the recording is stranded rather than absent. All five backends
		// throw it, which is what makes it a term of the seam rather than one
		// backend's caution.
		void set_view_count(int count);
		int view_count() const;
		DrawList view(int index) const;

		// CONSTRAINT: whatever a view recorded into, closing it is the seam's job
		// and not a caller's.
		//
		// Draws every view in view order, which is the only ordering guarantee
		// made here, and leaves nothing of the frame behind. What that costs
		// depends on the backend and is deliberately not described on this line;
		// SEAM.md has the five answers.
		//
		// Called once per frame, between begin_frame and end_frame - AND A SECOND
		// CALL ADDS NOTHING, which is a decision rather than a description. One
		// flag per backend, set here and cleared by begin_frame, off the
		// per-sprite path entirely (T8).
		void submit();

		// Back-buffer size in pixels.
		//
		// THE SIZE OF THE BUFFER, NOT OF THE LAST THING ANYONE SAID. The two
		// agree whenever the shell is keeping up, and the one state where they
		// need not is a drag-resize, during which the shell discards every
		// WM_SIZE and still asks for frames (engine/app/window.cpp). What a
		// backend answers then is whatever it is really drawing into: a swap
		// chain does not follow its window, so both Direct3D backends answer the
		// size they were told and let Present stretch; a WGL context's default
		// framebuffer is the window's client area, so the GL backend answers the
		// window. read_back_buffer below is sized from this and a viewport is
		// placed inside it, so a backend that answered from a cache would have
		// both of those disagree with the pixels it just drew.
		mattmath::Vector2F back_buffer_size() const;

		// Copies the back buffer out: 8-bit RGBA, row-major, top row first,
		// exactly width * height * 4 bytes. `pixels` is resized to fit.
		//
		// RGBA REGARDLESS OF WHAT THE BACKEND STORES, so that the assertions
		// another backend has to pass are the same bytes and not the same bytes
		// after a per-backend swizzle. What that costs is the backend's business
		// and is written down in the backend.
		//
		// A BACKEND MAY REFUSE, and one does. There is nothing for the null
		// backend to copy - it records what it was asked to draw and never
		// rasterises it - so it throws std::logic_error saying so, which is the
		// honest answer and the reason RenderPixelTests is not built in that
		// configuration at all rather than built and skipped.
		//
		// BETWEEN submit() AND end_frame(), because on the backends where a
		// present discards what it presented there is nothing left to read
		// afterwards. The interval is the narrow one every backend can keep, and
		// tests/render/pixel_tests.cpp's "a frame may be read back and then
		// presented" walks its far end.
		//
		// Not const: reading the GPU's memory means staging a copy through the
		// device, which is a write.
		//
		// NOT A FRAME-PATH CALL and not meant to become one (T8). It stalls on
		// the GPU by construction.
		void read_back_buffer(std::vector<unsigned char>& pixels);

		// Debug markers, AND THEY ARE ADVISORY: a backend may forward them to a
		// tool and may do nothing at all, and a caller may not tell which from
		// anything it can observe. One of the five forwards them - d3d11, to
		// ID3DUserDefinedAnnotation, which is what a PIX capture reads - and four
		// discard them, each arguing the discard in its own file. That is the
		// whole contract, and it is here because it was written in four backend
		// folders and nowhere a caller may read (T6).
		//
		// LEGAL BEFORE create_device AND OUTSIDE A FRAME, which is the half a
		// caller could otherwise only discover by crashing. Nesting is the
		// caller's business: begin/end are a pair, and a backend that forwards
		// them forwards whatever nesting it is given.
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
