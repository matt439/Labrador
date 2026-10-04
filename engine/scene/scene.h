#pragma once

#include "engine/collision/broad_phase.h"
#include "engine/collision/collision_object.h"
#include "engine/collision/contacts.h"
#include "engine/core/game_object.h"
#include "engine/math/rectanglef.h"

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>
#include "engine/render/camera.h"
#include "engine/render/viewport.h"

namespace labrador
{
	class DrawList;
	class Partitioner;
	class Renderer;
	class ThreadPool;

	// What is in the world, where the world is seen from, and the four phases
	// that step it and draw it.
	//
	// The scene is the mechanism every game needs: the objects and their
	// loops, the collision sweep and the per-view render fan-out. What a level
	// means - its rules, its timers, its music, its HUD - is the game's, so a
	// game's level is a class of its own that owns a Scene (PHILOSOPHY,
	// Structural types).
	//
	// IT IS ITS OWN MODULE, AND NOT PART OF engine/core/. A scene owns
	// collision objects and sweeps them, so it depends on `collision` - and
	// `collision` depends on `core` (CollisionObject is a GameObject), so a
	// scene in core would close a cycle the module table forbids. It also
	// drives the renderer, and `core` may depend on math alone. The arrows
	// here point one way, at core, math, collision and render, which is the
	// shape `ui` has for the same reason (ARCHITECTURE, Modules).
	//
	// WHAT IS DELIBERATELY NOT HERE. No ViewportManager, no CameraTools, no
	// ResolutionManager, no RenderResources. Where the panes are, how a camera
	// follows a player and what filtering the pixels want are all policy, and
	// policy belongs to the client (T1) - which is why the view list below is
	// something the game fills rather than something the scene computes.
	class Scene
	{
	public:
		// Where a view lands on the back buffer, and the mapping from world to
		// it.
		//
		// CONSTRAINT: the unit of work is a view (engine/render/renderer.h).
		// The render workers do not own disjoint slices of the object list -
		// every worker enters draw() on the same object at the same time, which
		// is why GameObject::draw is const. This struct is the thing they own
		// disjointly instead.
		//
		// A view is not a player. Split-screen is one view per player, but a
		// title screen is a view with nobody behind it, so the scene keeps the
		// list and the game decides what fills it.
		struct View
		{
			Viewport viewport;
			Camera camera = Camera::DEFAULT_CAMERA;
		};

		// Whatever the game draws over a view once the world is in it: a HUD, a
		// split-screen divider, a countdown, a debug overlay.
		//
		// It is a parameter to draw() rather than a registered list of overlay
		// objects, because an overlay is not an object in the world - it is not
		// culled, it is not swept, and it wants a different camera (usually the
		// identity, since a HUD is laid out in its own pane's coordinates).
		// Registering it would mean a second list with a second set of rules,
		// and the rules are the game's.
		//
		// It runs on the worker that owns that view, so it is held to the same
		// contract draw() is: a pure read of the game's own state. `view_index`
		// is an index into this scene's view list, and the game filled that
		// list, so it is the game's own ordering coming back.
		using ViewOverlay = std::function<void(int view_index, DrawList& list)>;

		// Borrowed, both of them: the shell owns the pool and outlives every
		// scene. They are the fan-out and nothing else - a scene with one view
		// touches neither.
		//
		// Either may be nullptr, and then every view is drawn on the calling
		// thread. That pair is the dial: below a few hundred objects, dividing
		// the views across workers costs more than the work it divides, and only
		// the game knows its own counts (PHILOSOPHY, Performance).
		Scene(ThreadPool* thread_pool, const Partitioner* partitioner);
		~Scene();

		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;

		// Takes ownership, and does not insert yet: the object is pending until
		// the next end_tick().
		//
		// Deferral is not tidiness. Objects enter the world mid-tick - a weapon
		// fires, a piece spawns - from inside an update() the scene is running,
		// and a push into the list being walked invalidates the iterator
		// walking it.
		//
		// The returned pointer is valid immediately, and is the object's own
		// type rather than the list's - it is what a caller that needs to keep
		// speaking to the object holds. It stops being valid when the scene
		// retires the object, which is the end of the tick in which something
		// called set_for_deletion(true) on it.
		//
		// One name and not two, because a collision object is both: it derives
		// from CollisionObject, which derives from GameObject, so an overload
		// pair would be ambiguous at every call that names a concrete type.
		// Which list an object joins is a property of the object, so the object
		// decides it - here, at compile time, with no runtime test and no
		// dynamic_cast.
		template <typename T>
		T* add(std::unique_ptr<T> object)
		{
			T* result = object.get();

			if constexpr (std::is_base_of_v<CollisionObject, T>)
			{
				this->pending_collision_objects_.push_back(std::move(object));
			}
			else
			{
				static_assert(std::is_base_of_v<GameObject, T>,
					"Scene::add takes a GameObject.");
				this->pending_objects_.push_back(std::move(object));
			}

			return result;
		}

		// Every collision object currently in the world, in no particular
		// order.
		//
		// This is what the sweep is handed, so it exists either way; exposing
		// it costs nothing and saves the game keeping a parallel list of its
		// own. What the game wants it for is the questions only the game can
		// ask - "how many of these are still standing" - and the answer to
		// those is a walk, once, not a virtual on the object.
		std::span<CollisionObject* const> collision_objects() const;

