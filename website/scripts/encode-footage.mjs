// Offline media preparation, never part of the Node-only site build.
// The capture tool writes one PNG per 1/60-second presentation update.
import { execFileSync } from 'node:child_process';
import { mkdirSync, readdirSync, renameSync, rmSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const website = fileURLToPath(new URL('..', import.meta.url));
const [framesArgument] = process.argv.slice(2);
if (!framesArgument || process.argv.length !== 3) {
	console.error('Usage: node website/scripts/encode-footage.mjs <frames-directory>');
	process.exit(2);
}

const frames = path.resolve(framesArgument);
const names = readdirSync(frames).sort();
if (names.length === 0 || names.some((name, index) => name !== `${String(index).padStart(6, '0')}.png`)) {
	throw new Error('Expected a continuous sequence of PNGs starting at 000000.png, with no other files.');
}

const output = path.join(website, 'src/assets/footage/linesweeper.mp4');
const temporary = path.join(path.dirname(output), 'linesweeper.encoding.mp4');
mkdirSync(path.dirname(output), { recursive: true });
try {
	execFileSync('ffmpeg', [
		'-hide_banner', '-loglevel', 'warning', '-nostdin', '-y',
		'-framerate', '60', '-start_number', '0', '-i', path.join(frames, '%06d.png'),
		'-an', '-c:v', 'libx264', '-preset', 'slow', '-crf', '20',
		'-pix_fmt', 'yuv420p', '-movflags', '+faststart', temporary,
	], { stdio: 'inherit' });
	const probe = JSON.parse(execFileSync('ffprobe', [
		'-v', 'error', '-count_frames', '-show_streams', '-of', 'json', temporary,
	], { encoding: 'utf8' }));
	const [video] = probe.streams;
	if (probe.streams.length !== 1 || video.codec_name !== 'h264' ||
		video.width !== 1280 || video.height !== 720 || video.pix_fmt !== 'yuv420p' ||
		video.r_frame_rate !== '60/1' || Number(video.nb_read_frames) !== names.length) {
		throw new Error('Encoded footage does not match the 1280x720, silent, 60 fps input sequence.');
	}
	renameSync(temporary, output);
	console.log(`Encoded ${names.length} frames (${(names.length / 60).toFixed(2)} seconds) to ${output}`);
} finally {
	rmSync(temporary, { force: true });
}
