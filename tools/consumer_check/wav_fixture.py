"""Add the direct-WAV first-sound guide to a fresh assembled starter, without building it."""

from __future__ import annotations

import argparse
import json
import re
import sys
import textwrap
from pathlib import Path


GUIDE = Path("website/src/content/docs/docs/guides/your-sound.mdx")
INCLUDE_ANCHOR = '#include "engine/app/application.h"'
FIELD_ANCHOR = "\tlabrador::Label* hint_ = nullptr;"
INIT_ANCHOR = "\tRenderResources* resources = this->app_->render_resources();"
UPDATE_ANCHOR = "\tconst Keyboard& keyboard = *this->app_->keyboard();"
FONT_DEPENDENCY = '        "${CMAKE_CURRENT_SOURCE_DIR}/content/fonts/jetbrains_mono_bold_16.spritefont"'
GAME_DEPENDENCY = "add_dependencies(MyGame game_content)"
INSTALL_ANCHOR = "install(DIRECTORY content/fonts DESTINATION .)"
WAVE_DEPENDENCY = '        "${CMAKE_CURRENT_SOURCE_DIR}/content/waves/ping_wav.wav"'
COPY_WAVES = """add_custom_command(TARGET game_content POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/content/waves"
        "$<TARGET_FILE_DIR:MyGame>/waves"
    VERBATIM
)
"""
ROOT_CMAKE = r'''
# Direct WAV fixture: ordinary starter checks still run before this extra case.
add_executable(MyGameWaveTests game/wav_consumer_tests.cpp)
target_link_libraries(MyGameWaveTests PRIVATE LabradorEngine labrador_settings)
target_compile_definitions(MyGameWaveTests PRIVATE
    "CONSUMER_WAV_DIRECTORY=\"${CMAKE_CURRENT_SOURCE_DIR}/game/content/waves/\"")
if(WIN32)
    target_link_libraries(MyGameWaveTests PRIVATE ole32)
endif()
if(EMSCRIPTEN)
    target_link_options(MyGameWaveTests PRIVATE -sNODERAWFS=1 -sENVIRONMENT=node)
endif()
add_test(NAME MyGameWaveTests COMMAND MyGameWaveTests)
set_tests_properties(MyGameWaveTests PROPERTIES TIMEOUT 60)
'''
CONSUMER_TEST = r'''#include "engine/assets/resource_loader.h"
#include "engine/audio/audio_device.h"
#include "engine/audio/audio_resources.h"
#include "engine/audio/sound_bank.h"

#if defined(_WIN32)
#include <Windows.h>
#include <objbase.h>
#endif

#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

using namespace labrador;

namespace
{
#if defined(_WIN32)
	class ComApartment
	{
	public:
		ComApartment()
		{
			if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
			{
				throw std::runtime_error("could not initialize COM for the native audio device");
			}
		}
		~ComApartment() { CoUninitialize(); }
		ComApartment(const ComApartment&) = delete;
		ComApartment& operator=(const ComApartment&) = delete;
	};
#endif

	void load(ResourceLoader& loader, const std::string& name)
	{
		AssetManifest manifest;
		manifest.source_path = "consumer direct-WAV fixture";
		manifest.entries.push_back(AssetEntry{ "wave", CONSUMER_WAV_DIRECTORY, name, false });
		loader.load_manifest(std::move(manifest));
	}
}

int main()
{
	try
	{
#if defined(_WIN32)
		const ComApartment com;
#endif
		AudioDevice device;
		AudioResources resources;
		ResourceLoader loader(nullptr, nullptr, &resources, &device);
		load(loader, "ping_wav");
		SoundBank* const bank = resources.sound_bank("ping_wav");
		if (!bank->audible() || !bank->resolve_wave("ping_wav").valid() ||
			bank->effect_state(bank->resolve_effect("ping_wav")) != SoundState::stopped)
		{
			throw std::runtime_error("the direct WAV did not become a loaded bank and effect");
		}
		const std::string refused_names[] = { "consumer_missing_wave", "consumer_broken_wave" };
		for (const std::string& name : refused_names)
		{
			bool refused = false;
			try { load(loader, name); }
			catch (const std::exception& error)
			{
				if (std::string(error.what()).find(name) == std::string::npos) throw;
				refused = true;
			}
			if (!refused) throw std::runtime_error("a required missing or malformed WAV was accepted");
			bool published = true;
			try { resources.sound_bank(name); }
			catch (const std::out_of_range&) { published = false; }
			if (published) throw std::runtime_error("a rejected WAV was published");
		}
		if (resources.sound_bank("ping_wav") != bank)
		{
			throw std::runtime_error("a rejected WAV replaced the loaded bank");
		}
		std::puts("consumer direct-WAV loading and required refusal checks passed; no listening claim");
		return 0;
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "consumer WAV fixture: %s\n", error.what());
		return 1;
	}
}
'''


