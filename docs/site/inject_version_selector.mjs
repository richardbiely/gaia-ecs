import { readFile, readdir, writeFile } from "node:fs/promises";
import { resolve, relative, dirname } from "node:path";

const CSS_FILE = "gaia-version-selector.css";
const JS_FILE = "gaia-version-selector.js";

function relPrefix(htmlPath, siteRoot) {
  const rel = relative(siteRoot, htmlPath);
  const depth = dirname(rel).split(/[\\/]/).filter((part) => part && part !== ".").length;
  return depth === 0 ? "./" : "../".repeat(depth);
}

async function injectAssets(htmlPath, siteRoot) {
  const text = await readFile(htmlPath, "utf8");

  if (text.includes(JS_FILE) || text.includes(CSS_FILE)) {
    return;
  }

  const prefix = relPrefix(htmlPath, siteRoot);
  const assets =
    `  <link rel="stylesheet" href="${prefix}${CSS_FILE}">\n` +
    `  <script defer src="${prefix}${JS_FILE}"></script>\n`;

  const marker = "</head>";
  if (!text.includes(marker)) {
    throw new Error(`${htmlPath} does not contain ${marker}`);
  }

  await writeFile(htmlPath, text.replace(marker, assets + marker), "utf8");
}

async function* htmlFiles(rootDir) {
  const entries = await readdir(rootDir, { withFileTypes: true });

  for (const entry of entries) {
    const fullPath = resolve(rootDir, entry.name);

    if (entry.isDirectory()) {
      yield* htmlFiles(fullPath);
      continue;
    }

    if (entry.isFile() && entry.name.endsWith(".html")) {
      yield fullPath;
    }
  }
}

async function main() {
  if (process.argv.length !== 3) {
    console.error("usage: inject_version_selector.mjs <site-root>");
    process.exit(1);
  }

  const siteRoot = resolve(process.argv[2]);

  for await (const htmlPath of htmlFiles(siteRoot)) {
    await injectAssets(htmlPath, siteRoot);
  }
}

await main();
