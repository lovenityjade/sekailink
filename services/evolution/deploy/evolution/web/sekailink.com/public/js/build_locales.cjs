"use strict";
/* Regénérer js/locales.js après édition des fichiers dans locales_src/ :
   node build_locales.cjs
*/

const fs = require("fs");
const path = require("path");

const dir = __dirname;
const srcDir = path.join(dir, "locales_src");
const codes = ["fr", "en", "es", "ja", "zh-Hans", "zh-Hant", "ko", "de", "pt", "it"];

const out = {};
for (const c of codes) {
  const file = `${c}.cjs`;
  const p = path.join(srcDir, file);
  if (!fs.existsSync(p)) {
    console.error("Missing", p);
    process.exit(1);
  }
  // eslint-disable-next-line import/no-dynamic-require, global-require
  delete require.cache[require.resolve(p)];
  out[c] = require(p);
}

fs.writeFileSync(
  path.join(dir, "locales.js"),
  `window.SEKAILINK_LOCALES = ${JSON.stringify(out, null, 2)};\n`,
  "utf8"
);
console.log("Wrote locales.js");
