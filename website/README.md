# KWin Tiling — documentation site

Astro 7 + Starlight site for the `kwin-tiling` project (Overview, Features,
Usage & Shortcuts, Roadmap).

## Develop

```sh
npm install
npm run dev        # local dev server
npm run build      # static build into dist/
```

`scripts/verify.sh` does two clean builds as a sanity check.

## Content

- `src/content/docs/*.{md,mdx}` — the pages (Overview is `index.mdx`).
- `src/fragments/{shipped,open}.md` — the single-source shipped/open feature
  lists, imported into Features and Roadmap.

## Deploy

Cloudflare Pages builds from Git on push to `main`. In the project settings
(**Settings → Builds**):

| Setting | Value |
| --- | --- |
| Root directory | `website` |
| Build command | `npm run build` |
| Build output directory | `dist` |
| Framework preset | Astro |
| Deploy command | *(leave empty)* |

Pages uploads `dist/` automatically after a successful build. Do **not** use
`npx wrangler pages deploy` here — that is for manual/CLI uploads and needs API
token permissions this project does not use.

### If you see a wrangler deploy error

Errors like `Must specify a directory of assets to deploy` or
`Authentication error [code: 10000]` mean a **deploy command** is still set to
`npx wrangler pages deploy`. Clear that field; keep the **build command** as
`npm run build`.
