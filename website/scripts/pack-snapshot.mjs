// Pack a verified candidate snapshot as the asset attached to its GitHub
// release, and print the manifest entry that would publish it. This command
// never uploads, tags, edits the manifest or deploys.
import { createHash } from 'node:crypto';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { isDeepStrictEqual } from 'node:util';
import { canonicalize } from './published-versions.mjs';
import { packTree, readArchive } from './snapshot-archive.mjs';
import { verifySnapshotOutput } from './snapshot-output.mjs';
import { verifySnapshotSource } from './snapshot-source.mjs';

function readBuildRecord(directory) {
	const filename = path.join(directory, 'snapshot-build.json');
	let text;
	try {
		text = readFileSync(filename, 'utf8');
	} catch (error) {
		throw new Error(`${filename}: expected the record written by npm run snapshot:build, found ${error.code ?? error.message}.`);
	}
	try {
		return JSON.parse(text);
	} catch (error) {
		throw new Error(`${filename}: expected JSON, found a syntax error: ${error.message}`);
	}
}

/**
 * Verify the checkout and its built candidate again, then write the archive to
 * website/release-assets/ and return its manifest entry and canonical text.
 */
export async function packSnapshot(root, tag, { now = () => new Date() } = {}) {
	const source = verifySnapshotSource(root, tag);
	const website = path.join(root, 'website');
	const directory = path.join(website, 'dist/snapshot');
	const record = readBuildRecord(directory);
	const recorded = Object.fromEntries(Object.entries(record ?? {}).filter(([key]) => key !== 'verification'));
	if (!isDeepStrictEqual(recorded, source)) {
		throw new Error(`snapshot-build.json: expected the source record ${JSON.stringify(source)}, found ${JSON.stringify(recorded)}; rebuild with npm run snapshot:build.`);
	}
	if (record.verification === null || typeof record.verification !== 'object') {
		throw new Error(`snapshot-build.json: expected a verification result, found ${JSON.stringify(record.verification)}; rebuild with npm run snapshot:build.`);
	}
	// The output must still be the tree that was verified when it was built.
	const verification = await verifySnapshotOutput(directory, source);
	if (!isDeepStrictEqual(verification, record.verification)) {
		throw new Error(`snapshot-build.json: expected verification ${JSON.stringify(record.verification)} to repeat, found ${JSON.stringify(verification)}; the output changed after it was built.`);
	}
	const archive = packTree(directory);
	const entry = {
		tag: source.tag,
		commit: source.commit,
		sourceDate: source.sourceDate,
		path: source.snapshotPath,
		asset: `labrador-docs-${source.tag}.tar.gz`,
		bytes: archive.length,
		sha256: createHash('sha256').update(archive).digest('hex'),
		published: now().toISOString().slice(0, 10),
	};
	// The deployment reads the asset with this reader, so prove it accepts it.
	readArchive(archive, entry);
	const folder = path.join(website, 'release-assets');
	const file = path.join(folder, entry.asset);
	mkdirSync(folder, { recursive: true });
	try {
		writeFileSync(file, archive, { flag: 'wx' });
	} catch (error) {
		throw new Error(`${file}: expected no existing release asset, found ${error.code ?? error.message}; an attached asset is never replaced.`);
	}
	return { entry, file, text: canonicalize(entry) };
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	try {
		if (process.argv.length !== 3) throw new Error('Usage: npm run snapshot:pack -- vMAJOR.MINOR.PATCH');
		const root = fileURLToPath(new URL('../../', import.meta.url));
		const { file, text } = await packSnapshot(root, process.argv[2]);
		process.stderr.write(`Wrote ${path.relative(process.cwd(), file)}; append this entry to published-versions.json after publishing the release:\n`);
		process.stdout.write(text);
	} catch (error) {
		process.stderr.write(`${error instanceof Error ? error.message : String(error)}\n`);
		process.exitCode = 1;
	}
}
