// Build candidate documentation from the release's own verified checkout.
// This command never creates a tag, updates a published index or deploys.
import { spawnSync } from 'node:child_process';
import { writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { verifySnapshotSource } from './snapshot-source.mjs';
import { verifySnapshotOutput } from './snapshot-output.mjs';

export async function buildSnapshot(root, tag) {
	const source = verifySnapshotSource(root, tag);
	const website = path.join(root, 'website');
	const environment = { ...process.env, LABRADOR_DOCS_TAG: tag };
	// Git hooks and alternate-index shells must not route the build's Git reads
	// somewhere other than the checkout that passed verification.
	for (const key of Object.keys(environment)) {
		if (/^GIT_/i.test(key)) delete environment[key];
	}
	environment.GIT_NO_REPLACE_OBJECTS = '1';
	environment.GIT_OPTIONAL_LOCKS = '0';
	const result = spawnSync(process.execPath, [path.join(website, 'node_modules/astro/bin/astro.mjs'), 'build'], {
		cwd: website,
		env: environment,
		stdio: 'inherit',
	});
	if (result.error) throw result.error;
	if (result.status !== 0) throw new Error(`Snapshot website build failed (${result.signal ?? result.status}).`);
	const directory = path.join(website, 'dist/snapshot');
	const verification = await verifySnapshotOutput(directory, source);
	// Source cannot change between the initial gate and the completed checks.
	const after = verifySnapshotSource(root, tag);
	if (JSON.stringify(after) !== JSON.stringify(source)) {
		throw new Error('Release source changed while building its documentation.');
	}
	const record = { ...source, verification };
	writeFileSync(path.join(directory, 'snapshot-build.json'), `${JSON.stringify(record, null, 2)}\n`, { flag: 'wx' });
	return record;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
	try {
		if (process.argv.length !== 3) throw new Error('Usage: npm run snapshot:build -- vMAJOR.MINOR.PATCH');
		const root = fileURLToPath(new URL('../../', import.meta.url));
		const record = await buildSnapshot(root, process.argv[2]);
		process.stdout.write(`Verified candidate ${record.tag} at ${record.commit}: website/dist/snapshot/\n`);
	} catch (error) {
		process.stderr.write(`${error instanceof Error ? error.message : String(error)}\n`);
		process.exitCode = 1;
	}
}
