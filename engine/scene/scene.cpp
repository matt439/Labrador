#include "engine/scene/scene.h"

#include "engine/collision/collision_object.h"
#include "engine/collision/partitioner.h"
#include "engine/core/thread_pool.h"
#include "engine/render/renderer.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace labrador
{
	namespace
	{
		// Whether one of the scene's lists owns `object`, compared as the
		// GameObject remove() was handed.
		template <typename T>
		bool owns(const std::vector<std::unique_ptr<T>>& list,
			const GameObject* object)
		{
			return std::any_of(list.begin(), list.end(),
				[object](const std::unique_ptr<T>& held)
				{
					return static_cast<const GameObject*>(held.get()) == object;
				});
		}

		// Erases from `list` what `sorted` names, keeping the order of the
		// rest.
		template <typename T>
		void erase_named(std::vector<std::unique_ptr<T>>& list,
			const std::vector<const GameObject*>& sorted)
		{
			std::erase_if(list, [&sorted](const std::unique_ptr<T>& held)
				{
					return std::binary_search(sorted.begin(), sorted.end(),
						static_cast<const GameObject*>(held.get()),
						std::less<const GameObject*>());
				});
		}
	}

	Scene::Scene(ThreadPool* thread_pool, const Partitioner* partitioner) :
		thread_pool_(thread_pool),
		partitioner_(partitioner)
	{
	}

	Scene::~Scene() = default;

	void Scene::remove(const GameObject* object)
	{
		if (object == nullptr ||
			(!owns(this->objects_, object) &&
			!owns(this->collision_objects_, object) &&
			!owns(this->pending_objects_, object) &&
			!owns(this->pending_collision_objects_, object)))
		{
			throw std::invalid_argument("Scene::remove: the object is not in "
				"this scene - it was never added, or an earlier end_tick() "
				"already removed or retired it.");
		}

		this->removals_.push_back(object);
	}

	std::span<CollisionObject* const> Scene::collision_objects() const
	{
		return this->collidables_;
	}

	void Scene::clear_views()
	{
		this->views_.clear();
	}

	void Scene::add_view(const Viewport& viewport, const Camera& camera)
	{
		this->views_.push_back(View{ viewport, camera });
	}

	int Scene::view_count() const
	{
		return static_cast<int>(this->views_.size());
	}

	const Scene::View& Scene::view(int index) const
	{
		if (index < 0 || static_cast<size_t>(index) >= this->views_.size())
		{
			throw std::out_of_range("Scene::view(" + std::to_string(index) +
				") - this scene has " + std::to_string(this->views_.size()) +
				" views.");
		}
		return this->views_[static_cast<size_t>(index)];
	}

	void Scene::set_bounds(const RectangleF& bounds)
	{
		this->bounds_ = bounds;
	}

	const std::optional<RectangleF>& Scene::bounds() const
	{
		return this->bounds_;
	}

	bool Scene::in_bounds(const CollisionObject& object) const
	{
		if (!this->bounds_.has_value())
		{
			return true;
		}
		return this->bounds_->intersects(object.shape()->bounding_box());
	}

	void Scene::update(float dt)
	{
		for (const std::unique_ptr<CollisionObject>& object :
			this->collision_objects_)
		{
			object->update(dt);
		}

		for (const std::unique_ptr<GameObject>& object : this->objects_)
		{
			object->update(dt);
		}
	}

	void Scene::resolve()
	{
		find_contacts(this->collidables_, this->contacts_,
			&this->broad_phase_);
		dispatch_contacts(this->contacts_);
	}

	std::span<const Contact> Scene::contacts() const
	{
		return this->contacts_;
	}

	void Scene::end_tick()
	{
		// Contacts borrow their participants. Expire the whole tick's snapshot
		// before any participant can be destroyed, even when none retires.
		this->contacts_.clear();

		// Mark out-of-bounds objects for the same retirement pass as explicit
		// requests. Each collision object decides whether it records the flag.
		if (this->bounds_.has_value())
		{
			for (const std::unique_ptr<CollisionObject>& object :
				this->collision_objects_)
			{
				if (!object->for_deletion() && !this->in_bounds(*object))
				{
					object->set_for_deletion(true);
				}
			}
		}

		// Keep survivors in insertion order: Scene::draw uses that order for
		// layering, so a removal must not reorder later draws. Stable erasure
		// preserves the pass's linear complexity.
		std::erase_if(this->collision_objects_,
			[](const std::unique_ptr<CollisionObject>& object)
			{
				return object->for_deletion();
			});

		// The removals. Sorted once so each list is one pass of binary
		// searches, and std::erase_if so the plain objects keep the order they
		// draw in. A pending object removed in its own tick goes here too,
		// before the adds below could admit it.
		if (!this->removals_.empty())
		{
			// std::less, because operator< between unrelated pointers is
			// not an order the standard promises and std::less is.
			std::sort(this->removals_.begin(), this->removals_.end(),
				std::less<const GameObject*>());
			erase_named(this->objects_, this->removals_);
			erase_named(this->collision_objects_, this->removals_);
			erase_named(this->pending_objects_, this->removals_);
			erase_named(this->pending_collision_objects_, this->removals_);
			this->removals_.clear();
		}

		// The pending adds, last.
		for (std::unique_ptr<GameObject>& object : this->pending_objects_)
		{
			this->objects_.push_back(std::move(object));
		}
		this->pending_objects_.clear();

		for (std::unique_ptr<CollisionObject>& object :
			this->pending_collision_objects_)
		{
			this->collision_objects_.push_back(std::move(object));
		}
		this->pending_collision_objects_.clear();

		// Rebuild the sweep's borrowed pointers after the owning list changes.
		// end_tick() is the only phase that inserts or removes those objects.
		this->collidables_.clear();
		this->collidables_.reserve(this->collision_objects_.size());
		for (const std::unique_ptr<CollisionObject>& object :
			this->collision_objects_)
		{
			this->collidables_.push_back(object.get());
		}
	}

	void Scene::draw(Renderer& renderer, const ViewOverlay& overlay) const
	{
		const int count = static_cast<int>(this->views_.size());
		const int offset = std::max(0, renderer.view_count() - 1);
		if (count == 0) return;
		renderer.set_view_count(offset + count);

		// One view needs no fan-out. Either null dependency selects the calling
		// thread for every view. The game chooses this dial: a useful parallel
		// crossover depends on the workload and machine, so an object-count
		// threshold would embed policy here (T1; docs/design/PHILOSOPHY.md,
		// Performance). bench/fanout_bench_null.cpp owns the measurement.
		if (count <= 1 || this->thread_pool_ == nullptr ||
			this->partitioner_ == nullptr)
		{
			this->draw_views(0, count, offset, renderer, overlay);
			return;
		}

		const std::vector<std::pair<int, int>> ranges =
			this->partitioner_->partition(count,
				this->thread_pool_->max_num_threads());

		try
		{
			for (const std::pair<int, int>& range : ranges)
			{
				this->thread_pool_->add_task([this, range, offset, &renderer, &overlay]()
					{
						this->draw_views(range.first, range.second, offset, renderer,
							overlay);
					});
			}
		}
		catch (...)
		{
			// Submitted callbacks borrow this scene, renderer and overlay. They
			// must finish before the failed submission unwinds those lifetimes.
			try
			{
				this->thread_pool_->wait_for_tasks_to_complete();
			}
			catch (...)
			{
				// The wait drains every callback before reporting a task error.
				// Preserve the submission failure that brought us here.
			}
			throw;
		}

		this->thread_pool_->wait_for_tasks_to_complete();
	}

	void Scene::draw_views(int start, int end, int offset, Renderer& renderer,
		const ViewOverlay& overlay) const
	{
		for (int i = start; i < end; i++)
		{
			const View& view = this->views_[static_cast<size_t>(i)];

			DrawList list = renderer.view(offset + i);
			list.set_viewport(view.viewport);
			list.set_camera(view.camera);

			// Invert this view's camera transform to find its world-space region:
			// camera translation plus pane dimensions divided by scale.
			const RectangleF visible =
				view.camera.visible_rectangle(view.viewport);

			// Cull both object lists for every view. Drawables provide conservative
			// extents for this camera's world units per pixel, including any
			// zoom-dependent growth (docs/design/PHILOSOPHY.md, Simulation and rendering).
			for (const std::unique_ptr<GameObject>& object : this->objects_)
			{
				if (object->cull_bounds(1.0f / view.camera.scale).intersects(visible))
				{
					object->draw(list);
				}
			}

			for (const std::unique_ptr<CollisionObject>& object :
				this->collision_objects_)
			{
				if (object->cull_bounds(1.0f / view.camera.scale).intersects(visible))
				{
					object->draw(list);
				}
			}

			if (overlay)
			{
				overlay(i, list);
			}
		}
	}
}