def fragment(guide: str, title: str) -> str:
    """Use the guide's actual paste blocks, so the copied code is what builds."""
    matches = re.findall(r'^[ \t]*```cpp title="' + re.escape(title) + r'"\r?\n(.*?)^[ \t]*```',
                         guide, re.MULTILINE | re.DOTALL)
    if len(matches) != 1:
        raise ValueError(f"The first-sound guide needs one C++ block titled {title!r}.")
    return textwrap.dedent(matches[0]).rstrip("\r\n")


def replace_once(text: str, anchor: str, replacement: str, path: Path) -> str:
    if text.count(anchor) != 1:
        raise ValueError(f"{path} needs exactly one {anchor!r}; update the WAV fixture with the starter.")
    return text.replace(anchor, replacement)


def prepare(project: Path, source: Path | None = None) -> None:
    """Validate every input and anchor before changing the assembled copy."""
    source = source or project / "external/labrador"
    audio_header = source / "engine/audio/audio_device.h"
    loader_source = source / "engine/assets/resource_loader.cpp"
    if "open_memory_bank" not in audio_header.read_text(encoding="utf-8") or 'register_kind("wave"' not in loader_source.read_text(encoding="utf-8"):
        raise ValueError("The direct-WAV fixture needs a development engine with memory banks and wave assets; v0.3.0 does not provide them.")
    guide = (source / GUIDE).read_text(encoding="utf-8")
    include = fragment(guide, "Add the sound-bank include")
    fields = textwrap.indent(fragment(guide, "Keep the bank and wave handle"), "\t")
    initialize = textwrap.indent(fragment(guide, "Resolve the WAV asset"), "\t")
    play = textwrap.indent(fragment(guide, "Play once on Space"), "\t")
    pcm = (source / "samples/audio/source/ping.wav").read_bytes()

    root_path = project / "CMakeLists.txt"
    root = root_path.read_text(encoding="utf-8")
    if root.count("add_subdirectory(game)") != 1:
        raise ValueError("The project must declare game/ once; assemble a fresh starter first.")
    if "MyGameWaveTests" in root:
        raise ValueError("This project already has the WAV fixture; use a fresh starter.")

    game_path = project / "game/CMakeLists.txt"
    game = game_path.read_text(encoding="utf-8")
    game = replace_once(game, FONT_DEPENDENCY, FONT_DEPENDENCY + "\n" + WAVE_DEPENDENCY, game_path)
    game = replace_once(game, GAME_DEPENDENCY, COPY_WAVES + GAME_DEPENDENCY, game_path)
    game = replace_once(game, INSTALL_ANCHOR, INSTALL_ANCHOR + "\ninstall(DIRECTORY content/waves DESTINATION .)", game_path)

    header_path = project / "game/states/hello_state.h"
    header = header_path.read_text(encoding="utf-8")
    header = replace_once(header, INCLUDE_ANCHOR, INCLUDE_ANCHOR + "\n" + include, header_path)
    header = replace_once(header, FIELD_ANCHOR, FIELD_ANCHOR + "\n" + fields, header_path)
    state_path = project / "game/states/hello_state.cpp"
    state = state_path.read_text(encoding="utf-8")
    state = replace_once(state, INIT_ANCHOR, INIT_ANCHOR + "\n" + initialize, state_path)
    state = replace_once(state, UPDATE_ANCHOR, UPDATE_ANCHOR + "\n" + play, state_path)

    manifest_path = project / "game/content/manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assets = manifest.get("assets")
    if not isinstance(assets, list):
        raise ValueError("The game manifest needs its assets list.")
    for group in assets:
        if group.get("kind") in {"wave", "sound_bank"} and "ping_wav" in group.get("names", []):
            raise ValueError("The manifest already names ping_wav; use a fresh starter.")
    assets.append({"kind": "wave", "directory": "./waves/", "names": ["ping_wav"]})

    new_files = {
        project / "game/content/waves/ping_wav.wav": pcm,
        project / "game/content/waves/consumer_broken_wave.wav": b"RIFF",
        project / "game/wav_consumer_tests.cpp": CONSUMER_TEST.encode("utf-8"),
    }
    for path in new_files:
        if path.exists():
            raise ValueError(f"{path} already exists; use a fresh starter.")
    missing = project / "game/content/waves/consumer_missing_wave.wav"
    if missing.exists():
        raise ValueError(f"{missing} exists, so it cannot test a missing asset.")

    edits = {root_path: root + ROOT_CMAKE, game_path: game, header_path: header,
             state_path: state, manifest_path: json.dumps(manifest, indent=4) + "\n"}
    for path, text in edits.items():
        path.write_text(text, encoding="utf-8", newline="\n")
    for path, data in new_files.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path, help="a fresh assembled MyGame project")
    parser.add_argument("--source", type=Path, help="the same development engine source used in the assembled project")
    options = parser.parse_args()
    prepare(options.project.resolve(), options.source.resolve() if options.source else None)
    print("Added the guide's direct-WAV Space control and MyGameWaveTests; configure/build/test, then run the browser with Space,Escape,Enter.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"WAV fixture: {error}", file=sys.stderr)
        sys.exit(2)
