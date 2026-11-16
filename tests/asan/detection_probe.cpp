#include <cstddef>
#include <cstring>
#include <memory>
#include <vector>

#if !defined(__SANITIZE_ADDRESS__)
#error The sanitizer detection probe must be instrumented.
#endif

int main(int argument_count, char** arguments)
{
	if (argument_count != 2) return 2;
	if (std::strcmp(arguments[1], "--safe") == 0)
	{
		const std::unique_ptr<int[]> values = std::make_unique<int[]>(4);
		return values[0];
	}
	if (std::strcmp(arguments[1], "--heap-overflow") == 0)
	{
		const std::unique_ptr<int[]> values = std::make_unique<int[]>(4);
		const volatile std::size_t index = 4;
		return values[index];
	}
	if (std::strcmp(arguments[1], "--container-overflow") == 0)
	{
		std::vector<int> values(4, 7);
		values.reserve(8);
		const volatile std::size_t index = values.size();
		// Raw access exercises ASan's poisoned unused capacity independently
		// of the Debug STL's checked operator[]. Annotations must stay on.
		return values.data()[index];
	}
	return 2;
}
