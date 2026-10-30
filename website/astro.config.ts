// The Labrador website: a showcase, a getting-started path and the
// documentation, built as static files from this checkout.
//
// Nothing in the engine's build depends on this directory, and nothing here
// is built by CMake. The site reads the rest of the repository - design
// documents, sample source, engine headers - at build time, so it always
// describes the revision it was built from (src/lib/repository.ts).

import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';
import { satteri } from '@astrojs/markdown-satteri';
import starlightLinksValidator from 'starlight-links-validator';
import { checkoutLinks, mermaidBlocks, referenceLinks } from './src/lib/markdown-plugins';
import { GITHUB_REPOSITORY, EDIT_BRANCH, RELEASE_SOURCE } from './src/lib/repository';
import { SITE_BASE } from './src/lib/site-path';
import { REFERENCE_MODULES, moduleSlug } from './src/lib/published';

export default defineConfig({
	// The one host the site is served from. Canonical links and the sitemap
	// are built from it, and wrangler.jsonc serves no workers.dev copy.
	site: 'https://labradorengine.com',
	base: SITE_BASE,
	// Candidate snapshots have their own output folder; publishing and retaining
	// permanent versions is a separate operation.
	outDir: RELEASE_SOURCE ? './dist/snapshot/' : './dist/',
	markdown: {
		processor: satteri({ mdastPlugins: [checkoutLinks, mermaidBlocks, referenceLinks] }),
	},
	integrations: [
		starlight({
			title: 'Labrador',
			description:
				'A 2D game engine for developers who want to write games in ordinary C++ and understand the machinery beneath them.',
			favicon: '/favicon.svg',
			logo: { src: './src/assets/mark.svg', alt: '', replacesTitle: false },
			social: [{ icon: 'github', label: 'Labrador on GitHub', href: GITHUB_REPOSITORY }],
			editLink: { baseUrl: RELEASE_SOURCE
				? `${GITHUB_REPOSITORY}/blob/${RELEASE_SOURCE.commit}/website/`
				: `${GITHUB_REPOSITORY}/edit/${EDIT_BRANCH}/website/` },
			customCss: ['./src/styles/site.css'],
			routeMiddleware: './src/route-data.ts',
			components: {
				Header: './src/components/overrides/Header.astro',
				Footer: './src/components/overrides/Footer.astro',
				EditLink: './src/components/overrides/EditLink.astro',
				MarkdownContent: './src/components/overrides/MarkdownContent.astro',
				Sidebar: './src/components/overrides/Sidebar.astro',
				Banner: './src/components/overrides/Banner.astro',
				Hero: './src/components/overrides/Hero.astro',
			},
			// Every group starts collapsed; Starlight opens the one holding the
			// current page, so the reader sees where they are without scrolling.
			sidebar: [
				{ label: 'Overview', link: '/docs/' },
				{
					label: 'Get Started',
					collapsed: true,
					items: [
						{ label: 'Introduction', link: '/docs/get-started/' },
						{ label: 'Install the tools', link: '/docs/get-started/prerequisites/' },
						{ label: 'Get the starter project', link: '/docs/get-started/starter/' },
						{ label: 'Make a first change', link: '/docs/get-started/first-change/' },
						{ label: 'Read a build error', link: '/docs/get-started/build-errors/' },
						{ label: 'Edit and debug in Visual Studio', link: '/docs/get-started/debug-your-game/' },
						// The route for readers who use Git, which makes the same project.
						{
							label: 'With Git instead',
							collapsed: true,
							items: [
								{ label: 'Build Labrador', link: '/docs/get-started/build-labrador/' },
								{ label: 'Run the minimal sample', link: '/docs/get-started/run-minimal/' },
								{ label: 'Start your own project', link: '/docs/get-started/your-project/' },
							],
						},
					],
				},
				{ label: 'Concepts', collapsed: true, items: [{ autogenerate: { directory: 'docs/concepts' } }] },
				{ label: 'Guides', collapsed: true, items: [{ autogenerate: { directory: 'docs/guides' } }] },
				{
					label: 'API Reference',
					collapsed: true,
					items: [
						{ label: 'About the reference', link: '/docs/reference/' },
						// One collapsed group per module, in the order published.ts lists them.
						...REFERENCE_MODULES.map((module) => ({
							label: module.name,
							collapsed: true,
							items: [
								{ label: 'Overview', link: `/${moduleSlug(module)}/` },
								{ autogenerate: { directory: `docs/reference/${module.name}` } },
							],
						})),
					],
				},
				{ label: 'Design', collapsed: true, items: [{ autogenerate: { directory: 'docs/design' } }] },
				{ label: 'Troubleshooting', link: '/docs/troubleshooting/' },
			],
			plugins: [starlightLinksValidator()],
		}),
	],
});
