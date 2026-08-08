#include "engine/collision/contacts.h"

#include "engine/collision/collision_object.h"
#include "engine/collision/narrow_phase.h"

#include <algorithm>

using mattmath::Vector2F;

namespace
{
	// One live object, reduced to the numbers the broad phase needs: a flat
	// POD with no vptr, so the sweep walks contiguous memory and never calls
	// back into a shape.
	struct Candidate
	{
		float min_x = 0.0f;
		float max_x = 0.0f;
		float min_y = 0.0f;
		float max_y = 0.0f;

		// The same interval along whichever axis was chosen to sweep, copied
		// once so the inner loop needs no branch to know which axis it is on.
		float sweep_min = 0.0f;
		float sweep_max = 0.0f;

		// Position in the caller's span. Pairs are handed to the filters in
		// this order, so `a` is still whichever object the caller listed
		// first and a contact normal keeps the meaning it always had.
		int index = 0;
	};

	// True when x is the better axis to sweep: the one whose centres are more
	// spread out, because that is the one where sorted intervals overlap
	// least (Ericson, 7.5).
	//
	// Variance is taken about the mean rather than as the mean of squares.
	// Levels run to 6200 units, 6200 squared is 3.8e7, and summing fifteen
	// hundred of those in a float loses the spread entirely - the quantity
	// being measured is the small difference between two large numbers, which
	// is the classic way to compute zero.
	bool sweep_on_x(const std::vector<Candidate>& candidates)
	{
		double mean_x = 0.0;
		double mean_y = 0.0;
		for (const Candidate& c : candidates)
		{
			mean_x += 0.5 * (static_cast<double>(c.min_x) + c.max_x);
			mean_y += 0.5 * (static_cast<double>(c.min_y) + c.max_y);
		}
		mean_x /= static_cast<double>(candidates.size());
		mean_y /= static_cast<double>(candidates.size());

		double spread_x = 0.0;
		double spread_y = 0.0;
		for (const Candidate& c : candidates)
		{
			const double dx = 0.5 * (static_cast<double>(c.min_x) + c.max_x) - mean_x;
			const double dy = 0.5 * (static_cast<double>(c.min_y) + c.max_y) - mean_y;
			spread_x += dx * dx;
			spread_y += dy * dy;
		}

		// Ties go to x, which matters only for scenes with no spread at all.
		return spread_x >= spread_y;
	}
}

namespace artattack
{
	void find_contacts(std::span<CollisionObject* const> objects,
		std::vector<Contact>& contacts)
	{
		SweepCounts discarded;
		find_contacts(objects, contacts, discarded);
	}

	void find_contacts(std::span<CollisionObject* const> objects,
		std::vector<Contact>& contacts, SweepCounts& counts)
	{
		contacts.clear();
		counts = SweepCounts{};

		// Scratch, kept across calls so a steady frame allocates nothing after
		// the first busy one - the same bargain `contacts` makes by being a
		// reference. thread_local because the engine already commits to a
		// multi-core update, and one buffer per thread is cheaper to reason
		// about than one shared buffer plus a lock.
		static thread_local std::vector<Candidate> candidates;
		candidates.clear();

		for (size_t i = 0; i < objects.size(); i++)
		{
			CollisionObject* const object = objects[i];
			if (object == nullptr || object->for_deletion())
			{
				continue;
			}

			// The shape's own box, not GameObject::bounds(). bounds() is the
			// coarse extent documented for a broad phase to index, but nothing
			// enforces that it contains the shape, and an index that is
			// smaller than the geometry misses collisions silently. This is
			// the exact quantity the filter below already trusts, so the two
			// cannot disagree.
			const mattmath::RectangleF box = object->shape()->bounding_box();

			Candidate candidate;
			candidate.min_x = box.left();
			candidate.max_x = box.right();
			candidate.min_y = box.top();
			candidate.max_y = box.bottom();
			candidate.index = static_cast<int>(i);
			candidates.push_back(candidate);
		}

		if (candidates.size() < 2)
		{
			return;
		}

		const bool on_x = sweep_on_x(candidates);
		for (Candidate& candidate : candidates)
		{
			candidate.sweep_min = on_x ? candidate.min_x : candidate.min_y;
			candidate.sweep_max = on_x ? candidate.max_x : candidate.max_y;
		}

		// Sorted on the minimum, which is the whole trick. Sorting on the
		// maximum reads just as plausibly and silently loses containment: a
		// wide platform that spans a whole level would sort last and never be
		// reached from the objects standing on it.
		std::sort(candidates.begin(), candidates.end(),
			[](const Candidate& lhs, const Candidate& rhs)
			{
				return lhs.sweep_min < rhs.sweep_min;
			});

		for (size_t i = 0; i < candidates.size(); i++)
		{
			const Candidate& first = candidates[i];

			for (size_t j = i + 1; j < candidates.size(); j++)
			{
				const Candidate& second = candidates[j];

				// Everything after this starts later still, so nothing beyond
				// j can reach back to overlap i. This break is what makes the
				// sweep worth doing; without it this is all-pairs with extra
				// steps.
				if (second.sweep_min > first.sweep_max)
				{
					break;
				}

				counts.pairs_enumerated++;

				// Back to the caller's ordering, so which object is `a` does
				// not depend on how the sort happened to arrange them.
				const size_t index_a = static_cast<size_t>(
					std::min(first.index, second.index));
				const size_t index_b = static_cast<size_t>(
					std::max(first.index, second.index));
				CollisionObject* const a = objects[index_a];
				CollisionObject* const b = objects[index_b];

				if (!layers_collide(a->layer(), a->mask(),
					b->layer(), b->mask()))
				{
					continue;
				}

				counts.bound_tests++;
				if (!a->shape()->AABB_intersects(b->shape()))
				{
					continue;
				}

				counts.narrow_tests++;
				const std::optional<Manifold> manifold =
					narrow_phase(*a->shape(), *b->shape());
				if (!manifold.has_value())
				{
					continue;
				}

				contacts.push_back(Contact{ a, b,
					manifold->normal, manifold->penetration });
			}
		}
	}

	void dispatch_contacts(std::span<const Contact> contacts)
	{
		for (const Contact& contact : contacts)
		{
			if (contact.a->for_deletion() || contact.b->for_deletion())
			{
				continue;
			}

			contact.a->on_contact(*contact.b, contact.normal,
				contact.penetration);
			contact.b->on_contact(*contact.a,
				Vector2F(-contact.normal.x, -contact.normal.y),
				contact.penetration);
		}
	}
}
