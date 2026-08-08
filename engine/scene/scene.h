#pragma once

#include "engine/collision/collision_object.h"
#include "engine/collision/contacts.h"
#include "engine/core/game_object.h"
#include "engine/math/matt_math.h"

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace artattack
{
	class DrawList;
	class Partitioner;
	class Renderer;
	class ThreadPool;

	// What is in the world, where the world is seen from, and the four phases
	// that step it and draw it.
	//
	// This is finding #18, outstanding since the 2023 review. What it replaces
	// is a 766-line Level that was three classes wearing one name: the object
	// lists and their loops, the collision sweep and the per-view render
	// fan-out - none of which is about a paint match - wrapped around a state
	// machine, four timers, a music track and a HUD, which all are. Every
	// engine that has ever been written has the first half; only this game has
	// the second. A second game got none of it.
	//
	// WHY THIS IS ITS OWN MODULE, AND NOT engine/core/.
	//
	// The plan filed this as engine/core/scene.*, written before
	// engine/collision/ existed. It cannot live there. A scene owns collision
	// objects and sweeps them, so it depends on `collision` - and `collision`
	// depends on `core` (CollisionObject is a GameObject), so core -> collision
	// closes a cycle the module table forbids. It also drives the renderer, and
	// `core` may depend on math alone. Both walls are real and neither is worth
	// breaking for a filing convenience: the arrows here point one way, at
	// core, math, collision and render, which is the same shape `ui` already
	// has for the same reason (ARCHITECTURE, Modules).
	//
	// WHAT IS DELIBERATELY NOT HERE. No ViewportManager, no CameraTools, no
	// ResolutionManager, no RenderResources. Where the panes are, how a camera
	// follows a player and what filtering the pixels want are all policy, and
	// policy belongs to the client - which is why the view list below is
	// something the game fills rather than something the scene computes. The
	// scene owns the mechanism: one object list, one sweep, one fan-out.
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
		// Before it existed, the renderer's only source of view information was
		// the player list: the view count was player_objects_->size(), each
		// viewport came off a Player::player_num() and each camera off a
		// Player::camera(). So folding the players into one object list - which
		// is the whole point of having one - deleted the renderer's view
		// information, and the view list had to exist before the fold could.
		struct View
		{
			mattmath::Viewport viewport;
			mattmath::Camera camera = mattmath::Camera::DEFAULT_CAMERA;
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
		Scene(ThreadPool* thread_pool, const Partitioner* partitioner);
		~Scene();

		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;

		// Takes ownership, and does not insert yet: the object is pending until
		// the next end_tick().
		//
		// Deferral is not tidiness. The one place objects enter the world
		// mid-frame is a weapon firing, and with the players in the same list
		// as their projectiles - which is what one object list means - a push
		// from inside the update loop invalidates the iterator walking it. The
		// old code got away with it only because players and projectiles were
		// two lists and the push went into the one nobody was walking.
		//
		// The returned pointer is valid immediately, and is the object's own
		// type rather than the list's - it is what a caller that needs to keep
		// speaking to the object holds. It stops being valid when the scene
		// retires the object, which is the end of the tick in which something
		// called set_for_deletion(true) on it.
		//
		// One name and not two, because a Player is both: it derives from
		// CollisionObject, which derives from GameObject, so an overload pair
		// would be ambiguous at every call that names a concrete type. Which
		// list an object joins is a property of the object, so the object
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
		// ask - "how much of this arena is painted my colour" - and the answer
		// to those is a walk, once, not a virtual on the object.
		std::span<CollisionObject* const> collision_objects() const;

		// The view list, refilled by the game each tick.
		//
		// Each tick and not each frame: draw() is const, because it is a pure
		// read from every worker at once. A camera that follows a player is
		// therefore chosen where the player moved, in update().
		void clear_views();
		void add_view(const mattmath::Viewport& viewport,
			const mattmath::Camera& camera = mattmath::Camera::DEFAULT_CAMERA);

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
		// frame reads it.
		void resolve();
		std::span<const Contact> contacts() const;

		// PHASE 3 is the game's, and it has no function on this class.
		//
		// Between resolution and the end of the tick, the paint-shooter moves
		// each player's weapon to follow the body a contact just pushed, and
		// records the rectangle next frame's sweep measures movement against.
		// The plan offered two homes for that: name the phase for everyone with
		// a virtual on GameObject, or let the game run it after resolve().
		//
		// It is the game's, because the alternative is a virtual call with an
		// empty body on five thousand paint tiles per frame to serve one class,
		// which is exactly the frame-loop tax T8 refuses. A game that wants the
		// phase writes a loop over the objects it already holds pointers to,
		// between resolve() and end_tick(), and the ordering is lexical and
		// visible where a hook's would not be.

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

		// The frame. Declares this frame's views to the renderer, then fills
		// each one: the world through that view's camera, culled to what that
		// view can see, then the game's overlay over it.
		//
		// The fan-out is here and nowhere else. There were two hand-written
		// copies before the seam and they had already diverged - one per player
		// in the level, one per widget in a menu, the second of which indexed
		// deferred contexts by widget ordinal and so capped every menu at
		// however many the shell happened to make. The unification the review
		// asked for was "give ThreadPool a parallel_for"; this is it, and it is
		// a scene function rather than a pool one because what it parallelises
		// is views - which is a thing only a scene knows it has.
		void draw(Renderer& renderer, const ViewOverlay& overlay = {}) const;

	private:
		// One worker's share of the views. Every worker on a different range,
		// all of them reading the same objects.
		void draw_views(int start, int end, Renderer& renderer,
			const ViewOverlay& overlay) const;

		std::vector<std::unique_ptr<GameObject>> objects_;
		std::vector<std::unique_ptr<CollisionObject>> collision_objects_;

		std::vector<std::unique_ptr<GameObject>> pending_objects_;
		std::vector<std::unique_ptr<CollisionObject>> pending_collision_objects_;

		// The collision objects as bare pointers, which is what the sweep takes
		// and what collision_objects() hands out. Rebuilt in end_tick(), the
		// only phase that changes the list - not per frame, which is what the
		// loop it replaces did.
		std::vector<CollisionObject*> collidables_;

		// Kept across ticks so a busy frame allocates nothing after the first.
		std::vector<Contact> contacts_;

		std::vector<View> views_;

		std::optional<mattmath::RectangleF> bounds_;

		ThreadPool* thread_pool_ = nullptr;
		const Partitioner* partitioner_ = nullptr;
	};
}
