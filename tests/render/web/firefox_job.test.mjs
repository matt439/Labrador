import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { isAbsolute } from 'node:path';
import { createInterface } from 'node:readline';
import { setTimeout as delay } from 'node:timers/promises';
import test from 'node:test';

// Run against the compiled Windows host tool before the hosted Firefox suite.
const helper = process.env.LABRADOR_FIREFOX_JOB_HELPER;
const windows = { skip: process.platform !== 'win32' && !helper && 'requires Windows' };

function start(t, executable, args) {
	const child = spawn(executable, args, { stdio: ['pipe', 'pipe', 'pipe'], windowsHide: true, timeout: 15000 });
	const closed = once(child, 'close');
	let stdout = '';
	let stderr = '';
	child.stdout.setEncoding('utf8').on('data', (text) => { stdout += text; });
	child.stderr.setEncoding('utf8').on('data', (text) => { stderr += text; });
	t.after(async () => {
		if (child.exitCode === null && child.signalCode === null) child.kill();
		await closed.catch(() => {});
	});
	return { child, closed, stdout: () => stdout, stderr: () => stderr };
}

function supervised(t, args) {
	assert.equal(process.platform, 'win32');
	assert.ok(helper && isAbsolute(helper), 'Set LABRADOR_FIREFOX_JOB_HELPER to the compiled helper');
	return start(t, helper, [process.execPath, ...args]);
}

test('the job preserves arguments and stdio and propagates the launcher exit code', windows, async (t) => {
	const args = ['', 'two words', 'a"b', 'C:\\with spaces\\', '\\\\"quoted', 'dépôt', 'two\tcolumns'];
	const echo = supervised(t, ['-e', `
		const { readFileSync } = require('node:fs');
		console.log(JSON.stringify([readFileSync(0, 'utf8'), ...process.argv.slice(1)]));
		console.error('fixture stderr');
	`, '--', ...args]);
	assert.deepEqual(await echo.closed, [0, null], echo.stderr());
	assert.deepEqual(JSON.parse(echo.stdout()), ['', ...args]);
	assert.match(echo.stderr(), /fixture stderr/);

	const failed = supervised(t, ['-e', 'process.exitCode = 37;']);
	assert.deepEqual(await failed.closed, [37, null], failed.stderr());
});

// The leaf writes a line only once its own Node runtime is running. The
// independent backstop lets it exit even if a broken supervisor loses it.
const leaf = `
	console.log(JSON.stringify({ leader: Number(process.argv[1]), leaf: process.pid }));
	setTimeout(() => {}, 30000);
`;
function leader(stay_alive, exit_code) {
	return `
		const { spawn } = require('node:child_process');
		// Detachment lets the leaf outlive this Node launcher's own libuv job.
		// It still belongs to the enclosing Firefox supervisor job.
		const child = spawn(process.execPath, ['-e', ${JSON.stringify(leaf)}, String(process.pid)],
			{ detached: true, stdio: ['ignore', 'inherit', 'inherit'], windowsHide: true });
		child.unref();
		process.exitCode = ${exit_code};
		${stay_alive ? 'setTimeout(() => {}, 30000);' : ''}
	`;
}

function running(pid) {
	try { process.kill(pid, 0); return true; }
	catch (error) { if (error.code === 'ESRCH') return false; throw error; }
}

for (const method of ['byte', 'EOF']) {
	test(`a control ${method} stops the owned tree and leaves an independent process alive`, windows, async (t) => {
		const sibling = start(t, process.execPath, ['-e', 'setTimeout(() => {}, 30000);']);
		// A stop request for surviving descendants must preserve the status of
		// a launcher that has already exited with a failure.
		const exit_code = method === 'byte' ? 0 : 37;
		const job = supervised(t, ['-e', leader(method === 'byte', exit_code)]);
		const lines = createInterface({ input: job.child.stdout });
		try {
			const [line] = await Promise.race([once(lines, 'line'), job.closed.then(([code]) => {
				throw new Error(`Job closed before leaf startup (exit ${code}): ${job.stderr()}`);
			})]);
			const pids = JSON.parse(line);
			assert.ok(Number.isInteger(pids.leader) && Number.isInteger(pids.leaf));
			assert.ok(running(pids.leaf));
			if (method === 'EOF') {
				// The dead launcher can no longer identify its surviving tree by PID.
				const deadline = Date.now() + 5000;
				while (running(pids.leader) && Date.now() < deadline) await delay(20);
				assert.equal(running(pids.leader), false);
				assert.ok(running(pids.leaf));
				assert.equal(job.child.exitCode, null);
				job.child.stdin.end();
			} else {
				assert.ok(running(pids.leader));
				job.child.stdin.write('q');
			}
			assert.deepEqual(await job.closed, [exit_code, null], job.stderr());
			assert.equal(running(pids.leader), false);
			assert.equal(running(pids.leaf), false);
			assert.ok(running(sibling.child.pid));
		} finally { lines.close(); }
	});
}
