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
import { checkoutLinks, mermaidBlocks } from './src/lib/markdown-plugins';
import { GITHUB_REPOSITORY, EDIT_BRANCH } from './src/lib/repository';
import { REFERENCE_MODULES } from './src/lib/published';

export default defineConfig({
	markdown: {
		processor: satteri({ mdastPlugins: [checkoutLinks, mermaidBlocks] }),
	},
	integrations: [
		starlight({
			title: 'Labrador',
			description:
				'A 2D game engine for developers who want to write games in ordinary C++ and understand the machinery beneath them.',
			favicon: '/favicon.svg',
			social: [{ icon: 'github', label: 'Labrador on GitHub', href: GITHUB_REPOSITORY }],
			editLink: { baseUrl: `${GITHUB_REPOSITORY}/edit/${EDIT_BRANCH}/website/` },
			customCss: ['./src/styles/site.css'],
			components: {
				Header: './src/components/overrides/Header.astro',
				Footer: './src/components/overrides/Footer.astro',
				MarkdownContent: './src/components/overrides/MarkdownContent.astro',
				MobileMenuFooter: './src/components/overrides/MobileMenuFooter.astro',
			},
			sidebar: [
				{ label: 'Overview', link: '/docs/' },
				{
					label: 'Get Started',
					items: [
						{ label: 'Introduction', link: '/docs/get-started/' },
						{ label: 'Install the tools', link: '/docs/get-started/prerequisites/' },
						{ label: 'Build Labrador', link: '/docs/get-started/build-labrador/' },
						{ label: 'Run the minimal sample', link: '/docs/get-started/run-minimal/' },
						{ label: 'Start your own project', link: '/docs/get-started/your-project/' },
						{ label: 'Make a first change', link: '/docs/get-started/first-change/' },
					],
				},
				{ label: 'Concepts', items: [{ autogenerate: { directory: 'docs/concepts' } }] },
				{ label: 'Guides', items: [{ autogenerate: { directory: 'docs/guides' } }] },
				{
					label: 'API Reference',
					items: [
						{ label: 'About the reference', link: '/docs/reference/' },
						// One collapsed group per module, in the order published.ts lists them.
						...REFERENCE_MODULES.map((module) => ({
							label: module.name,
							collapsed: true,
							items: [{ autogenerate: { directory: `docs/reference/${module.name}` } }],
						})),
					],
				},
				{ label: 'Design', items: [{ autogenerate: { directory: 'docs/design' } }] },
				{ label: 'Troubleshooting', link: '/docs/troubleshooting/' },
			],
			plugins: [starlightLinksValidator()],
		}),
	],
});
