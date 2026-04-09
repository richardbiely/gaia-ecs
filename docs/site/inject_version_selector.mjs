import { readFile, readdir, writeFile } from "node:fs/promises";
import { resolve } from "node:path";

async function injectAssets(htmlPath, payload) {
  let text = await readFile(htmlPath, "utf8");
  if (!text.includes("window.GAIA_VERSION_SELECTOR_DATA")) {
    const marker = "</head>";
    if (!text.includes(marker)) {
      throw new Error(`${htmlPath} does not contain ${marker}`);
    }

    const inlinePayload =
      `  <script>window.GAIA_VERSION_SELECTOR_DATA = ${JSON.stringify(payload)};</script>\n`;

    text = text.replace(marker, inlinePayload + marker);
  }

  await writeFile(htmlPath, text, "utf8");
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
  const payload = JSON.parse(await readFile(resolve(siteRoot, "versions.json"), "utf8"));

  for await (const htmlPath of htmlFiles(siteRoot)) {
    await injectAssets(htmlPath, payload);
  }
}

await main();
