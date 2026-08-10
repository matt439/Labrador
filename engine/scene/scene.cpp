#include "engine/scene/scene.h"

#include "engine/collision/collision_object.h"
#include "engine/collision/partitioner.h"
#include "engine/core/thread_pool.h"
#include "engine/render/renderer.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace mattmath;

namespace artattack
{
	Scene::Scene(ThreadPool* thread_pool, const Partitioner* partitioner) :
		thread_pool_(thread_pool),
		partitioner_(partitioner)
	{
	}

	Scene::~Scene() = default;

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
		// The bounds sweep. Flagging rather than erasing, so an object that has
		// left the world is retired by the same line as an object that asked to
		// be - and so a game that wants to hear about it first can look at the
		// flags before this returns.
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

		// Retirement, by swap-and-pop: order in this list is not observable, so
		// paying to preserve it would be paying for nothing.
		for (size_t i = 0; i < this->collision_objects_.size();)
		{
			if (this->collision_objects_[i]->for_deletion())
			{
				this->collision_objects_[i] =
					std::move(this->collision_objects_.back());
				this->collision_objects_.pop_back();
			}
			else
			{
				i++;
			}
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

		// The bare-pointer mirror the sweep and collision_objects() speak in.
		// Rebuilt here because this is the only phase that can change the list;
		// what it replaces rebuilt it every frame, from two lists, inside the
		// tick.
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
		renderer.set_view_count(count);

		// One view is not worth a fan-out, and neither is a scene with no pool:
		// a menu, a sample and a headless test all draw one pane, and the pool
		// is a constructor parameter they should be able to leave null.
		if (count <= 1 || this->thread_pool_ == nullptr ||
			this->partitioner_ == nullptr)
		{
			this->draw_views(0, count, renderer, overlay);
			return;
		}

		const std::vector<std::pair<int, int>> ranges =
			this->partitioner_->partition(count,
				this->thread_pool_->max_num_threads());

		for (const std::pair<int, int>& range : ranges)
		{
			this->thread_pool_->add_task([this, range, &renderer, &overlay]()
				{
					this->draw_views(range.first, range.second, renderer,
						overlay);
				});
		}

		this->thread_pool_->wait_for_tasks_to_complete();
	}

	void Scene::draw_views(int start, int end, Renderer& renderer,
		const ViewOverlay& overlay) const
	{
		for (int i = start; i < end; i++)
		{
			const View& view = this->views_[static_cast<size_t>(i)];

			DrawList list = renderer.view(i);
			list.set_viewport(view.viewport);
			list.set_camera(view.camera);

			// What this view can see, in world space. It used to be
			// ViewportManager::camera_adjusted_player_viewport_rect(player_num,
			// camera), which asked the layout for a viewport it had just been
			// handed one of - the arithmetic is the camera's translation and
			// the pane's size divided by its zoom, and it needs no layout at
			// all.
			//
			// It then wrote that arithmetic out inline and MULTIPLIED by the
			// zoom, which the comment above it had already described
			// correctly. Camera::visible_rectangle is the inverse transform,
			// stated once where the forward one lives.
			const RectangleF visible =
				view.camera.visible_rectangle(view.viewport);

			// The cull. "Objects expose bounds; the scene culls" (PHILOSOPHY,
			// Rendering) - and this is the line that makes it true for every
			// view rather than for the ones somebody remembered. The overview
			// pass had no cull while the player panes twenty lines above it
			// did, so the whole post-match menu flow drew 5,230 paint tiles per
			// frame, single-threaded, behind an opaque results box.
			for (const std::unique_ptr<GameObject>& object : this->objects_)
			{
				if (object->bounds().intersects(visible))
				{
					object->draw(list);
				}
			}

			for (const std::unique_ptr<CollisionObject>& object :
				this->collision_objects_)
			{
				if (object->bounds().intersects(visible))
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
