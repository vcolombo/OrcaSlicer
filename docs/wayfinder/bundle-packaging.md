# Ticket #4 — Web bundle packaging path: findings

Question: how does a Vite-built web bundle get built, versioned, embedded, and served?
Traced against source (wreckfish @ 824b216f18). All paths relative to repo root.

## How `resources/web/` is embedded and served today

- **What it is:** plain checked-in static files. 794 files, ~22 MB (`resources/web/`):
  `guide/`, `homepage/`, `model/`, `dialog/` (Plugins, SpeedDial, Terminal, …),
  `flush/`, `orca/`, `elegoolink/`, `include/` (jquery, swiper, xterm, viewer),
  `js/`, `data/`, `image/`. No build step exists anywhere: no `package.json`
  (except inside vendored swiper `node_modules`, 42 files tracked in git),
  no node/npm/vite/webpack/esbuild reference in `CMakeLists.txt`,
  `src/CMakeLists.txt`, or CI workflows.
- **Serving:** every page loads via `file://<resources_dir>/web/...` into wxWebView:
  `WebViewHostDialog::build_resource_url()` (`src/slic3r/GUI/Widgets/WebViewHostDialog.cpp`),
  plus older direct `wxString::Format("file://%s/web/...")` call sites
  (`WebViewDialog.cpp` homepage, `Project.cpp` model page, `Plater.cpp`
  `web/orca/missing_connection.html`, `ElegooLink.cpp`
  `web/elegoolink/lan_service_web/index.html`). Backends: WebView2/Edge (Win),
  WKWebView (macOS), WebKit2GTK (Linux). Bridge: `AddScriptMessageHandler("wx")`
  + document-start user scripts (host theme contract). No HTTP involved.
- **Runtime resource location** (`src/OrcaSlicer.cpp`, `set_resources_dir`):
  macOS `OrcaSlicer.app/Contents/Resources`, Windows `<exe dir>/resources`,
  Linux FHS `SLIC3R_FHS_RESOURCES` (e.g. `/usr/share/OrcaSlicer`), else
  `<prefix>/resources` next to the binary (AppImage `$APPDIR/resources`).
- **Packaging (all three OSes copy the whole `resources/` tree verbatim):**
  - Windows: `install(DIRECTORY "${SLIC3R_RESOURCES_DIR}/" DESTINATION "./resources")`
    (`CMakeLists.txt:1252`) → NSIS installer (`CPACK_GENERATOR NSIS`); build tree
    uses a junction/symlink POST_BUILD step (`src/CMakeLists.txt:258-277`).
  - macOS: build tree symlinks `resources/` into `OrcaSlicer.app/Contents/Resources`
    (`src/CMakeLists.txt:334-357`); `build_release_macos.sh:293-296` resolves the
    symlink and copies the real tree into the shipped `.app`/dmg.
  - Linux: `build_linux_image.sh.in:183` does `cp -Rf ../resources "$APPDIR/resources"`
    (AppImage); FHS/Flatpak use the `install(DIRECTORY … DESTINATION
    ${SLIC3R_FHS_RESOURCES})` rule (`CMakeLists.txt:1261`).
  - Consequence: a new subdirectory under `resources/web/` ships on all platforms
    with zero packaging changes — no per-file manifest exists.
- **Versioning / caching today:** none. The only URL query used is `?lang=xx`
  (appended in `build_resource_url`). One page (`web/model/index.html`) carries
  `<meta http-equiv="Cache-Control" content="max-age=7200">`, meaningless for
  `file://` loads. No content hashes, no bundle versions, no invalidation logic.

## Precedent: a built bundle is already checked in

`resources/web/elegoolink/lan_service_web/index.html` (3.4 MB, single file) is a
Vite production build output — minified `<script type="module" crossorigin>` —
committed directly to the tree and served via `file://`. So "build-time vs
checked-in" has an in-tree answer already: **checked-in built artifact**.

## Options for the Wayfinder served bundle

1. **A. Checked-in prebuilt bundle** (recommend): Vite `dist/` committed under
   `resources/web/<app>/`, shipped by the existing whole-tree copy. Zero build-system
   or CI change; works offline/air-gapped; matches the elegoolink precedent.
   Costs: large binary-ish diffs on rebuild; stale-bundle risk (mitigate: keep the
   Vite source + lockfile in-repo, e.g. `webui/`, with a CI check that the committed
   bundle reproduces from source).
2. **B. Build-time Vite step in CMake:** source-only checkout, Node invoked during
   build. Always-fresh bundle and reviewable diffs, but adds a hard Node toolchain
   dependency to all three OS builds, CI images, and contributors; needs version
   pinning and breaks offline builds.
3. **C. Runtime download / CDN:** rejected — offline-first desktop app, plus
   signing/notarization and supply-chain exposure on every launch.

## Recommendation

**Option A**, with two refinements for the loopback-served case (tickets #2/#5/#6):

- **Serve the bundle over loopback HTTP from the embedded server** (stable
  `http://127.0.0.1:<port>` origin) rather than `file://`. Rationale: `file://`
  pages have an opaque/`null` origin, which complicates `fetch()` to the loopback
  API (CORS) and the token-auth lifecycle (#5). Serving the bundle from the same
  server that exposes the API makes same-origin fetch just work and gives real
  HTTP cache semantics. Hook-in stays trivial: the server's static-file root
  points at `<resources_dir>/web/<app>/`; `WebViewHostDialog`-style dialogs load
  the loopback URL instead of a `file://` URL.
- **Versioning/caching:** keep Vite's default content-hashed asset filenames
  (free cache-busting); serve `index.html` as `Cache-Control: no-cache` and hashed
  assets as `immutable`. Stamp the app version into the bundle (e.g. `version.json`
  or a meta tag) so the API/UI mismatch is detectable. If any page stays on
  `file://`, caching is a non-issue — no action needed.

Sources: `CMakeLists.txt:84,1251-1273,1324-1338`; `src/CMakeLists.txt:228-367`;
`src/OrcaSlicer.cpp:~7900-7929`; `src/slic3r/GUI/Widgets/WebViewHostDialog.cpp`;
`src/slic3r/GUI/Widgets/WebView.cpp:260-391`; `src/slic3r/GUI/WebViewDialog.cpp:39`;
`src/slic3r/GUI/Project.cpp:47`; `src/slic3r/GUI/Plater.cpp:3486`;
`src/slic3r/Utils/ElegooLink.cpp:343`; `build_release_macos.sh:185-296`;
`src/dev-utils/platform/unix/build_linux_image.sh.in:167-218`.
