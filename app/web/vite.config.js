import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { defineConfig } from 'vite';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const readmePath = path.resolve(__dirname, '../../README.md');
let appVersion = '1.0.0';
try {
  const content = fs.readFileSync(readmePath, 'utf8');
  const match = content.match(
    /(?:<!--\s*robohero-version:\s*|\*\*Version:\s*)([0-9]+\.[0-9]+\.[0-9]+)/i
  );
  if (match) {
    appVersion = match[1];
  }
} catch (e) {
  console.warn('Could not read version from README.md', e);
}

export default defineConfig({
  base: './',
  define: {
    __APP_VERSION__: JSON.stringify(appVersion),
  },
  server: {
    host: '0.0.0.0',
    port: 3000,
  },
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
    sourcemap: false,
  },
});
