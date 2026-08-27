#include <doctest/doctest.h>

#include "engine/assets/sound_bank_loader.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace labrador;

// What a bank definition is allowed to say, and what comes out of it.
//
// NO DEVICE, IN EVERY CONFIGURATION, WHICH IS NEW AND IS THE POINT.
// read_sound_bank_definition is a parse and nothing else: it produces the
// struct sound_bank_loader.h describes and touches no audio API, so the whole
// of the content half of this module is assertable on a machine with no sound
// card and no XAudio2. It could not be before. The parse used to happen inside
// read_sound_bank, against a DirectX::WaveBank that had to be constructed
// first - so testing what the JSON meant required an .xwb this repository
// cannot contain, and none of it was tested at all.
//
// WHERE THE OTHER HALF IS. build_sound_bank needs a device to create voices
// against, so it is pinned in tests/audio/null_tests.cpp, which compiles in the
// configuration that has the headless backend. The split is the seam's: this
// file is above it and that one is across it.

namespace
{
	// A bank definition on disk, deleted when the test leaves. The same shape
	// sprite_sheet_loader_tests.cpp uses beside it, and deliberately not shared
	// with it for the reason that file already gives: two loaders each need
	// four lines of RAII to be handed a path, and a header holding those four
	// lines would be a third file in the folder earning nothing.
	class TempDefinition
	{
	public:
		explicit TempDefinition(const std::string& contents)
		{
			this->path_ = std::filesystem::temp_directory_path() /
				("labrador_bank_test_" + std::to_string(next_id()) + ".json");

			std::ofstream file(this->path_, std::ios::binary);
			file << contents;
		}

		~TempDefinition()
		{
			std::error_code ignored;
			std::filesystem::remove(this->path_, ignored);
		}

		TempDefinition(const TempDefinition&) = delete;
		TempDefinition& operator=(const TempDefinition&) = delete;

		std::string path() const { return this->path_.string(); }

	private:
		static int next_id()
		{
			static int id = 0;
			return ++id;
		}

		std::filesystem::path path_;
	};
}

TEST_CASE("a definition names its waves and its effect instances")
{
	const TempDefinition definition_file(R"({
		"waves": [ "shot", "hit", "engine" ],
		"create_effect_instance_for_each_wave": false,
		"sound_effect_instances": [
			{ "name": "engine_loop", "wave": "engine" }
		]
	})");

	const SoundBankDefinition definition =
		read_sound_bank_definition(definition_file.path().c_str());

	const std::vector<std::string> waves = { "shot", "hit", "engine" };
	CHECK(definition.waves == waves);

	REQUIRE(definition.effects.size() == 1);
	CHECK(definition.effects[0].name == "engine_loop");
	CHECK(definition.effects[0].wave == "engine");

	// The order of `waves` is the index space a backend answers wave_index out
	// of when it has no container of its own to consult, so it is a property
	// worth pinning rather than an artefact of how the vector was filled.
	CHECK(definition.source_path == definition_file.path());
}

TEST_CASE("an effect may name a wave the wave list did not, and it joins the list")
{
	const TempDefinition definition_file(R"({
		"waves": [ "shot" ],
		"create_effect_instance_for_each_wave": false,
		"sound_effect_instances": [
			{ "name": "engine_loop", "wave": "engine" }
		]
	})");

	const SoundBankDefinition definition =
		read_sound_bank_definition(definition_file.path().c_str());

	// THIS IS THE ONE THING THE PARSE HAD TO LEARN FOR THE SEAM TO WORK. A
	// definition has always been allowed to build an instance over a wave the
	// `waves` array did not list - DirectXTK looked the name up in the .xwb, so
	// the array was advisory. A backend with no container has nothing else to
	// look in, so a wave nothing declares is a wave nothing can find, and the
	// list this parse produces has to be the union rather than the array.
	const std::vector<std::string> waves = { "shot", "engine" };
	CHECK(definition.waves == waves);
}

TEST_CASE("a definition can ask for one instance per wave")
{
	const TempDefinition definition_file(R"({
		"waves": [ "shot", "hit" ],
		"create_effect_instance_for_each_wave": true,
		"sound_effect_instances": [
			{ "name": "engine_loop", "wave": "engine" }
		]
	})");

	const SoundBankDefinition definition =
		read_sound_bank_definition(definition_file.path().c_str());

	// The per-wave instances first and the explicit ones after, which is
	// creation order and therefore handle order.
	REQUIRE(definition.effects.size() == 3);
	CHECK(definition.effects[0].name == "shot");
	CHECK(definition.effects[1].name == "hit");
	CHECK(definition.effects[2].name == "engine_loop");
}

TEST_CASE("two effect instances of one name is a content bug, and it says so")
{
	const TempDefinition definition_file(R"({
		"waves": [ "shot", "hit" ],
		"create_effect_instance_for_each_wave": false,
		"sound_effect_instances": [
			{ "name": "bang", "wave": "shot" },
			{ "name": "bang", "wave": "hit" }
		]
	})");

	// Registry::add refills a name's slot rather than rejecting it, so without
	// this check the second definition would quietly win and the first would
	// have been written for nothing. The throw names the file, because a
	// content bug is found by whoever wrote the content (T6).
	CHECK_THROWS_AS(std::ignore =
		read_sound_bank_definition(definition_file.path().c_str()),
		std::runtime_error);
}

TEST_CASE("a name claimed by a per-wave instance is claimed")
{
	const TempDefinition definition_file(R"({
		"waves": [ "shot" ],
		"create_effect_instance_for_each_wave": true,
		"sound_effect_instances": [
			{ "name": "shot", "wave": "shot" }
		]
	})");

	// The per-wave instances are named after their waves, so an explicit
	// instance may collide with one without repeating itself anywhere a reader
	// would notice. It is the same defect and it gets the same throw.
	CHECK_THROWS_AS(std::ignore =
		read_sound_bank_definition(definition_file.path().c_str()),
		std::runtime_error);
}

TEST_CASE("a repeated wave is one wave")
{
	const TempDefinition definition_file(R"({
		"waves": [ "shot", "shot" ],
		"create_effect_instance_for_each_wave": true,
		"sound_effect_instances": []
	})");

	const SoundBankDefinition definition =
		read_sound_bank_definition(definition_file.path().c_str());

	// A repeat used to be absorbed by Registry::add refilling the slot, which
	// is invisible and produced one instance. A list is not a registry, so it
	// is dropped in the parse instead and the count is the same.
	const std::vector<std::string> waves = { "shot" };
	CHECK(definition.waves == waves);
	CHECK(definition.effects.size() == 1);
}

TEST_CASE("a definition that is not there names itself")
{
	CHECK_THROWS_AS(std::ignore = read_sound_bank_definition(
		"no_such_sound_bank.json"), std::runtime_error);
}
