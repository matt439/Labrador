#include "engine/collision/contacts.h"

#include "engine/collision/collision_object.h"
#include "engine/collision/narrow_phase.h"

using mattmath::Vector2F;

namespace artattack
{
	void find_contacts(std::span<CollisionObject* const> objects,
		std::vector<Contact>& contacts)
	{
		contacts.clear();

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

				if (!layers_collide(a->layer(), a->mask(),
					b->layer(), b->mask()))
				{
					continue;
				}

				if (!a->shape()->AABB_intersects(b->shape()))
				{
					continue;
				}

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