		// The view list, refilled by the game each tick.
		//
		// Each tick and not each frame: draw() is const, because it is a pure
		// read from every worker at once. A camera that follows a player is
		// therefore chosen where the player moved, in update().
		void clear_views();
		void add_view(const Viewport& viewport,
			const Camera& camera = Camera::DEFAULT_CAMERA);

		int view_count() const;

		// Throws std::out_of_range outside the current count.
		const View& view(int index) const;

		// The world's extent. Collision objects whose shape leaves it are
		// retired at the end of the tick.
		//
		// Optional, and unset by default: a menu has no edges to fall off. A
		// projectile that leaves the level is ordinary - it missed - so this is
		// retirement and not an error. An object the game cannot afford to lose
		// this way says so by ignoring set_for_deletion(), which is the default
		// for anything that is part of the fixed geometry.
		void set_bounds(const mattmath::RectangleF& bounds);
		const std::optional<mattmath::RectangleF>& bounds() const;

		// Whether the object is inside the bounds above, which is always true
		// while they are unset. Public because "my player left the world" is a
		// simulation failure the game wants to report loudly rather than a
		// retirement it wants performed quietly, and both readings come off the
		// same rectangle.
		bool in_bounds(const CollisionObject& object) const;

		// PHASE 1. Steps every object in the world, once.
		void update(float dt);

		// PHASE 2. Measures every overlapping pair and tells both participants.
		//
		// The contacts are a value list, readable afterwards - which is the
		// reason find_contacts fills a vector instead of firing a callback from
		// inside the sweep, and the reason there is no event bus here. A test
		// asserts on the list; a game that wants to know what touched what this
		// frame reads it before end_tick(). The span and copied participant
		// pointers are borrowed only until the next resolve(), end_tick(), or
		// scene destruction. end_tick() clears all contacts, including pairs
		// whose participants survive; inspection after it needs copied values
		// such as geometry, not participant pointers.
		void resolve();
		std::span<const Contact> contacts() const;

		// PHASE 3 is the game's, and it has no function on this class.
		//
		// Between resolution and the end of the tick is where a game does what
		// depends on where the contacts left things: a weapon moved to follow
		// the body a contact just pushed, or the rectangle next tick's sweep
		// measures movement against. A virtual on GameObject would name that
		// phase for everyone, at the price of a call with an empty body on
		// every object of every tick to serve the few that need it, which is
		// the frame-loop tax T8 refuses. A game that wants the phase writes a
		// loop over the objects it already holds pointers to, between
		// resolve() and end_tick(), and the ordering is lexical and visible
		// where a hook's would not be.

		// PHASE 4. Applies everything that was waiting for the tick to be over:
		// the bounds sweep, then the retirements, then the pending adds.
		//
		// That order is the whole of it. Retiring before inserting means an
		// object cannot be born already flagged by a sweep that ran before it
		// existed, and inserting last means this tick's spawns are first swept
		// by the next tick - a projectile does not collide on the frame it
		// leaves the muzzle.
		//
		// A scene that has just been populated has a tick to end before it has
		// had one, because the frame drawn before the first update() is a real
		// frame. Whoever fills the scene calls this once.
		void end_tick();

		// Appends this scene after earlier draws in the frame. Its first view
		// reuses the final declared renderer view, preserving its earlier draws;
		// additional views occupy new slots. A fullscreen overlay therefore
		// needs no extra capacity. Multiple multiview scenes need capacity for
		// 1 + sum(scene.view_count() - 1). Submission follows scene order,
		// then each scene's view order; overlay callback indices stay local.
		// An empty scene leaves the existing frame alone.
		//
		// Fills each one: the world through that view's camera, culled to what
		// that view can see, then the game's overlay over it.
		//
		// The fan-out is here and nowhere else, because hand-written copies of
		// it diverge. It is a scene function rather than a general parallel-for
		// on ThreadPool because what it parallelises is views, which is a thing
		// only a scene knows it has.
		void draw(Renderer& renderer, const ViewOverlay& overlay = {}) const;

	private:
		// One worker's share of the views. Every worker on a different range,
		// all of them reading the same objects.
		void draw_views(int start, int end, int offset, Renderer& renderer,
			const ViewOverlay& overlay) const;

		std::vector<std::unique_ptr<GameObject>> objects_;
		std::vector<std::unique_ptr<CollisionObject>> collision_objects_;

		std::vector<std::unique_ptr<GameObject>> pending_objects_;
		std::vector<std::unique_ptr<CollisionObject>> pending_collision_objects_;

		// The collision objects as bare pointers, which is what the sweep takes
		// and what collision_objects() hands out. Rebuilt in end_tick(), the
		// only phase that changes the list, and not per frame.
		std::vector<CollisionObject*> collidables_;

		// Kept across ticks so a busy frame allocates nothing after the first.
		std::vector<Contact> contacts_;

		// Which pairs are worth measuring. A member rather than a local so
		// that its buffers survive the frame, which is what keeps resolve()
		// allocation-free once it is warm.
		BroadPhase broad_phase_;

		std::vector<View> views_;

		std::optional<mattmath::RectangleF> bounds_;

		ThreadPool* thread_pool_ = nullptr;
		const Partitioner* partitioner_ = nullptr;
	};
}
