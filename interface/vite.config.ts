import { defineConfig } from 'vite';
import preact from '@preact/preset-vite';
import { viteSingleFile } from 'vite-plugin-singlefile';
import { compression } from 'vite-plugin-compression2';

export default defineConfig({
  plugins: [
    preact(),
    viteSingleFile(),
    compression({
      algorithm: 'gzip',
      include: /\.(html)$/i,
      deleteOriginalAssets: false,
    }),
  ],
  server: {
    port: 5173,
    proxy: {
      '/ws': {
        target: 'ws://localhost:3001',
        ws: true,
      },
    },
  },
  build: {
    outDir: '../data',
    emptyOutDir: true,
    minify: 'terser',
    terserOptions: {
      compress: {
        drop_console: true,
        drop_debugger: true,
      },
    },
  },
});
