// The Emscripten version Labrador's browser build is built and tested with,
// read from the pin in CI's wasm job, so a page that tells a reader which
// version to install cannot name another one.
import { readRepositoryFile } from './repository';

const WORKFLOW = '.github/workflows/ci.yml';
const pin = readRepositoryFile(WORKFLOW).match(/^\s*EMSDK_VERSION:\s*(\S+)\s*$/m);

if (!pin) {
	throw new Error(`No EMSDK_VERSION in ${WORKFLOW}; the browser build page needs the pinned Emscripten version.`);
}

export const EMSDK_VERSION: string = pin[1];
