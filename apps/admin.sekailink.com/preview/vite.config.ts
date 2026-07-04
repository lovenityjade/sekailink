import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      '/identity': {
        target: 'http://127.0.0.1:19095',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/identity/, ''),
      },
      '/lobby-admin': {
        target: 'http://127.0.0.1:19096',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/lobby-admin/, ''),
      },
      '/chat-gateway': {
        target: 'http://127.0.0.1:19098',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/chat-gateway/, ''),
      },
      '/admin-agent': {
        target: 'http://127.0.0.1:19091',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/admin-agent/, ''),
      },
      '/room-server': {
        target: 'http://127.0.0.1:19097',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/room-server/, ''),
      },
    },
  },
})
