#pragma once

namespace artattack
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
		void set_context(StateContext* context);
	protected:
		StateContext* context() const;
	private:
		StateContext* context_ = nullptr;
	};
}
