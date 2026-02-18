module.exports = {
  root: true,
  env: {
    browser: true,
    es2022: true,
    node: true,
  },
  parser: "@typescript-eslint/parser",
  parserOptions: {
    ecmaVersion: "latest",
    sourceType: "module",
    ecmaFeatures: {
      jsx: true,
    },
  },
  plugins: ["@typescript-eslint", "react-hooks", "react-refresh"],
  extends: ["eslint:recommended", "plugin:@typescript-eslint/recommended", "plugin:react-hooks/recommended", "eslint-config-prettier"],
  rules: {
    "react-refresh/only-export-components": ["warn", { allowConstantExport: true }],
    "@typescript-eslint/no-explicit-any": "off",
    "@typescript-eslint/no-unused-vars": [
      "warn",
      {
        argsIgnorePattern: "^_",
        varsIgnorePattern: "^_",
        caughtErrorsIgnorePattern: "^_",
      },
    ],
    "@typescript-eslint/no-require-imports": "off",
    "no-empty": "off",
    "no-control-regex": "off",
    "no-useless-escape": "off",
  },
  ignorePatterns: [
    "dist/",
    "release/",
    "node_modules/",
    ".playwright-browsers/",
    "public/assets/sfx.js",
  ],
  overrides: [
    {
      files: ["electron/**/*.cjs", "scripts/**/*.mjs"],
      env: {
        node: true,
        browser: false,
      },
      rules: {
        "@typescript-eslint/no-unused-vars": "off",
      },
    },
    {
      files: [
        "src/components/AuthGate.tsx",
        "src/components/SocialDrawer.tsx",
        "src/components/TrackerPanel.tsx",
        "src/pages/GameManager.tsx",
        "src/pages/Lobby.tsx",
        "src/pages/PlayerOptions.tsx",
        "src/pages/RoomList.tsx",
        "src/pages/Settings.tsx",
      ],
      rules: {
        "react-hooks/exhaustive-deps": "off",
      },
    },
    {
      files: ["src/i18n/index.tsx"],
      rules: {
        "react-refresh/only-export-components": "off",
      },
    },
  ],
};
