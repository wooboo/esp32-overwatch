/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,ts,jsx,tsx}'],
  theme: {
    extend: {
      colors: {
        bg: '#0f1720',
        card: '#111926',
        text: '#e8edf3',
        muted: '#94a3b8',
        accent: '#2dd4bf',
        'accent-hover': '#20b9a8',
        danger: '#f87171',
        border: '#30404f',
        'input-bg': '#15202b',
        'chip-bg': '#24303c',
        'chip-ok': '#1f6f3c',
        'chip-bad': '#5a1d1d',
      },
      fontFamily: {
        sans: ['Inter', 'Segoe UI', 'system-ui', '-apple-system', 'sans-serif'],
      },
    },
  },
  plugins: [],
};
