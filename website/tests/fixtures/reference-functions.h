namespace test_first
{
	void shared_name();

	// The first overload has its own section.
	void overloaded(int value);

	// The second overload has a different comment.
	void overloaded(float value);

	void spread_out(int value);

	using Alias = void(int);
	const int initialised = shared_name();
	void (*callback)(int);
	void Owner::member();
	static_assert(true);

	namespace detail
	{
		void hidden();
	}
}

namespace test_second
{
	void shared_name();
}

void global_function();
