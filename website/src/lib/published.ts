// What the site publishes from the rest of the checkout, and where.
//
// This is the explicit set the website plan asks for: a document is on the site
// because it is listed here, never because it happens to be under docs/. The
// historical reviews, the surveys, the performance evidence and the planning
// documents stay in the repository and are linked to there.
//
// A relative link inside any published document is resolved against this
// list: a target that is published becomes a link to its page, and anything
// else becomes a link to the file on GitHub at the built revision
// (remark-checkout-links.ts).

export interface PublishedDocument {
	// The file in the repository, from the root.
	source: string;
	// The site path, without leading or trailing slashes.
	slug: string;
	title: string;
	description: string;
	order: number;
}

export const DESIGN_DOCUMENTS: PublishedDocument[] = [
	{
		source: 'docs/design/PHILOSOPHY.md',
		slug: 'docs/design/philosophy',
		title: 'Philosophy',
		description:
			'The twelve trade-offs Labrador makes, the price of each, and the engine they describe.',
		order: 1,
	},
	{
		source: 'docs/design/ARCHITECTURE.md',
		slug: 'docs/design/architecture',
		title: 'Architecture',
		description: 'The targets, the tree, the module table, and where the engine/game boundary runs.',
		order: 2,
	},
	{
		source: 'docs/design/CONVENTIONS.md',
		slug: 'docs/design/conventions',
		title: 'Conventions',
		description: 'Naming, files, and what a comment is for.',
		order: 3,
	},
];

export type ReferenceAudience = 'game code' | 'renderer writer';

export const REFERENCE_AUDIENCES: ReferenceAudience[] = ['game code', 'renderer writer'];

export interface ReferenceHeader {
	module: string;
	// The header, from the repository root.
	source: string;
	slug: string;
	// The sidebar label: the primary type the header declares, or what it is
	// for when it declares no type.
	title: string;
	order: number;
	audience: ReferenceAudience;
}

export interface ReferenceModule {
	// The folder under engine/, which is also the module's name.
	name: string;
	// What the module is, for the reference's introduction and module pages.
	summary: string;
	// The headers, by file name, in the order a reader meets them, each with
	// its sidebar label.
	headers: [file: string, title: string, audience: ReferenceAudience][];
}

