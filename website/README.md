# The Labrador website

The public site: a showcase, a getting-started path and the documentation.
[docs/website-plan.md](../docs/website-plan.md) is the plan it implements and
says what is decided and what is not.

It is a static site built with [Astro](https://astro.build/) and
[Starlight](https://starlight.astro.build/). Node is the only thing it needs.
Nothing in the engine's CMake build depends on this folder, and nothing here
needs the engine's toolchain.

```
npm ci            # once, and after package-lock.json changes
npm run dev       # a local server that reloads on edits, including to the files below
npm run check     # types
npm run build     # the site, into dist/ - fails on any broken internal link
npm run preview   # serve dist/
```

## It reads the rest of the repository

The site is built from the checkout it sits in, not from copies. At build time
it reads:

| What | From | Into |
| --- | --- | --- |
| The three design documents, as written | `docs/design/*.md` | `/docs/design/...` |
| A reference page for every public header | `engine/<module>/*.h` | `/docs/reference/<module>/...` |
| Which samples and tests include each header | `samples/`, `tests/` | the foot of its reference page |
| Quoted passages of code | `samples/`, `tests/` | wherever a page uses `<SourceFile>` |
| The `vcpkg.json` a new project needs | `vcpkg.json` | Get Started |
| The include lines a copied sample must change | `samples/minimal/` | Get Started |
| The revision all of it describes | `git rev-parse HEAD` | every page's footer |

So **a change outside `website/` can break the site build**, and it is meant
to. Each of these fails naming the file rather than publishing a page that is
wrong (PHILOSOPHY T6):

- a quoted passage whose first or last line is gone, or whose first line is no
  longer unique (`src/lib/excerpt.ts`);
- a relative link in a published design document to a file that does not
  exist (`src/lib/markdown-plugins.ts`);
- a header the reference reader does not understand (`src/lib/cpp-header.ts`);
- a header path written as code in a page, such as `engine/scene/scene.h`, that the checkout
  does not have (`src/lib/reference.ts`, `codeLink`);
- a public header that `src/lib/published.ts` neither publishes nor leaves out
  by name (`src/lib/reference.ts`, `unlistedHeaders`);
- any internal link or `#anchor` that does not resolve
  ([starlight-links-validator](https://github.com/HiDeoo/starlight-links-validator)).

`.github/workflows/website.yml` runs the check and the build on every push.

## Where things are

```
astro.config.ts           Starlight, the sidebar, and the three Markdown plugins
src/content.config.ts     one collection, filled by src/lib/content-loader.ts
src/content/docs/         pages written for the site (.mdx)
  index.mdx, why.mdx, examples.mdx    the three splash pages
  docs/                               everything under /docs/: get-started/,
                                      concepts/, guides/, troubleshooting.mdx,
                                      and the reference's introduction
src/lib/
  repository.ts           where the checkout is, its revision, GitHub links
  published.ts            THE list of documents and headers the site publishes
  content-loader.ts       adds those to the collection beside the site's pages
  markdown-plugins.ts     relative links in published documents, diagrams, and
                          code-formatted names linked to the reference
  cpp-header.ts           the line reader behind the reference pages
  reference.ts            a parsed header as a page, and the index of names
                          and members every page links through
  excerpt.ts              a passage of a file, found by its text
src/components/           SourceFile, RepoLink, Capture and friends;
                          overrides/ holds the Starlight components the site
                          replaces
src/assets/captures/      frames of LineSweeper, written by
                          tools/linesweeper_capture/ and never edited by hand
src/assets/footage/       the encoded LineSweeper video, served on Home and Examples
scripts/encode-footage.mjs  offline PNG-to-MP4 encoding and format verification
src/walkthrough/          the files Get Started tells a reader to create
src/styles/site.css       colours and the few layouts Starlight lacks
```

## Doing the common things

**Quote code.** Use `<SourceFile path="samples/minimal/main.cpp" from="..." to="..." />`,
where `from` and `to` are the beginnings of the first and last lines wanted -
never line numbers, which would quietly quote the wrong lines after an edit. A
`to` that starts with `}` means the brace at the same depth as `from`.

**Link to the API reference.** Write the name as code: `` `Scene` ``,
`` `StateContext::push` `` or `` `engine/scene/scene.h` ``. The first mention in
each section links to its place in the reference by itself, a member to its own
section where it has one. Nothing written by hand has to track the anchors.

**Link to a file in the repository.** Use `<RepoLink path="...">`, which links
to it at the built revision and fails the build if it is not there. For an
engine header, write its path as code instead, which reaches its reference page.

**Publish another design document or header.** Add it to `src/lib/published.ts`:
a design document to `DESIGN_DOCUMENTS`, a header to its module in
`REFERENCE_MODULES` with the label the sidebar shows. That is the whole of it:
the loader, the link rewriting and the sidebar all read those lists. A new
public header that is in neither `REFERENCE_MODULES` nor `UNPUBLISHED_HEADERS`
fails the build. A header that the reader rejects names its file and line; see
the reference's own introduction page for what the reader does and does not
understand, and why it was chosen over Doxygen and Clang.

**Show a frame of the sample.** Import it from `src/assets/captures/` and use
`<Capture>`, which serves the PNG as it is. The frames are made by
`tools/linesweeper_capture/`, whose README says how to change the script and
regenerate them; regenerate rather than edit, and look at every image that
changes.

**Regenerate the video.** Build `LineSweeperCapture` with `x64-release`, then
run these from the repository root with FFmpeg (`ffmpeg` and `ffprobe`) on
`PATH`:

```powershell
out\build\x64-release\tools\linesweeper_capture\LineSweeperCapture.exe out\website-footage tools\linesweeper_capture\footage.txt --sequence
if ($LASTEXITCODE -eq 0) {
    node website/scripts/encode-footage.mjs out/website-footage/frames
} else {
    throw 'Capture failed; do not encode its partial frames.'
}
```

Choose a fresh capture output directory for each run: the tool refuses a
nonempty `frames/` folder so old frames cannot become part of the next clip.
Encode only after capture exits successfully; a refused script can leave a
partial sequence, which is useful for diagnosis but is not the finished video.
The script and the capture tool's README describe the match. Each PNG is one
presentation update at 1/60 second, including the particles after top-out;
the encoder verifies that every frame reached a silent 1280×720, 60 fps H.264
MP4. It uses CRF 20 and YUV 4:2:0, so the video is compressed rather than an
exact pixel contract. The four PNG stills remain the exact frames.

Watch the whole clip before keeping a regeneration, including the clear and
top-out effects. Encoding needs FFmpeg only when replacing the checked-in
MP4: `npm ci`, `npm run check`, `npm run build` and CI still need only Node.
`LineSweeperFootage` shares the player across Home and Examples, with an
existing still as its poster, native controls, a download link and a text
description. It neither autoplays nor preloads the video.

**Change the Get Started project files.** Edit them in `src/walkthrough/`, then
build the walkthrough again by hand before publishing; its README says how it
was built.

## Not done yet

Hosting, the domain and deployment are undecided, so nothing here publishes
anything; there is no `site` URL in the config, and so no sitemap. The
collision, audio and local-multiplayer guides wait on a sample that does each.
The plan's *Open decisions* table is the list.
