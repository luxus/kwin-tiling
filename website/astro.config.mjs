// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

import cloudflare from '@astrojs/cloudflare';

// https://astro.build/config
export default defineConfig({
  // Cloudflare Pages hosting. Update `site` to your custom domain when set.
  site: 'https://kwin-tiling.pages.dev',

  base: '/',

  integrations: [
      starlight({
          title: 'KWin Tiling',
          description:
              'Native dynamic tiling patched into KWin — master-stack, stacked, scrolling and centred layouts, gaps, float/ignore window rules, and a settings KCM.',
          social: [{ icon: 'github', label: 'GitHub', href: 'https://github.com/luxus/kwin-tiling' }],
          sidebar: [
              { label: 'Overview', slug: '' },
              { label: 'Design', slug: 'design' },
              { label: 'Features', slug: 'features' },
              { label: 'Usage & Shortcuts', slug: 'usage' },
              { label: 'Roadmap', slug: 'roadmap' },
          ],
      }),
	],

  adapter: cloudflare(),
});