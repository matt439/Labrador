#include "engine/collision/contacts.h"

#include "engine/collision/broad_phase.h"
#include "engine/collision/collision_object.h"
#include "engine/collision/narrow_phase.h"

#include <vector>

using mattmath::Vector2F;

namespace artattack
{
	namespace
	{
		// The three filters, cheapest first, for one candidate pair. Shared so
		// that the swept and the indexed enumerations cannot drift.
		void measure_pair(CollisionObject* a, CollisionObject* b,
			std::vector<Contact>& contacts)
		{
			if (!layers_collide(a->layer(), a->mask(), b->layer(), b->mask()))
			{
				return;
			}

			if (!a->shape()->AABB_intersects(b->shape()))
			{
				return;
			}

			const std::optional<Manifold> manifold =
				narrow_phase(*a->shape(), *b->shape());
			if (!manifold.has_value())
			{
				return;
			}

			contacts.push_back(Contact{ a, b,
				manifold->normal, manifold->penetration });
		}
	}

	void find_contacts(std::span<CollisionObject* const> objects,
		std::vector<Contact>& contacts, BroadPhase* broad_phase)
	{
		contacts.clear();

		if (broad_phase != nullptr)
		{
			// The grid has already dropped every pair whose boxes cannot
			// overlap, and hands back what is left in the same ascending order
			// the sweep below would have produced.
			std::vector<std::pair<int, int>>& pairs =
				broad_phase->pairs_buffer();
			broad_phase->find_pairs(objects, pairs);

			for (const std::pair<int, int>& pair : pairs)
			{
				measure_pair(objects[static_cast<size_t>(pair.first)],
					objects[static_cast<size_t>(pair.second)], contacts);
			}
			return;
		}

		for (size_t i = 0; i < objects.size(); i++)
		{
			CollisionObject* const a = objects[i];
			if (a == nullptr || a->for_deletion())
			{
				continue;
			}

			// From i + 1, so every pair is considered once and no object is
			// tested against itself. The loops this replaces did neither.
			for (size_t j = i + 1; j < objects.size(); j++)
			{
				CollisionObject* const b = objects[j];
				if (b == nullptr || b->for_deletion())
				{
					continue;
				}

				measure_pair(a, b, contacts);
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

			// Measured again, here, because a response moves things.
			//
			// A player standing on the seam between two floor tiles has two
			// contacts of equal depth. Separating from the first ends the
			// second, and handing over the depth that was true before it
			// would push them out twice - a visible pop at every tile seam.
			// The stored manifold is what the frame looked like before
			// anything responded, which is the right thing for a caller
			// inspecting the list and the wrong thing for the caller acting
			// on it.
			const std::optional<Manifold> manifold =
				narrow_phase(*contact.a->shape(), *contact.b->shape());
			if (!manifold.has_value())
			{
				continue;
			}

			contact.a->on_contact(*contact.b, manifold->normal,
				manifold->penetration);
			contact.b->on_contact(*contact.a, -manifold->normal,
				manifold->penetration);
		}
	}
}
