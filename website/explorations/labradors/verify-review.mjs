import assert from 'node:assert/strict';
import { readFile, access } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';
import { parse } from 'parse5';
import { Document, Element, Text } from 'domhandler';
import { selectAll, selectOne } from 'css-select';

// Browserless smoke checks of the emitted HTML and its actual built script.
// The small DOM adapter supplies selectors, state and change events only.
// It cannot measure layout, keyboard behavior or the accessibility tree.
const website = fileURLToPath(new URL('../../', import.meta.url));
const output = path.resolve(website, '../out/labrador-dog-concepts/offline');

function dom(source) {
	function convert(node) {
		if (node.nodeName === '#text') return new Text(node.value);
		const children = (node.childNodes ?? []).map(convert).filter(Boolean);
		const element = node.tagName
			? new Element(node.tagName, Object.fromEntries(node.attrs.map(({ name, value }) => [name, value])), children)
			: new Document(children);
		children.forEach((child, index) => {
			child.parent = element;
			child.prev = children[index - 1] ?? null;
			child.next = children[index + 1] ?? null;
		});
		element.querySelectorAll = (selector) => selectAll(selector, element.children);
		element.querySelector = (selector) => selectOne(selector, element.children);
		element.hidden = Object.hasOwn(element.attribs ?? {}, 'hidden');
		element.open = Object.hasOwn(element.attribs ?? {}, 'open');
		element.dataset = Object.fromEntries(Object.entries(element.attribs ?? {})
			.filter(([key]) => key.startsWith('data-')).map(([key, value]) => [key.slice(5), value]));
		element.value = element.attribs?.value ?? element.querySelector('option')?.attribs.value ?? '';
		const listeners = new Map();
		element.addEventListener = (type, listener) => {
			if (!listeners.has(type)) listeners.set(type, []);
			listeners.get(type).push(listener);
		};
		element.change = (value) => {
			element.value = value;
			for (const listener of listeners.get('change') ?? []) listener();
		};
		return element;
	}
	const document = convert(parse(source));
	document.documentElement = document.querySelector('html');
	return document;
}

function textContent(node) {
	return node.type === 'text' ? node.data : (node.children ?? []).map(textContent).join('');
}
function visible(node) {
	return node && !node.hidden && (!node.parent || visible(node.parent));
}

let states = 0;
for (const filename of ['index.html', 'duo.html', 'guides.html']) {
	const html = await readFile(path.join(output, filename), 'utf8');
	const document = dom(html);
	const all = (selector) => document.querySelectorAll(selector);
	const one = (selector) => document.querySelector(selector);
	assert.equal(one('html').attribs.lang, 'en');
	assert.equal(all('h1').length, 1);
	assert.equal(all('[aria-current="page"]').length, 1);
	assert.match(one('meta[name="robots"]').attribs.content, /noindex/);
	const ids = all('[id]').map((node) => node.attribs.id);
	assert.equal(ids.length, new Set(ids).size, 'IDs must be unique');
	for (const node of all('[aria-labelledby]')) {
		for (const id of node.attribs['aria-labelledby'].split(/\s+/)) assert.ok(ids.includes(id));
	}
	for (const image of all('img')) {
		assert.ok(Object.hasOwn(image.attribs, 'alt'));
		assert.match(image.attribs.src, /^data:image\//, 'offline images must be embedded');
	}
	for (const link of all('a[href]')) {
		const href = link.attribs.href;
		if (href.startsWith('#')) assert.ok(ids.includes(href.slice(1)));
		else if (!href.startsWith('https:')) await access(path.join(output, href));
	}
	for (const portrait of all('.portrait')) assert.match(textContent(portrait), /Placeholder/);
	for (const script of all('script')) {
		assert.ok(!script.attribs.src, 'offline script must be embedded');
		vm.runInNewContext(textContent(script), { document }, { timeout: 1000 });
	}
	assert.equal(all('[data-dog-aside]').filter(visible).length, 1);
	for (const theme of ['light', 'dark']) {
		one('#theme').change(theme);
		assert.equal(document.documentElement.dataset.theme, theme);
		for (const context of ['home', 'examples']) {
			one(`input[name="context"][value="${context}"]`).change(context);
			assert.deepEqual(all('[data-context]').filter(visible).map((node) => node.dataset.context), [context]);
			for (const presence of ['asides', 'footer', 'off']) {
				const footer = one('[data-dog-footer]');
				footer.open = true;
				one('#presence').change(presence);
				assert.equal(all('[data-dog-aside]').filter(visible).length, presence === 'asides' ? 1 : 0);
				assert.equal(visible(footer), presence !== 'off');
				if (presence === 'off') assert.equal(footer.open, false);
				assert.ok(all('.primary-link, .next-step a').some(visible), 'essential next step remains available');
				states++;
			}
		}
	}
	console.log(`${filename}: HTML and controls passed`);
}
console.log(`${states} theme/context/presence combinations passed in the browserless DOM adapter.`);
