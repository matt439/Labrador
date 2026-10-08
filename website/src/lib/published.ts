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

export interface ReferenceHeader {
	module: string;
	// The header, from the repository root.
	source: string;
	slug: string;
	// The sidebar label: the primary type the header declares, or what it is
	// for when it declares no type.
	title: string;
	order: number;
}

export interface ReferenceModule {
	// The folder under engine/, which is also the module's name.
	name: string;
	// What the module is, for the reference's introduction and module pages.
	summary: string;
	// The headers, by file name, in the order a reader meets them, each with
	// its sidebar label.
	headers: [file: string, title: string][];
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
			['vector2f.h', 'Vector2F'],
			['vector2i.h', 'Vector2I'],
			['rectanglef.h', 'RectangleF'],
			['rectanglei.h', 'RectangleI'],
			['matrix3x2f.h', 'Matrix3x2F'],
			['scalar.h', 'Scalars and tolerance'],
			['shape.h', 'Shape'],
			['shape_type.h', 'ShapeType'],
			['circle.h', 'Circle'],
			['triangle.h', 'Triangle'],
			['quad.h', 'Quad'],
			['rectangle_rotated.h', 'RectangleRotated'],
			['segment.h', 'Segment'],
			['intersects.h', 'Intersection tests'],
			['inflate.h', 'Inflating shapes'],
			['ericson_math.h', 'Ericson routines'],
		],
	},
	{
		name: 'core',
		summary: 'Game objects, the state stack, handles and the tables that issue them, and the thread pool.',
		headers: [
			['state.h', 'State'],
			['state_context.h', 'StateContext'],
			['game_object.h', 'GameObject'],
			['handle.h', 'Handle'],
			['registry.h', 'Registry'],
			['name_table.h', 'NameTable'],
			['thread_pool.h', 'ThreadPool'],
			['byte_reader.h', 'ByteReader'],
			['file_path.h', 'UTF-8 paths'],
			['moving_object.h', 'MovingObject'],
		],
	},
	{
		name: 'collision',
		summary: 'Layers and masks, a broad phase, a narrow phase that measures contacts, and analytic resolution.',
		headers: [
			['collision_object.h', 'CollisionObject'],
			['collision_layer.h', 'Layers and masks'],
			['contacts.h', 'Contact'],
			['manifold.h', 'Manifold'],
			['narrow_phase.h', 'Narrow phase'],
			['resolve.h', 'Resolution'],
			['broad_phase.h', 'BroadPhase'],
			['tunnelling.h', 'Tunnelling'],
			['partitioner.h', 'Partitioner'],
		],
	},
	{
		name: 'render',
		summary: 'The renderer, draw lists, cameras and viewports, sprites, text, colours, and the files content arrives in.',
		headers: [
			['renderer.h', 'Renderer'],
			['render_resources.h', 'RenderResources'],
			['camera.h', 'Camera'],
			['viewport.h', 'Viewport'],
			['colour.h', 'Colour'],
			['label.h', 'Label'],
			['text.h', 'Text'],
			['text_object.h', 'TextObject'],
			['text_drop_shadow.h', 'TextDropShadow'],
			['font.h', 'Font'],
			['text_encoding.h', 'Text encoding'],
			['texture_object.h', 'TextureObject'],
			['animation_object.h', 'AnimationObject'],
			['sprite_sheet.h', 'SpriteSheet'],
			['sprite_sheet_object.h', 'SpriteSheetObject'],
			['sprite_frame.h', 'SpriteFrame'],
			['animation_strip.h', 'AnimationStrip'],
			['draw_object.h', 'DrawObject'],
			['visual.h', 'Visual'],
			['rotation_origin.h', 'RotationOrigin'],
			['camera_tools.h', 'CameraTools'],
			['border_thickness.h', 'BorderThickness'],
			['viewport_manager.h', 'ViewportManager'],
			['resolution_manager.h', 'ResolutionManager'],
			['screen_resolution.h', 'ScreenResolution'],
			['screen_layout.h', 'ScreenLayout'],
			['sprite_geometry.h', 'Sprite geometry'],
			['sprite_vertex.h', 'SpriteVertex'],
			['texture_format.h', 'TextureFormat'],
			['texture_data.h', 'TextureData'],
			['dds_file.h', 'DDS files'],
			['sprite_font_file.h', 'SpriteFontFile'],
			['resource_factory.h', 'Resource factory'],
			['throw_if_failed.h', 'throw_if_failed'],
		],
	},
	{
		name: 'scene',
		summary: 'One object list, one collision sweep, and one per-view render fan-out.',
		headers: [['scene.h', 'Scene']],
	},
	{
		name: 'input',
		summary: 'Keyboard, mouse and four gamepads, with press edges, deadzones and menu directions.',
		headers: [
			['keyboard.h', 'Keyboard'],
			['gamepads.h', 'Gamepads'],
			['gamepad.h', 'GamepadState'],
			['mouse.h', 'Mouse'],
			['direction.h', 'Direction'],
			['gamepad_reader.h', 'GamepadReader'],
		],
	},
	{
		name: 'audio',
		summary: 'Sound banks and voices, behind the AudioDevice seam.',
		headers: [
			['sound_bank.h', 'SoundBank'],
			['audio_resources.h', 'AudioResources'],
			['sound_bank_object.h', 'SoundBankObject'],
			['audio_device.h', 'AudioDevice'],
		],
	},
	{
		name: 'ui',
		summary: 'Widgets, focus and directional navigation.',
		headers: [
			['widget.h', 'Widgets'],
			['focus.h', 'FocusGroup'],
			['button.h', 'Button'],
			['navigation.h', 'Navigation'],
		],
	},
	{
		name: 'assets',
		summary: 'The content manifest, the loader that walks it, and checked JSON.',
		headers: [
			['asset_manifest.h', 'AssetManifest'],
			['resource_loader.h', 'ResourceLoader'],
			['json.h', 'JSON'],
			['asset_manifest_loader.h', 'Reading a manifest'],
			['sprite_sheet_loader.h', 'Reading a sprite sheet'],
			['sound_bank_loader.h', 'Reading a sound bank'],
		],
	},
	{
		name: 'app',
		summary: 'The application shell: the window, the frame loop, and the services a game is handed.',
		headers: [
			['application.h', 'Application'],
			['content_root.h', 'Content paths'],
			['window.h', 'Window'],
		],
	},
];

// Public headers left out of the reference on purpose, each with its reason.
export const UNPUBLISHED_HEADERS: [source: string, reason: string][] = [
	[
		'engine/core/step_timer.h',
		"Microsoft's timer from the DirectX templates, carried as written (NOTICE) and private to Application.",
	],
];

export const REFERENCE_HEADERS: ReferenceHeader[] = REFERENCE_MODULES.flatMap((module) =>
	module.headers.map(([file, title], index) => ({
		module: module.name,
		source: `engine/${module.name}/${file}`,
		slug: `docs/reference/${module.name}/${file.replace(/\.h$/, '').replace(/_/g, '-')}`,
		title,
		order: index + 1,
	}))
);

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