// The modules the reference publishes, in ARCHITECTURE's dependency order, and
// every public header in each: a header is on the site because it is listed
// here.
//
// A module is published once its headers say what the code is and why, and
// nothing about what it used to be: the reference shows a header's comments as
// written, so history in them would read as contract (CONVENTIONS, Comments).
// Backend folders hold no public API and are never listed.
//
// The site build fails on a public header that is in neither list (reference.ts,
// unlistedHeaders), so a new one is published or excluded in the commit that
// creates it.
export const REFERENCE_MODULES: ReferenceModule[] = [
	{
		name: 'math',
		summary: 'Vectors, rectangles, shapes, intersection tests and 2D affine transforms. Depends on nothing.',
		headers: [
			['vector2f.h', 'Vector2F', 'game code'],
			['vector2i.h', 'Vector2I', 'game code'],
			['rectanglef.h', 'RectangleF', 'game code'],
			['rectanglei.h', 'RectangleI', 'game code'],
			['matrix3x2f.h', 'Matrix3x2F', 'game code'],
			['scalar.h', 'Scalars and tolerance', 'game code'],
			['shape.h', 'Shape', 'game code'],
			['shape_type.h', 'ShapeType', 'game code'],
			['circle.h', 'Circle', 'game code'],
			['triangle.h', 'Triangle', 'game code'],
			['quad.h', 'Quad', 'game code'],
			['rectangle_rotated.h', 'RectangleRotated', 'game code'],
			['segment.h', 'Segment', 'game code'],
			['intersects.h', 'Intersection tests', 'game code'],
			['inflate.h', 'Inflating shapes', 'game code'],
			['ericson_math.h', 'Ericson routines', 'game code'],
		],
	},
	{
		name: 'core',
		summary: 'Game objects, the state stack, handles and the tables that issue them, the thread pool, and a seeded random stream.',
		headers: [
			['state.h', 'State', 'game code'],
			['state_context.h', 'StateContext', 'game code'],
			['game_object.h', 'GameObject', 'game code'],
			['handle.h', 'Handle', 'game code'],
			['registry.h', 'Registry', 'game code'],
			['name_table.h', 'NameTable', 'game code'],
			['thread_pool.h', 'ThreadPool', 'game code'],
			['byte_reader.h', 'ByteReader', 'game code'],
			['file_path.h', 'UTF-8 paths', 'game code'],
			['step_position.h', 'StepPosition', 'game code'],
			['random.h', 'Random', 'game code'],
		],
	},
	{
		name: 'collision',
		summary: 'Layers and masks, a broad phase, a narrow phase that measures contacts, and analytic resolution.',
		headers: [
			['collision_object.h', 'CollisionObject', 'game code'],
			['collision_layer.h', 'Layers and masks', 'game code'],
			['contacts.h', 'Contact', 'game code'],
			['manifold.h', 'Manifold', 'game code'],
			['narrow_phase.h', 'Narrow phase', 'game code'],
			['resolve.h', 'Resolution', 'game code'],
			['broad_phase.h', 'BroadPhase', 'game code'],
			['tunnelling.h', 'Tunnelling', 'game code'],
			['partitioner.h', 'Partitioner', 'game code'],
		],
	},
	{
		name: 'render',
		summary: 'The renderer, draw lists, cameras and viewports, sprites, text, colours, and the files content arrives in.',
		headers: [
			['renderer.h', 'Renderer', 'game code'],
			['render_resources.h', 'RenderResources', 'game code'],
			['camera.h', 'Camera', 'game code'],
			['viewport.h', 'Viewport', 'game code'],
			['colour.h', 'Colour', 'game code'],
			['label.h', 'Label', 'game code'],
			['text.h', 'Text', 'game code'],
			['text_object.h', 'TextObject', 'game code'],
			['font.h', 'Font', 'game code'],
			['text_encoding.h', 'Text encoding', 'game code'],
			['texture_object.h', 'TextureObject', 'game code'],
			['sprite_sheet.h', 'SpriteSheet', 'game code'],
			['sprite_sheet_object.h', 'SpriteSheetObject', 'game code'],
			['sprite_frame.h', 'SpriteFrame', 'game code'],
			['animation_strip.h', 'AnimationStrip', 'game code'],
			['draw_object.h', 'DrawObject', 'game code'],
			['visual.h', 'Visual', 'game code'],
			['border_thickness.h', 'BorderThickness', 'game code'],
			['viewport_manager.h', 'ViewportManager', 'game code'],
			['resolution_manager.h', 'ResolutionManager', 'game code'],
			['screen_resolution.h', 'ScreenResolution', 'game code'],
			['screen_layout.h', 'ScreenLayout', 'game code'],
			['sprite_geometry.h', 'Sprite geometry', 'renderer writer'],
			['sprite_vertex.h', 'SpriteVertex', 'renderer writer'],
			['texture_format.h', 'TextureFormat', 'renderer writer'],
			['texture_data.h', 'TextureData', 'renderer writer'],
			['dds_file.h', 'DDS files', 'renderer writer'],
			['block_decode.h', 'Block decoding', 'renderer writer'],
			['sprite_font_file.h', 'SpriteFontFile', 'renderer writer'],
			['resource_factory.h', 'Resource factory', 'renderer writer'],
			['throw_if_failed.h', 'throw_if_failed', 'renderer writer'],
		],
	},
	{
		name: 'scene',
		summary: 'One object list, one collision sweep, and one per-view render fan-out.',
		headers: [['scene.h', 'Scene', 'game code']],
	},
	{
		name: 'input',
		summary: 'Keyboard, mouse, touch and four gamepads, with press edges, deadzones and menu directions.',
		headers: [
			['keyboard.h', 'Keyboard', 'game code'],
			['gamepads.h', 'Gamepads', 'game code'],
			['gamepad.h', 'GamepadState', 'game code'],
			['mouse.h', 'Mouse', 'game code'],
			['touch.h', 'Touch', 'game code'],
			['direction.h', 'Direction', 'game code'],
			['gamepad_reader.h', 'GamepadReader', 'game code'],
		],
	},
	{
		name: 'audio',
		summary: 'Sound banks and voices, behind the AudioDevice seam.',
		headers: [
			['sound_bank.h', 'SoundBank', 'game code'],
			['audio_resources.h', 'AudioResources', 'game code'],
			['sound_group.h', 'SoundGroup', 'game code'],
			['audio_device.h', 'AudioDevice', 'game code'],
			['xwb_file.h', 'XWB files', 'game code'],
		],
	},
	{
		name: 'ui',
		summary: 'Widgets, focus and directional navigation.',
		headers: [
			['widget.h', 'Widgets', 'game code'],
			['focus.h', 'FocusGroup', 'game code'],
			['button.h', 'Button', 'game code'],
			['navigation.h', 'Navigation', 'game code'],
		],
	},
	{
		name: 'assets',
		summary: 'The content manifest, the loader that walks it, and checked JSON.',
		headers: [
			['asset_manifest.h', 'AssetManifest', 'game code'],
			['resource_loader.h', 'ResourceLoader', 'game code'],
			['json.h', 'JSON', 'game code'],
			['asset_manifest_loader.h', 'Reading a manifest', 'game code'],
			['sprite_sheet_loader.h', 'Reading a sprite sheet', 'game code'],
			['sound_bank_loader.h', 'Reading a sound bank', 'game code'],
		],
	},
	{
		name: 'app',
		summary: 'The application shell: the window, the frame loop, and the services a game is handed.',
		headers: [
			['application.h', 'Application', 'game code'],
			['content_root.h', 'Content paths', 'game code'],
			['window.h', 'Window', 'game code'],
		],
	},
];

