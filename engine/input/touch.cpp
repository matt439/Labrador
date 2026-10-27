#include "engine/input/touch.h"

namespace labrador
{
	namespace
	{
		bool was_cancelled(const TouchState& state, uint32_t id)
		{
			for (int i = 0; i < state.cancelled_count; ++i)
			{
				if (state.cancelled[i] == id)
				{
					return true;
				}
			}
			return false;
		}
	}

	const TouchPoint* TouchState::find(uint32_t id) const
	{
		if (id == 0)
		{
			return nullptr;
		}
		for (int i = 0; i < this->count; ++i)
		{
			if (this->points[i].id == id)
			{
				return &this->points[i];
			}
		}
		return nullptr;
	}

	bool TouchState::is_down(uint32_t id) const
	{
		return this->find(id) != nullptr;
	}

	std::span<const TouchPoint> TouchState::active() const
	{
		return std::span<const TouchPoint>(this->points,
			static_cast<size_t>(this->count));
	}

	bool pressed(const TouchState& now, const TouchState& before, uint32_t id)
	{
		if (!now.focused || !before.focused)
		{
			return false;
		}
		return now.is_down(id) && !before.is_down(id);
	}

	bool released(const TouchState& now, const TouchState& before, uint32_t id)
	{
		if (!now.focused || !before.focused)
		{
			return false;
		}
		return before.is_down(id) && !now.is_down(id) &&
			!was_cancelled(now, id);
	}

	Vector2I motion(const TouchState& now, const TouchState& before,
		uint32_t id)
	{
		if (!now.focused || !before.focused)
		{
			return Vector2I::ZERO;
		}
		const TouchPoint* const to = now.find(id);
		const TouchPoint* const from = before.find(id);
		if (to == nullptr || from == nullptr)
		{
			return Vector2I::ZERO;
		}
		return to->position - from->position;
	}

	bool just_focused(const TouchState& now, const TouchState& before)
	{
		return now.focused && !before.focused;
	}

	bool just_unfocused(const TouchState& now, const TouchState& before)
	{
		return !now.focused && before.focused;
	}

	int Touch::find_live(uint32_t platform_id) const
	{
		for (int i = 0; i < this->live_count_; ++i)
		{
			if (!this->live_[i].lifted &&
				this->live_[i].platform_id == platform_id)
			{
				return i;
			}
		}
		return -1;
	}

	void Touch::remove_live(int index)
	{
		// Shifted rather than swapped with the last, so the order the points
		// went down in survives - which is the order TouchState promises.
		for (int i = index + 1; i < this->live_count_; ++i)
		{
			this->live_[i - 1] = this->live_[i];
		}
		--this->live_count_;
	}

	void Touch::cancel_live(int index)
	{
		// Only a point a frame has seen can have an edge to suppress. One that
		// came and went between polls is simply never seen.
		if (this->live_[index].latched &&
			this->pending_cancelled_count_ < TouchState::capacity)
		{
			this->pending_cancelled_[this->pending_cancelled_count_++] =
				this->live_[index].point.id;
		}
		this->remove_live(index);
	}

	void Touch::on_down(uint32_t platform_id,
		const Vector2I& position)
	{
		if (!this->live_focused_)
		{
			return;
		}

		const int existing = this->find_live(platform_id);
		if (existing >= 0)
		{
			this->cancel_live(existing);
		}

		// The overflow rule TouchState::capacity states. Nothing is recorded
		// about the refused point: every later message for its platform id
		// finds no contact and is dropped, which is the rule.
		if (this->live_count_ == TouchState::capacity)
		{
			return;
		}

		Contact& contact = this->live_[this->live_count_++];
		contact = Contact();
		contact.platform_id = platform_id;
		contact.point.id = this->next_id_;
		contact.point.position = position;

		++this->next_id_;
		if (this->next_id_ == 0)
		{
			this->next_id_ = 1;
		}
	}

	void Touch::on_move(uint32_t platform_id,
		const Vector2I& position)
	{
		const int index = this->find_live(platform_id);
		if (index >= 0)
		{
			this->live_[index].point.position = position;
		}
	}

	void Touch::on_up(uint32_t platform_id, const Vector2I& position)
	{
		const int index = this->find_live(platform_id);
		if (index < 0)
		{
			return;
		}

		// A point a frame has already seen ends now, and its release is the
		// next poll's. One no frame has seen stays, lifted, until a poll has
		// latched it once - the tap the header promises is not lost.
		if (this->live_[index].latched)
		{
			this->remove_live(index);
			return;
		}
		this->live_[index].point.position = position;
		this->live_[index].lifted = true;
	}

	void Touch::on_cancel(uint32_t platform_id)
	{
		const int index = this->find_live(platform_id);
		if (index >= 0)
		{
			this->cancel_live(index);
		}
	}

	void Touch::cancel_points()
	{
		while (this->live_count_ > 0)
		{
			this->cancel_live(this->live_count_ - 1);
		}
	}

	void Touch::set_focused(bool focused)
	{
		this->live_focused_ = focused;
		if (!focused)
		{
			// Every point a frame has seen ends cancelled, not released. The
			// both-frames rule covers only a poll that sees the unfocused
			// frame, and a hidden page, which gets no frames, or an alt-tab
			// inside one frame has none, so the next focused frame would
			// otherwise release them. Ids already pending are kept.
			this->cancel_points();
		}
	}

	void Touch::poll()
	{
		this->previous_ = this->current_;

		TouchState next;
		next.focused = this->live_focused_;
		next.count = this->live_count_;
		for (int i = 0; i < this->live_count_; ++i)
		{
			next.points[i] = this->live_[i].point;
		}
		next.cancelled_count = this->pending_cancelled_count_;
		for (int i = 0; i < this->pending_cancelled_count_; ++i)
		{
			next.cancelled[i] = this->pending_cancelled_[i];
		}
		this->current_ = next;
		this->pending_cancelled_count_ = 0;

		// Every contact is now latched, and the lifted ones have had their one
		// frame: they leave the live set, and the next poll releases them.
		int kept = 0;
		for (int i = 0; i < this->live_count_; ++i)
		{
			if (this->live_[i].lifted)
			{
				continue;
			}
			this->live_[kept] = this->live_[i];
			this->live_[kept].latched = true;
			++kept;
		}
		this->live_count_ = kept;
	}

	const TouchState& Touch::state() const
	{
		return this->current_;
	}

	const TouchState& Touch::previous_state() const
	{
		return this->previous_;
	}

	std::span<const TouchPoint> Touch::points() const
	{
		return this->current_.active();
	}

	bool Touch::held(uint32_t id) const
	{
		return this->current_.focused && this->current_.is_down(id);
	}

	bool Touch::pressed(uint32_t id) const
	{
		return labrador::pressed(this->current_, this->previous_, id);
	}

	bool Touch::released(uint32_t id) const
	{
		return labrador::released(this->current_, this->previous_, id);
	}

	Vector2I Touch::motion(uint32_t id) const
	{
		return labrador::motion(this->current_, this->previous_, id);
	}

	bool Touch::focused() const
	{
		return this->current_.focused;
	}

	bool Touch::just_focused() const
	{
		return labrador::just_focused(this->current_, this->previous_);
	}

	bool Touch::just_unfocused() const
	{
		return labrador::just_unfocused(this->current_, this->previous_);
	}
}
