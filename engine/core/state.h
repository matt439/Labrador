#pragma once

namespace labrador
{
	class Renderer;
	class StateContext;

	class State
	{
	public:
		State() = default;
		virtual ~State() = default;

		// dt arrives as a parameter, for the same reason GameObject::update
		// takes one: reading it off a member meant the shell owned a heap float
		// and handed a const float* to everything that wanted the frame time -
		// eleven classes, their constructors, and the builders above those.
		//
		// Only the top of the stack is updated. A state with something above it
		// is suspended: see on_suspend().
		virtual void update(float dt) = 0;

		// const, and taking the renderer.
		//
		// const is not decoration here. Every draw below this line runs on the
		// same objects from every render worker at once, and this is the line
		// where the compiler starts holding them to it - MenuPage's non-const
		// draw helpers, running on sixteen threads, become a compile error
		// rather than a comment.
		//
		// The state declares how many views this frame has and fills them; it
		// does not submit. begin_frame / submit / end_frame belong to whoever
		// owns the frame, which is the shell.
		virtual void draw(Renderer& renderer) const = 0;

		virtual void init() = 0;

		// Whether this state fills the frame on its own.
		//
		// The stack draws from the topmost state that says yes, upward - so a
		// pause menu, which is a box over a running match, says no and the match
		// underneath keeps drawing. Everything else is a screen and the default
		// is therefore true: a state that replaced the whole frame and forgot to
		// say so would be drawn over a stale one below it, which is a bug you
		// see; a state that covers the screen and pays for one hidden draw of
		// what is under it is a bug you do not.
		//
		// It is asked once per frame, before any drawing, so it is not on the
		// per-object path and there is no T8 cost to it being virtual.
		virtual bool covers_screen() const { return true; }

		// Something was pushed above this state. It stops receiving update()
		// and - unless the state above covers the screen - keeps receiving
		// draw() until that thing pops.
		//
		// This is where "quiet down while something is above me" goes, and it
		// is the half of the pause bug no amount of not-calling-update fixes:
		// a looping voice keeps playing precisely because the update that would
		// have stopped it is the one being skipped.
		virtual void on_suspend() {}

		// Whatever was above this state popped, and it is the top again. Runs
		// before the result callback the push was given, so the callback sees a
		// stack that has already finished changing shape.
		virtual void on_resume() {}

		// The application got or lost the foreground: alt-tab, minimise, a
		// power suspend, or coming back from any of them.
		//
		// A DIFFERENT QUESTION FROM on_suspend, and the pair above is exactly
		// why this one had to exist. on_suspend means "something was pushed
		// above me" and is asked of one state; these mean "nobody is looking
		// at any of us", which is a fact about the window that no state can
		// see. The shell used to keep it entirely to itself - it suspended the
		// pad reader and told the stack nothing - and a suspended reader
		// answers "disconnected" for every slot, which becomes a neutral
		// input, which is a legal input that nothing downstream questions. So
		// a match alt-tabbed out of played itself out with nobody in it: the
		// characters standing still, the clock running down, and the music at
		// full volume over whatever the player had switched to.
		//
		// EVERY FRAME IS TOLD, top down, not the top one alone. A match under
		// a pause menu is not being updated and is still holding the music and
		// a looping weapon voice, so the frame that most needs to hear this is
		// the one furthest from the top.
		//
		// AN EDGE, so a state may pause here and resume there without keeping
		// a bool of its own. Windows offers no such guarantee - deactivating
		// and then minimising is two separate messages that both mean "not in
		// front of the player" - and StateContext::notify_activation is where
		// the sources collapse into one question, so it is where the edge is
		// made. A state constructed while the application is already inactive
		// missed the edge and is told nothing; it asks StateContext::active().
		virtual void on_activated() {}
		virtual void on_deactivated() {}

		void set_context(StateContext* context);
	protected:
		StateContext* context() const;
	private:
		StateContext* context_ = nullptr;
	};
}
