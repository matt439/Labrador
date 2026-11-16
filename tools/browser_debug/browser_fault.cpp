#include <emscripten/emscripten.h>
#include <cstdio>

extern "C" EMSCRIPTEN_KEEPALIVE int browser_score(int player_count)
{
	int points_per_player = 10;
	int total_points = player_count + points_per_player;
	std::printf("score: %d players produced %d points\n", player_count, total_points);
	return total_points;
}

int main()
{
	std::printf("stdout: browser debugging example ready\n");
	std::fprintf(stderr, "stderr: intentional diagnostic example\n");
	return 0;
}
