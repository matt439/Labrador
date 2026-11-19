// Install before the page or its WASM loader creates a context. Both lookup
// methods must agree about the extension's presence.
export function hide_extension_script(name) {
	return `(() => {
		const hidden = ${JSON.stringify(name)};
		for (const type of [self.WebGLRenderingContext, self.WebGL2RenderingContext]) {
			if (!type) { continue; }
			const get = type.prototype.getExtension;
			type.prototype.getExtension = function (wanted) {
				return wanted === hidden ? null : get.call(this, wanted);
			};
			const list = type.prototype.getSupportedExtensions;
			type.prototype.getSupportedExtensions = function () {
				const names = list.call(this);
				return names ? names.filter((each) => each !== hidden) : names;
			};
		}
	})();`;
}
