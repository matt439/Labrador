import { defineConfig } from 'astro/config';

// A separate entry point: the regular website build never includes this study.
export default defineConfig({
	srcDir: './explorations/labradors/src',
	publicDir: './explorations/labradors/public',
	cacheDir: './explorations/labradors/.astro',
	outDir: '../out/labrador-dog-concepts',
	server: { host: '127.0.0.1', port: 4326 },
});
