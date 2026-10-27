// Frontmatter links are outside the Markdown pipeline. Route them after the
// link validator's pre-middleware so it validates the URLs readers receive.
import { defineRouteMiddleware } from '@astrojs/starlight/route-data';
import { sitePath } from './lib/site-path.ts';

export const onRequest = defineRouteMiddleware(async (context, next) => {
	const route = context.locals.starlightRoute;
	const { data } = route.entry;
	for (const action of data.hero?.actions ?? []) action.link = sitePath(action.link);
	for (const direction of ['prev', 'next'] as const) {
		const link = data[direction];
		if (typeof link === 'object' && typeof link.link === 'string') link.link = sitePath(link.link);
		// Starlight derives these links before route middleware runs. Keep its
		// rendered pagination consistent with the frontmatter being validated.
		const rendered = route.pagination[direction];
		if (rendered) rendered.href = sitePath(rendered.href);
	}
	await next();
});
