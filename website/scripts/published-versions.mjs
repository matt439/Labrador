// The published-release manifest is the development site's only record of
// which release snapshots exist. Its bytes are canonical so that review sees
// every change, and its history is append-only so a publication is permanent.
import { readFileSync } from 'node:fs';
import path from 'node:path';
import { releaseVersion } from './snapshot-source.mjs';

export const ARCHIVE_MAX_BYTES = 256 * 1024 * 1024;

const MANIFEST_FILE = 'published-versions.json';
const REPOSITORY_URL = 'https://github.com/matt439/Labrador/';
const MANIFEST_KEYS = ['schemaVersion', 'releases', 'errata'];
const RELEASE_KEYS = ['tag', 'commit', 'sourceDate', 'path', 'asset', 'bytes', 'sha256', 'published'];
const ERRATUM_KEYS = ['tag', 'date', 'title', 'href'];
// One ranking serves the manifest, a release entry and an erratum, so an entry
// printed on its own has the same key order it has inside the manifest.
const KEY_ORDER = [...new Set([...MANIFEST_KEYS, ...RELEASE_KEYS, ...ERRATUM_KEYS])];

function keyRank(key) {
	const rank = KEY_ORDER.indexOf(key);
	return rank === -1 ? KEY_ORDER.length : rank;
}

function ordered(value) {
	if (Array.isArray(value)) return value.map(ordered);
	if (value === null || typeof value !== 'object') return value;
	// Unknown keys keep their relative order after the known ones; validation
	// refuses them, but canonical text must still show them to the reviewer.
	const keys = Object.keys(value).sort((a, b) => keyRank(a) - keyRank(b));
	return Object.fromEntries(keys.map((key) => [key, ordered(value[key])]));
}

/** Canonical manifest text: fixed key order, 2-space indent, LF, one trailing newline. */
export function canonicalize(value) {
	return `${JSON.stringify(ordered(value), null, 2)}\n`;
}

function stableTag(tag) {
	const match = typeof tag === 'string' && /^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$/.exec(tag);
	if (!match) throw new Error(`Expected a stable release tag vMAJOR.MINOR.PATCH, found ${JSON.stringify(tag)}.`);
	return match.slice(1).map(BigInt);
}

/** Semantic-version order of two stable release tags: negative, zero or positive. */
export function compareTags(a, b) {
	const left = stableTag(a);
	const right = stableTag(b);
	for (let index = 0; index < 3; index += 1) {
		if (left[index] !== right[index]) return left[index] < right[index] ? -1 : 1;
	}
	return 0;
}

/** The download is derived from the tag and asset name, never stored, so an edit cannot redirect it. */
export function assetUrl(entry) {
	return `${REPOSITORY_URL}releases/download/${entry.tag}/${entry.asset}`;
}

function describe(value) {
	return value === undefined ? 'nothing' : JSON.stringify(value);
}

