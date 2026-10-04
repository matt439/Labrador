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
| Reference pages for a slice of the public headers | `engine/**/*.h` | `/docs/reference/...` |
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
- any internal link or `#anchor` that does not resolve
  ([starlight-links-validator](https://github.com/HiDeoo/starlight-links-validator)).

`.github/workflows/website.yml` runs the check and the build on every push.

## Where things are

```
astro.config.ts           Starlight, the sidebar, and the two Markdown plugins
src/content.config.ts     one collection, filled by src/lib/content-loader.ts
src/content/docs/         pages written for the site (.mdx)
  index.mdx, why.mdx, examples.mdx    the three splash pages
  docs/                               everything under /docs/
src/lib/
  repository.ts           where the checkout is, its revision, GitHub links
  published.ts            THE list of documents and headers the site publishes
  content-loader.ts       adds those to the collection beside the site's pages
  markdown-plugins.ts     relative links in published documents, and diagrams
  cpp-header.ts           the line reader behind the reference pages
  reference.ts            a parsed header as a page, with symbol links
  excerpt.ts              a passage of a file, found by its text
src/components/           SourceFile, RepoLink and friends; overrides/ holds
                          the Starlight components the site replaces
src/walkthrough/          the files Get Started tells a reader to create
src/styles/site.css       colours and the few layouts Starlight lacks
```

## Doing the common things

**Quote code.** Use `<SourceFile path="samples/minimal/main.cpp" from="..." to="..." />`,
where `from` and `to` are the beginnings of the first and last lines wanted -
never line numbers, which would quietly quote the wrong lines after an edit. A
`to` that starts with `}` means the brace at the same depth as `from`.

**Link to a file in the repository.** Use `<RepoLink path="...">`, which links
to it at the built revision and fails the build if it is not there.

**Publish another design document or header.** Add it to `src/lib/published.ts`.
That is the whole of it: the loader, the link rewriting and the sidebar all
read that list. A header that the reader rejects names its file and line; see
the reference's own introduction page for what the reader does and does not
understand, and why it was chosen over Doxygen and Clang.

**Change the Get Started project files.** Edit them in `src/walkthrough/`, then
build the walkthrough again by hand before publishing; its README says how it
was built.

## Not done yet

Hosting, the domain and deployment are undecided, so nothing here publishes
anything; there is no `site` URL in the config, and so no sitemap. There is no
screenshot or footage of either sample: none exists in the repository yet.
The plan's *Open decisions* table is the list.
