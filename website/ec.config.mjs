import { defineEcConfig } from '@astrojs/starlight/expressive-code';
import { apiCodeLinks } from './src/lib/code-links.ts';

// Shared by Markdown fences and the Code component used by SourceFile.
export default defineEcConfig({ plugins: [apiCodeLinks] });
