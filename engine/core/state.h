#pragma once

class StateContext;

class State
{
public:
	State() = default;
	virtual ~State() = default;
	virtual void update() = 0;
	virtual void draw() = 0;
	virtual void init() = 0;
	void set_context(StateContext* context);
protected:
	StateContext* get_context() const;
private:
	StateContext* context_ = nullptr;
};
