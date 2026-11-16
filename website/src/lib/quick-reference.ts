// Curate tasks and names here; the existing header reader supplies every
// declaration and the full reference supplies every contract destination.
import { declarationSignature, parseHeader, type Declaration, type Item } from './cpp-header.ts';
import { REFERENCE_HEADERS } from './published.ts';
import { codeLink } from './reference.ts';
import { REVISION, commitUrl, readRepositoryFile, sourceUrl } from './repository.ts';

export const QUICK_REFERENCE_SLUG = 'docs/reference/quick-reference';

export interface ApiSelection {
	header: string;
	name: string;
}

export interface CommonTask {
	title: string;
	summary: string;
	guide: { title: string; url: string };
	api: readonly ApiSelection[];
}

export const COMMON_TASKS: readonly CommonTask[] = [
	{
		title: 'Run and quit the game',
		summary: 'Give the application its first state. The application owns the loop and the platform lifecycle.',
		guide: { title: 'Read your starter\'s C++', url: '/docs/concepts/read-your-code/' },
		api: [
			{ header: 'engine/app/application.h', name: 'Application::run' },
			{ header: 'engine/app/application.h', name: 'Application::quit' },
		],
	},
	{
		title: 'Change screens or open a pause screen',
		summary: 'Replace the current state, push a screen above it, or pop back. Read the contracts for deferral and result delivery.',
		guide: { title: 'Add another screen', url: '/docs/guides/your-screen/' },
		api: [
			{ header: 'engine/core/state_context.h', name: 'StateContext::transition_to' },
			{ header: 'engine/core/state_context.h', name: 'StateContext::push' },
			{ header: 'engine/core/state_context.h', name: 'StateContext::pop' },
		],
	},
	{
		title: 'Load and find a picture',
		summary: 'Load named content from a manifest, then resolve the texture handle a drawable keeps.',
		guide: { title: 'Assets', url: '/docs/guides/assets/' },
		api: [
			{ header: 'engine/assets/resource_loader.h', name: 'ResourceLoader::load_manifest' },
			{ header: 'engine/render/render_resources.h', name: 'RenderResources::resolve_texture' },
		],
	},
	{
		title: 'Read a key or typed text',
		summary: 'Read the fixed update\'s input snapshot. Key positions and UTF-8 typed characters are separate channels; bindings belong to the game.',
		guide: { title: 'Input', url: '/docs/guides/input/' },
		api: [
			{ header: 'engine/input/keyboard.h', name: 'Keyboard::held' },
			{ header: 'engine/input/keyboard.h', name: 'Keyboard::pressed' },
			{ header: 'engine/input/keyboard.h', name: 'Keyboard::typed' },
		],
	},
	{
		title: 'Own objects in a scene',
		summary: 'Transfer ownership to the scene and commit pending changes at the tick boundary. Returned pointers borrow the object\'s scene lifetime.',
		guide: { title: 'The game and its objects', url: '/docs/concepts/game/' },
		api: [
			{ header: 'engine/scene/scene.h', name: 'Scene::add' },
			{ header: 'engine/scene/scene.h', name: 'Scene::remove' },
			{ header: 'engine/scene/scene.h', name: 'Scene::end_tick' },
		],
	},
	{
		title: 'Draw a sprite through a camera',
		summary: 'Record into the supplied view\'s draw list. Draw stays const; order, coordinates, tint and handle lifetime follow the full contract.',
		guide: { title: 'Sprites and text', url: '/docs/guides/sprites-and-text/' },
		api: [
			{ header: 'engine/render/renderer.h', name: 'DrawList::set_camera' },
			{ header: 'engine/render/renderer.h', name: 'DrawList::draw_sprite' },
		],
	},
	{
		title: 'Change displayed words',
		summary: 'Text stores wide characters and measures when the words change. Convert UTF-8 at the edit boundary and check the font\'s coverage.',
		guide: { title: 'Put words on screen', url: '/docs/guides/your-font/' },
		api: [
			{ header: 'engine/render/text.h', name: 'Text::set_text' },
			{ header: 'engine/render/text.h', name: 'Text::set_position' },
		],
	},
	{
		title: 'Play a one-shot sound',
		summary: 'Resolve a wave from the loaded bank, then play it. Browser activation, silent banks and suspended playback have explicit contracts.',
		guide: { title: 'Audio', url: '/docs/guides/audio/' },
		api: [
			{ header: 'engine/audio/sound_bank.h', name: 'SoundBank::resolve_wave' },
			{ header: 'engine/audio/sound_bank.h', name: 'SoundBank::play_wave' },
		],
	},
];

// Return all public overloads of the selected name. A rename or visibility
// change fails with the selection and header, rather than dropping an entry.
export function selectedDeclarations(selection: ApiSelection, source: string): Declaration[] {
	const found: Declaration[] = [];
	const visit = (items: Item[], owners: string[] = []) => {
		for (const item of items) {
			if (item.kind === 'type' && item.access === 'public') {
				visit(item.items, [...owners, item.name]);
			} else if (item.kind === 'group' && item.access === 'public') {
				for (const declaration of item.declarations) {
					if (declaration.namespace === 'labrador' && [...owners, declaration.name].join('::') === selection.name) {
						found.push({ ...declaration, text: declarationSignature(declaration.text) });
					}
				}
			}
		}
	};
	visit(parseHeader(selection.header, source).items);
	if (found.length === 0) {
		throw new Error(`Quick reference selects '${selection.name}', but '${selection.header}' has no such public declaration.`);
	}
	return found;
}

export function quickReferencePage(tasks: readonly CommonTask[] = COMMON_TASKS): string {
	const output = [
		'For game code after the starter is running: eight common tasks, with declarations read from this checkout\'s published headers. This is a lookup page; the linked full contracts own preconditions, errors, lifetimes and platform limits.',
		`Built from [\`${REVISION.short}\`](${commitUrl()})${REVISION.modified ? ' with local changes' : ''}. Match the documentation version to your engine pin; [Which Labrador you have](/docs/guides/your-version/) explains how.`,
		'[Browse all modules and headers](/docs/reference/) or follow a guide for a complete example. Member declarations below appear in their named class; they are not a standalone program. All public overloads of each selected name are shown.',
	];
	for (const task of tasks) {
		output.push(`## ${task.title}`, task.summary);
		const code: string[] = [];
		const contracts: string[] = [];
		for (const selection of task.api) {
			const header = REFERENCE_HEADERS.find((header) => header.source === selection.header && header.audience === 'game code');
			if (!header) throw new Error(`Quick reference selection '${selection.name}' needs a published game-code header: '${selection.header}'.`);
			const declarations = selectedDeclarations(selection, readRepositoryFile(selection.header));
			const contract = codeLink(selection.name, QUICK_REFERENCE_SLUG);
			if (!contract?.startsWith(`/${header.slug}/#`)) {
				throw new Error(`Quick reference selection '${selection.name}' has no full contract in '${selection.header}'.`);
			}
			code.push(`// ${selection.name}`, ...declarations.map((declaration) => declaration.text));
			contracts.push(`[\`${selection.name}\`](${contract}) ([header](${sourceUrl(selection.header, { start: declarations[0].line + 1, end: declarations[0].line + 1 })}))`);
		}
		output.push('```cpp\n' + code.join('\n') + '\n```', `Contracts: ${contracts.join(' · ')}.`, `Guide and example: [${task.guide.title}](${task.guide.url}).`);
	}
	return output.join('\n\n') + '\n';
}