function isRecord(value) {
	return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function requireKeys(location, value, keys) {
	if (!isRecord(value)) throw new Error(`${location}: expected an object, found ${describe(value)}.`);
	const found = Object.keys(value);
	const extra = found.filter((key) => !keys.includes(key));
	if (extra.length) throw new Error(`${location}: expected only the keys ${keys.join(', ')}, found ${extra.join(', ')}.`);
	const missing = keys.filter((key) => !found.includes(key));
	if (missing.length) throw new Error(`${location}: expected the keys ${keys.join(', ')}, missing ${missing.join(', ')}.`);
}

function requireDate(location, value) {
	const match = typeof value === 'string' && /^([0-9]{4})-([0-9]{2})-([0-9]{2})$/.exec(value);
	const date = match && new Date(Date.UTC(Number(match[1]), Number(match[2]) - 1, Number(match[3])));
	if (!date || date.toISOString().slice(0, 10) !== value) {
		throw new Error(`${location}: expected a calendar date YYYY-MM-DD, found ${describe(value)}.`);
	}
}

function requireTag(location, value) {
	try {
		if (typeof value !== 'string') throw new Error('Use a stable release tag of the form vMAJOR.MINOR.PATCH.');
		releaseVersion(value);
	} catch (error) {
		throw new Error(`${location}: ${error.message} Found ${describe(value)}.`);
	}
}

function validateRelease(location, entry) {
	requireKeys(location, entry, RELEASE_KEYS);
	requireTag(`${location}.tag`, entry.tag);
	if (typeof entry.commit !== 'string' || !/^(?:[0-9a-f]{40}|[0-9a-f]{64})$/.test(entry.commit)) {
		throw new Error(`${location}.commit: expected a full lowercase commit of 40 or 64 hex digits, found ${describe(entry.commit)}.`);
	}
	requireDate(`${location}.sourceDate`, entry.sourceDate);
	if (entry.path !== `/versions/${entry.tag}/`) {
		throw new Error(`${location}.path: expected ${describe(`/versions/${entry.tag}/`)}, found ${describe(entry.path)}.`);
	}
	if (entry.asset !== `labrador-docs-${entry.tag}.tar.gz`) {
		throw new Error(`${location}.asset: expected ${describe(`labrador-docs-${entry.tag}.tar.gz`)}, found ${describe(entry.asset)}.`);
	}
	if (!Number.isSafeInteger(entry.bytes) || entry.bytes <= 0 || entry.bytes > ARCHIVE_MAX_BYTES) {
		throw new Error(`${location}.bytes: expected an integer from 1 to ${ARCHIVE_MAX_BYTES}, found ${describe(entry.bytes)}.`);
	}
	if (typeof entry.sha256 !== 'string' || !/^[0-9a-f]{64}$/.test(entry.sha256)) {
		throw new Error(`${location}.sha256: expected 64 lowercase hex digits, found ${describe(entry.sha256)}.`);
	}
	requireDate(`${location}.published`, entry.published);
}

function siteRelative(href) {
	if (!/^\/[A-Za-z0-9._~\/-]*(?:#[A-Za-z0-9._-]+)?$/.test(href) || href.startsWith('//')) return false;
	const segments = href.split('#', 1)[0].split('/').slice(1);
	if (segments.some((segment, index) => segment === '.' || segment === '..' || (segment === '' && index < segments.length - 1))) {
		return false;
	}
	// Errata belong to the development site; a snapshot path is frozen content.
	return segments[0] !== 'versions';
}

function repositoryLink(href) {
	if (!href.startsWith(REPOSITORY_URL)) return false;
	let url;
	try { url = new URL(href); } catch { return false; }
	return url.href === href && url.origin === 'https://github.com' && !url.username && !url.password
		&& url.pathname.startsWith('/matt439/Labrador/');
}

function validateErratum(location, erratum, tags) {
	requireKeys(location, erratum, ERRATUM_KEYS);
	requireTag(`${location}.tag`, erratum.tag);
	if (!tags.has(erratum.tag)) {
		throw new Error(`${location}.tag: expected a listed release, found ${describe(erratum.tag)}.`);
	}
	requireDate(`${location}.date`, erratum.date);
	if (typeof erratum.title !== 'string' || !erratum.title.trim() || /[\u0000-\u001f\u007f]/.test(erratum.title)) {
		throw new Error(`${location}.title: expected a non-empty single-line title, found ${describe(erratum.title)}.`);
	}
	if (typeof erratum.href !== 'string' || !(siteRelative(erratum.href) || repositoryLink(erratum.href))) {
		throw new Error(`${location}.href: expected a site path outside /versions/ or a ${REPOSITORY_URL} URL, found ${describe(erratum.href)}.`);
	}
}

function freeze(value) {
	if (value !== null && typeof value === 'object') {
		for (const child of Object.values(value)) freeze(child);
		Object.freeze(value);
	}
	return value;
}

/** Parse and validate manifest text; the result is a frozen copy in canonical key order. */
export function validateManifest(text) {
	if (typeof text !== 'string') throw new Error(`Manifest: expected text, found ${typeof text}.`);
	if (text.startsWith('﻿')) throw new Error('Manifest: expected UTF-8 without a byte-order mark, found one.');
	if (text.includes('\r')) throw new Error('Manifest: expected LF line endings, found a carriage return.');
	let value;
	try {
		value = JSON.parse(text);
	} catch (error) {
		throw new Error(`Manifest: expected JSON, found a syntax error: ${error.message}`);
	}
	requireKeys('Manifest', value, MANIFEST_KEYS);
	if (value.schemaVersion !== 1) throw new Error(`Manifest.schemaVersion: expected 1, found ${describe(value.schemaVersion)}.`);
	for (const key of ['releases', 'errata']) {
		if (!Array.isArray(value[key])) throw new Error(`Manifest.${key}: expected an array, found ${describe(value[key])}.`);
	}
	const tags = new Map();
	value.releases.forEach((entry, index) => {
		const location = `releases[${index}]`;
		validateRelease(location, entry);
		if (tags.has(entry.tag)) {
			throw new Error(`${location}.tag: expected a tag not already listed, found ${entry.tag} again after releases[${tags.get(entry.tag)}].`);
		}
		tags.set(entry.tag, index);
	});
	value.errata.forEach((erratum, index) => validateErratum(`errata[${index}]`, erratum, tags));
	// Key order, whitespace and duplicate keys (which JSON.parse silently merges)
	// all show up as a difference from the canonical text.
	const canonical = canonicalize(value);
	if (text !== canonical) {
		const lines = text.split('\n');
		const expected = canonical.split('\n');
		const line = expected.findIndex((content, index) => lines[index] !== content);
		throw new Error(`Manifest: expected canonical text, found a difference at line ${line + 1}: expected ${describe(expected[line])}, found ${describe(lines[line])}.`);
	}
	return freeze(ordered(value));
}

/** Read and validate website/published-versions.json from the website folder. */
export function readManifest(website) {
	const filename = path.join(website, MANIFEST_FILE);
	let text;
	try {
		text = new TextDecoder('utf-8', { fatal: true, ignoreBOM: true }).decode(readFileSync(filename));
	} catch (error) {
		throw new Error(`${filename}: expected a readable UTF-8 manifest, found ${error.code ?? error.message}.`);
	}
	return validateManifest(text);
}

function checkList(kind, previous, next) {
	if (next.length < previous.length) {
		throw new Error(`${kind}: expected at least the ${previous.length} published item(s), found ${next.length}; items are never removed.`);
	}
	previous.forEach((item, index) => {
		const before = JSON.stringify(ordered(item));
		const after = JSON.stringify(ordered(next[index]));
		if (before !== after) {
			throw new Error(`${kind}[${index}]: expected the published item unchanged ${before}, found ${after}; only appending is allowed.`);
		}
	});
}

/**
 * Refuse any change except appending. Publication order is append order, so a
 * backport may follow a newer tag; display order is sorted separately.
 */
export function checkAppendOnly(previous, next) {
	if (previous.schemaVersion !== next.schemaVersion) {
		throw new Error(`schemaVersion: expected ${describe(previous.schemaVersion)}, found ${describe(next.schemaVersion)}.`);
	}
	// A tag returning at a new index gets its own message before the index
	// comparison would report it as an edit.
	const published = new Map(previous.releases.map((entry, index) => [entry.tag, index]));
	next.releases.slice(previous.releases.length).forEach((entry, offset) => {
		if (published.has(entry.tag)) {
			throw new Error(`releases[${previous.releases.length + offset}].tag: expected a tag never published before, found ${entry.tag}, published at releases[${published.get(entry.tag)}].`);
		}
	});
	checkList('releases', previous.releases, next.releases);
	checkList('errata', previous.errata, next.errata);
}
