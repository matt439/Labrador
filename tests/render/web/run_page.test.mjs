// run_page.mjs's command line, without a browser: importing it runs nothing.
import assert from 'node:assert/strict';
import { join, resolve } from 'node:path';
import test from 'node:test';
import { NETWORK_PROFILES, USAGE, UsageError, heap_line, parse_arguments, unexcused_browser_complaints } from './run_page.mjs';

const page = join('build', 'samples', 'minimal', 'MinimalSample.html');
const sample = ['--browser', 'chrome', '--page', page, '--quit-keys', 'Escape,Enter'];

test('a sample\'s command line gives its keys, media, paths and deadline', () => {
	const parsed = parse_arguments([...sample, '--emulate-media', 'prefers-reduced-motion=reduce',
		'--expect-console', 'motion reduced', '--timeout', '60']);
	assert.deepEqual(parsed.keys, ['Escape', 'Enter']);
	assert.equal(parsed.frame_count, 60);
	assert.deepEqual(parsed.media, { name: 'prefers-reduced-motion', value: 'reduce' });
	assert.equal(parsed['expect-console'], 'motion reduced');
	assert.equal(parsed.page_path, resolve(page));
	assert.equal(parsed.root_path, resolve('build', 'samples', 'minimal'));
	assert.equal(parsed.page_name, 'MinimalSample.html');
	assert.equal(parsed.timeout_ms, 60000);
	const rooted = parse_arguments(['--browser', 'chrome', '--page', page, '--root', 'build']);
	assert.equal(rooted.page_name, 'samples/minimal/MinimalSample.html');
	assert.equal(rooted.timeout_ms, 300000);
});

test('a malformed --emulate-media is refused with the other usage errors, before anything starts', () => {
	for (const media of ['reduce', '=reduce', '']) {
		assert.throws(() => parse_arguments([...sample, '--emulate-media', media]),
			(error) => error instanceof UsageError && /--emulate-media takes <feature>=<value>/.test(error.message));
	}
});

test('an unknown or misspelt option is refused rather than ignored', () => {
	assert.throws(() => parse_arguments([...sample, '--expect-consol', 'motion']),
		(error) => error instanceof UsageError && /unknown option --expect-consol/.test(error.message));
	for (const refused of [['--browser', 'chrome'], [...sample, '--frames', '0'], [...sample, '--collect', 'out'],
		[...sample, 'loose'], ['--browser', 'chrome', '--page', page, '--root', join('elsewhere')]]) {
		assert.throws(() => parse_arguments(refused), UsageError, refused.join(' '));
	}
});

test('--cpu-throttle and --network take a factor and a named profile, and refuse anything else', () => {
	const parsed = parse_arguments([...sample, '--cpu-throttle', '4', '--network', 'slow-4g']);
	assert.equal(parsed.cpu_rate, 4);
	assert.deepEqual(parsed.network_conditions, { offline: false, ...NETWORK_PROFILES['slow-4g'] });
	assert.equal(NETWORK_PROFILES['slow-4g'].latency, 562.5);
	assert.equal(NETWORK_PROFILES['slow-4g'].downloadThroughput, 180000);
	assert.equal(parse_arguments(sample).cpu_rate, undefined);
	assert.equal(parse_arguments(sample).network_conditions, undefined);
	for (const rate of ['0', '0.5', '21', 'fast', '']) {
		assert.throws(() => parse_arguments([...sample, '--cpu-throttle', rate]),
			(error) => error instanceof UsageError && /--cpu-throttle takes/.test(error.message), rate);
	}
	assert.throws(() => parse_arguments([...sample, '--network', '4g']),
		(error) => error instanceof UsageError && /--network takes one of slow-4g/.test(error.message));
});

test('the usage names the options either form takes', () => {
	for (const option of ['--emulate-media', '--expect-console', '--root', '--quit-keys', '--input',
		'--hide-extension', '--sample-input', '--cpu-throttle', '--network']) {
		assert.match(USAGE, new RegExp(option));
	}
	assert.match(USAGE, /either form: .*--emulate-media.*--expect-console.*--root/);
});

test('a context-lost warning is excused only by the page\'s own report of an unasked loss, one for one', () => {
	const lost = 'WebGL: CONTEXT_LOST_WEBGL: loseContext: context lost';
	const other = 'WebGL: INVALID_OPERATION: bindTexture: object does not belong to this context';
	const report = 'unasked context loss: the browser lost the context before the test asked\n';
	assert.deepEqual(unexcused_browser_complaints([lost, other], []), [lost, other]);
	assert.deepEqual(unexcused_browser_complaints([lost, other], ['ok: a step\n', report]), [other]);
	assert.deepEqual(unexcused_browser_complaints([lost, lost], [report]), [lost]);
	assert.deepEqual(unexcused_browser_complaints([other], [report, report]), [other]);
	assert.deepEqual(unexcused_browser_complaints([lost], [`said: ${report}`]), [lost]);
});

test('a heap line names each module heap in bytes and MiB, a collected one and none at all', () => {
	assert.equal(heap_line('after 60 frames', [16777216]),
		'run_page: wasm heap after 60 frames: 16777216 bytes (16.00 MiB)\n');
	assert.equal(heap_line('at quit', [17891328, null]),
		'run_page: wasm heap at quit: 17891328 bytes (17.06 MiB), collected\n');
	assert.equal(heap_line('at quit', []), 'run_page: wasm heap at quit: no module heap was seen\n');
});
