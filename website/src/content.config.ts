import { defineCollection } from 'astro:content';
import { docsSchema } from '@astrojs/starlight/schema';
import { labradorDocsLoader } from './lib/content-loader';

export const collections = {
	docs: defineCollection({ loader: labradorDocsLoader(), schema: docsSchema() }),
};
