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

Cloudflare Pages builds from Git on push to `main`. In the project settings:

| Setting | Value |
| --- | --- |
| Root directory | `website` |
| Build command | `npm run build` |
| Build output directory | `dist` |
| Framework preset | Astro |

No `wrangler.toml` or deploy command — Pages uploads `dist/` after the build.