// Public headers left out of the reference on purpose, each with its reason.
export const UNPUBLISHED_HEADERS: [source: string, reason: string][] = [
	[
		'engine/core/step_timer.h',
		"Microsoft's timer from the DirectX templates, carried as written (NOTICE) and private to Application.",
	],
	[
		'engine/input/vibration_output.h',
		'The lifetime rules both input backends share behind Gamepads::vibrate, private to them; the reference documents what a game calls, in gamepads.h and gamepad.h.',
	],
	[
		'engine/app/frame_timer.h',
		'The arithmetic behind Application::frame_timings(), private to Application; the reference documents what it publishes, in application.h.',
	],
];

export const REFERENCE_HEADERS: ReferenceHeader[] = REFERENCE_MODULES.flatMap((module) =>
	module.headers.map(([file, title, audience], index) => ({
		module: module.name,
		source: `engine/${module.name}/${file}`,
		slug: `docs/reference/${module.name}/${file.replace(/\.h$/, '').replace(/_/g, '-')}`,
		title,
		order: index + 1,
		audience,
	}))
);

export function validateReferenceAudiences(headers: readonly ReferenceHeader[] = REFERENCE_HEADERS): void {
	for (const header of headers) {
		if (!REFERENCE_AUDIENCES.includes(header.audience)) {
			throw new Error(
				`Reference header '${header.source}' needs an audience: game code or renderer writer ` +
					`(website/src/lib/published.ts).`
			);
		}
	}
}

export function moduleSlug(module: ReferenceModule): string {
	return `docs/reference/${module.name}`;
}

// Every repository file that has a page of its own, keyed by repository path.
export function publishedPages(): Map<string, string> {
	const pages = new Map<string, string>();
	for (const document of DESIGN_DOCUMENTS) {
		pages.set(document.source, `/${document.slug}/`);
	}
	for (const header of REFERENCE_HEADERS) {
		pages.set(header.source, `/${header.slug}/`);
	}
	return pages;
}
