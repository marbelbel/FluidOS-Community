/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,ts,jsx,tsx}'],
  theme: {
    extend: {
      colors: {
        fluid: {
          50: '#f0f6ff',
          100: '#e0eeff',
          200: '#c0ddff',
          300: '#94c5ff',
          400: '#60a3ff',
          500: '#3b82f6',
          600: '#2563eb',
          700: '#1d4fd9',
          800: '#1e40af',
          900: '#1e3a8a',
        },
        accent: {
          cyan: '#22d3ee',
          teal: '#2dd4bf',
          mint: '#6ee7b7',
        },
        glass: {
          light: 'rgba(255,255,255,0.18)',
          mid: 'rgba(255,255,255,0.12)',
          dark: 'rgba(15,23,42,0.55)',
        },
      },
      fontFamily: {
        sans: ['Inter', 'SF Pro Display', '-apple-system', 'system-ui', 'sans-serif'],
        mono: ['JetBrains Mono', 'SF Mono', 'ui-monospace', 'monospace'],
      },
      backdropBlur: {
        xs: '2px',
        '4xl': '40px',
        '5xl': '64px',
      },
      boxShadow: {
        glass: '0 8px 32px rgba(0,0,0,0.18), inset 0 1px 1px rgba(255,255,255,0.25)',
        'glass-sm': '0 4px 16px rgba(0,0,0,0.14), inset 0 1px 1px rgba(255,255,255,0.2)',
        'glass-lg': '0 24px 80px rgba(0,0,0,0.30), inset 0 1px 1px rgba(255,255,255,0.2)',
        dock: '0 12px 40px rgba(0,0,0,0.28), inset 0 1px 2px rgba(255,255,255,0.3)',
      },
      animation: {
        'fade-in': 'fade-in 0.6s ease forwards',
        'fade-in-slow': 'fade-in 1.2s ease forwards',
        'liquid': 'liquid 18s ease-in-out infinite',
        'liquid-fast': 'liquid 12s ease-in-out infinite',
        'breath': 'breath 8s ease-in-out infinite',
        'shake': 'shake 0.45s ease',
        'slide-up': 'slide-up 0.5s cubic-bezier(0.16,1,0.3,1) forwards',
        'scale-in': 'scale-in 0.4s cubic-bezier(0.16,1,0.3,1) forwards',
        'spin-slow': 'spin 4s linear infinite',
        'pulse-glow': 'pulse-glow 2.4s ease-in-out infinite',
      },
      keyframes: {
        'fade-in': { from: { opacity: '0' }, to: { opacity: '1' } },
        'slide-up': { from: { opacity: '0', transform: 'translateY(24px)' }, to: { opacity: '1', transform: 'translateY(0)' } },
        'scale-in': { from: { opacity: '0', transform: 'scale(0.96)' }, to: { opacity: '1', transform: 'scale(1)' } },
        'shake': { '0%,100%': { transform: 'translateX(0)' }, '20%,60%': { transform: 'translateX(-8px)' }, '40%,80%': { transform: 'translateX(8px)' } },
        liquid: {
          '0%,100%': { transform: 'translate(0,0) scale(1)' },
          '33%': { transform: 'translate(6%, -4%) scale(1.08)' },
          '66%': { transform: 'translate(-4%, 6%) scale(0.94)' },
        },
        breath: {
          '0%,100%': { opacity: '0.55', transform: 'scale(1)' },
          '50%': { opacity: '0.85', transform: 'scale(1.06)' },
        },
        'pulse-glow': {
          '0%,100%': { opacity: '0.6' },
          '50%': { opacity: '1' },
        },
      },
    },
  },
  plugins: [],
};
