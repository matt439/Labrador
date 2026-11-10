# Contact responses that reach the other party: issue #124

**Draft, 2026-11-10. No verdict.** This file sets out the evidence, the
options and a recommendation for
[issue #124](https://github.com/matt439/Labrador/issues/124). The decision is
the owner's, and the issue stays open until it is made. Nothing here is
implemented, and no header, test or guide has changed.

The request is for a defined way for one participant's contact response to
change the other participant. `CollisionObject::on_contact` hands the
responding object a `const CollisionObject& other`, so a response can read the
other party and write only itself. The client's case is a pair in which only
one side can work out the result of the contact and the other side needs that
result after the tick's contacts have all been dispatched. Its workaround is a
`const_cast` on `other`. It names two alternatives it rejected: `mutable`
state on the receiving class, and a record keyed by identity that the game
settles after `Scene::resolve()`.

## What the engine has now

Rechecked at `043589ee4f2a8d0ec872b9b1223558103541b993`. Line numbers will
drift, so recheck a citation before acting on it.

- [`on_contact`](../../../engine/collision/collision_object.h) takes
  `const CollisionObject& other, const Vector2F& normal, float penetration`
  (`engine/collision/collision_object.h:49-50`). Its comment promises that
  the two participants "are told the same overlap with opposite normals, and
  each is told exactly once" (`:38-41`). It does not say why `other` is
  `const`, and no design document does either: PHILOSOPHY's *Collision*
  section (`docs/design/PHILOSOPHY.md:644-653`) gives the game "what it
  *means*" and is silent on which party a response may write.
- [`Contact`](../../../engine/collision/contacts.h) holds both participants
  as non-const `CollisionObject*` (`engine/collision/contacts.h:21-22`).
  [`dispatch_contacts`](../../../engine/collision/contacts.cpp) calls
  `contact.a->on_contact(*contact.b, ...)` and then
  `contact.b->on_contact(*contact.a, ...)`
  (`engine/collision/contacts.cpp:111-114`). The caller holds both objects
  mutably; the constness is introduced at the virtual's signature and
  nowhere else. The issue's reading of the code is correct.
- Every object a `Scene` dispatches to is owned non-const
  (`std::unique_ptr<CollisionObject>`, `engine/scene/scene.h:296`), so the
  client's cast writes to an object that is not `const`. It is defined
  behaviour today. What it lacks is the engine's word: nothing in a public
  header says a participant is never a `const` object.
- Dispatch runs both halves of a pair it dispatches, with no check between
  them. A pair is skipped when either participant is already retired, and
  measured again before it is told (`engine/collision/contacts.cpp:89-109`).
  If the first half retires its own object, the second half still runs:
  `tests/collision/contacts_tests.cpp`, *a contact is dropped when an earlier
  response retired a participant*, has the bullet, listed first, retire
  itself, and still requires the wall to have been told once. Both halves
  receive the one measurement taken before either responded.
- Which participant is `a` is the order of the objects handed to
  `find_contacts`: the earlier one (`engine/collision/contacts.cpp:62-80`,
  and the broad phase returns pairs "in the same ascending order", `:47-49`).
  In a `Scene` that is the order collision objects were added. No header
  states it as contract.
- The list is a value on purpose. `contacts.h:15-18` says a scene "can hand
  it to the game after resolution instead of during it", and
  [`Scene::contacts()`](../../../engine/scene/scene.h) returns it until
  `end_tick()` (`engine/scene/scene.h:215-227`). The span is
  `std::span<const Contact>`, but each `Contact`'s pointers are non-const, so
  a game reading the list holds both participants mutably. `scene.h:229-240`
  gives the time between `resolve()` and `end_tick()` to the game as its own
  phase, and declines a per-object virtual for it on T8.
- The list is not the set of contacts that were dispatched. Its depths
  "describe the frame before anything responded to it"
  (`engine/collision/contacts.h:70-72`), and so does its membership: dispatch
  reads the list and never edits it, so it still holds the pairs dispatch
  dropped because an earlier response retired a participant or separated
  the pair. Nothing records which entries were told. After `resolve()`, `for_deletion()` cannot recover it, because
  an object retired by its own contact and an object retired before its
  contact was reached answer the same.
- In this tree `on_contact` is overridden in the collision sample
  (`samples/collision/body.h`, `body.cpp`), in the test doubles
  (`tests/collision/stub_collider.h`, `contacts_tests.cpp`,
  `broad_phase_tests.cpp`, `tests/scene/scene_tests.cpp`,
  `samples/collision/arena_tests.cpp`), in the scene benchmark
  (`bench/scene_bench.cpp`) and in two website guides that quote
  or spell the signature (`guides/collision.mdx`, `guides/your-collision.mdx`).
  Each reads `other`'s tag, records its address or ignores it.

## What the request is and is not

It is a request about the virtual's signature, and it is a real client's:
T1's test, "mechanisms are extracted from a real client game in hand", is met
by one client with one cast. It is not a report of undefined behaviour, for
the reason above, so T2 does not settle it by itself: the engine is not
choosing between a sound design and an unsound one. The question is which
statement the type should make, and what the engine promises about the two
halves of a pair once one of them may write the other.

`const` on `other` does make a statement. T5 lists `const` among the walls
that are meant to be boring, and what this one says is that a response writes
only its own object. It does not make a response independent of the order of
the two halves: a response may already read state the other party's own half
wrote a moment earlier, and whether it sees it depends on which of the two
was added to the scene first. The wall stops a write across the pair. It does
not stop a read that depends on order.

## The options

**A. Keep the signature, and document the list as the route.** No source
changes. `collision_object.h` and the collision guide say that a response
which must change the other party is written in the game's phase between
`resolve()` and `end_tick()`, as a loop over `Scene::contacts()`, whose
participants are mutable there.

- For: no break, no new name, and it is the design `contacts.h` already
  argues for. T4: each class's response stays in that class.
- Against: the list holds pairs that were never told, and nothing says which.
  A game that settles from it must repeat dispatch's two filters by hand, and
  cannot repeat the retirement one exactly. So the route is defined only for
  responses that are right for every measured pair, which is not the case
  the client has. It also moves a pairwise response out of the virtual into
  a loop that downcasts both participants by tag, where the virtual receives
  one of them already typed as `this`.
- A game can avoid the list: the side that computes the result keeps it,
  keyed by the other participant's address, and the other side asks for it
  through a `const` query in the game's phase. That needs no engine change
  and no cast. It is the side channel the issue describes, and its cost is a
  per-object store and a second pass the game writes.

**B. Make `other` non-const.** `on_contact(CollisionObject& other, ...)`.

- For: the smallest change that answers the request, and the one a newcomer
  expects. The public face's test is whether the API can be explained in an
  introductory series; "a bullet's response damages what it hit" is the
  first thing such a reader writes. It adds no call, no branch and no
  indirection, so T8 is not engaged. Dispatch needs no change: both halves
  of a dispatched pair already run, with one measurement.
- For: the break is loud and mechanical (T5). Every override stops compiling
  until `const` is removed from its first parameter; an override marked
  `override` is refused by name, and one that is not leaves its class
  abstract. No override keeps compiling with a changed meaning.
- Against: it is a source break for every `CollisionObject` in every client,
  for the benefit of the responses that need it. The public face asks that
  breaks be "deliberate, batched and documented", so it belongs in a tag
  that already carries others.
- Against: the type stops saying that a response writes only its receiver,
  and the order of the two halves becomes easier to depend on. The contract
  then has to say three things the header does not say today, each of which
  is what the code already does:
  1. Both halves of a dispatched pair run. A response that retires either
     participant drops that object's later pairs, not the other half of this
     one. "Each is told exactly once" stays literally true.
  2. Both halves receive the measurement taken before either responded. A
     response that moves the other party does not change what that party is
     then told.
  3. Which half runs first is either stated (the participant added earlier)
     or declared unspecified. Unspecified is the safer promise: a write
     across the pair should leave a fact the other party acts on after the
     tick's contacts, not one its own half must already have seen.
- Tests that would ship with it: a response writes the other participant and
  the write is visible after dispatch; a first half that retires the other
  participant still leaves the second half told once and drops that object's
  later pairs; the existing separation and retirement cases unchanged.

**C. Say which contacts were told.** Keep `const`, and close option A's gap:
`Contact` gains a flag that dispatch sets, or the scene keeps a second list,
so the game's phase can walk exactly the pairs that were dispatched with both
participants mutable.

- For: no break to `on_contact`. It makes the documented route (the list)
  sufficient for the case in hand, and the same record is what a contact
  begin/end helper (`IDEA#9`, an open decision in TODO.md) would read.
- Against: it is a second mechanism for one response, and the response still
  leaves the virtual. `dispatch_contacts` takes `std::span<const Contact>`,
  so a flag written by dispatch changes that signature for callers that hold
  their own list; a second list costs a push per dispatched pair (small, and
  on the tick path). The measurements in the list would still be the
  pre-response ones unless that is changed too.

**D. A second virtual for responses that cross the pair.** The issue names
it as an alternative. This draft does not recommend it: a hook with an
empty default body is called for every contact of every object to serve the
few that override it, which is the tax `scene.h:229-240` already refuses on T8 for the game's phase, and two
virtuals for one event fail T4.

## Recommendation

**B, landed with the three contract sentences above and their tests, in the
next tag that already carries a source break; and option A's two sentences
about the list added to `collision_object.h` whichever way the verdict goes**,
because they are true today and the header does not say them.

The argument for B over A is that A's route does not serve the case that was
reported: the list cannot tell a pair that was told from one that was
dropped. The argument for B over C is T3 and T4: one parameter loses a
qualifier, where C adds a record and leaves the game to write the walk. The
argument against B is real and is the owner's to weigh: it gives up a wall
T5 values, for a client that has one cast and has said it will keep that
cast, documented, if the answer is no.

If the verdict is to decline, the decline is option A with its limits
written down: `collision_object.h` states that `other` is `const` so that a
response writes only its receiver, that every participant a `Scene`
dispatches to is a non-const object, and that a response needing more is
settled in the game's phase. That answer rests on T5 and on the told-once
rule, as the issue itself anticipates, not on the number of clients asking.

## What a verdict changes

| Verdict | Engine change | Record |
|---|---|---|
| B | `collision_object.h` signature and contract, `contacts.h` comment, the collision sample, five test files, the scene benchmark, two guides | CHANGES.md source break naming `const CollisionObject& other` and `CollisionObject& other`, with the sample's diff as the migration; a line in the minimal sample's README is not needed, since the template has no collision object |
| A (decline) | Comments in `collision_object.h` and a paragraph in the collision guide | Not consumer-visible; the issue closes as declined with the trade-off named |
| C | `contacts.h`, `contacts.cpp`, `scene.h`, tests, the collision guide | CHANGES.md entry; a source break only if `dispatch_contacts`' parameter changes |

A signature break under B is also the condition TODO.md's `REV#81` (a
contact point in `Manifold`, `Contact` and `on_contact`, deferred) names for
revisiting it, so the two would be weighed for one tag.

Under B or C the client can delete its cast and the comments that explain
it. Under A it keeps a workaround of its own choosing.

## Not verified

- No code was compiled or run for this draft. The statements about dispatch
  are read from `contacts.cpp` and from the existing test's assertions.
- That the minimal template has no collision object rests on a search of
  `samples/minimal` for `CollisionObject`, which finds nothing.
- How many of the client's classes override `on_contact`, and so the size of
  its migration under B, is the client's to say.
