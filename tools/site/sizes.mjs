//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  What each page of a staged site (tools/build_site.sh) sends over the
//  network, as GitHub Pages serves it: every file the page loads, raw, gzip
//  (level 9) and brotli (quality 11), with nothing taken to be cached.
//
//      node tools/site/sizes.mjs <site dir> [--budget <bytes> --budget-page <page.html>]
//
//  A page's files are the page itself and what its HTML names locally (each
//  src= and href= that is not a link to another page or an absolute URL), and
//  for each Emscripten script X.js the X.wasm and X.data it fetches. It prints
//  one line per file and a total per page, and with --budget exits 1 unless
//  the budget page's gzip total (the larger of the two encodings, so it holds
//  whichever one the server picks) is at most <bytes>.
//

import { existsSync, readFileSync, readdirSync, realpathSync } from 'node:fs';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import zlib from 'node:zlib';

const usage = 'usage: node sizes.mjs <site dir> [--budget <bytes> --budget-page <page.html>]';

// The site-relative files a page loads, itself first.
export function pageFiles(site, page)
{
    const files = [page];
    const html = readFileSync(join(site, page), 'utf8').replace(/<!--[\s\S]*?-->/g, '');
    for(const match of html.matchAll(/<(script|link|img)\b[^>]*?\b(src|href)="([^"]+)"/g))
    {
        const url = match[3];
        if(/^[a-z]+:|^\/\/|^#/i.test(url))
            continue;
        const path = url.split(/[?#]/)[0];
        if(!files.includes(path))
            files.push(path);
        if(path.endsWith('.js'))
        {
            for(const ext of ['.wasm', '.data'])
            {
                const fetched = path.slice(0, -3) + ext;
                if(existsSync(join(site, fetched)) && !files.includes(fetched))
                    files.push(fetched);
            }
        }
    }
    return files;
}

export function encodedSizes(bytes)
{
    return {
        raw: bytes.length,
        gzip: zlib.gzipSync(bytes, { level: 9 }).length,
        brotli: zlib.brotliCompressSync(bytes, { params: { [zlib.constants.BROTLI_PARAM_QUALITY]: 11 } }).length,
    };
}

// {page: {files: [{path, raw, gzip, brotli}], raw, gzip, brotli}} for every
// .html at the site's top level
export function siteSizes(site)
{
    const pages = {};
    for(const page of readdirSync(site).filter(name => name.endsWith('.html')).sort())
    {
        const total = { files: [], raw: 0, gzip: 0, brotli: 0 };
        for(const path of pageFiles(site, page))
        {
            const sizes = encodedSizes(readFileSync(join(site, path)));
            total.files.push({ path, ...sizes });
            total.raw += sizes.raw;
            total.gzip += sizes.gzip;
            total.brotli += sizes.brotli;
        }
        pages[page] = total;
    }
    return pages;
}

const kb = bytes => `${(bytes/1e3).toFixed(1)} kB`;
const mb = bytes => `${(bytes/1e6).toFixed(2)} MB`;

function main()
{
    const argv = process.argv.slice(2);
    let site = null, budget = null, budgetPage = null;
    for(let i = 0; i < argv.length; ++i)
    {
        if(argv[i] === '--budget' && i + 1 < argv.length)
            budget = Number(argv[++i]);
        else if(argv[i] === '--budget-page' && i + 1 < argv.length)
            budgetPage = argv[++i];
        else if(!argv[i].startsWith('--') && site === null)
            site = argv[i];
        else
            site = undefined;
    }
    if(!site || (budget === null) !== (budgetPage === null) || (budget !== null && !(budget > 0)))
    {
        console.error(usage);
        return 1;
    }

    const pages = siteSizes(site);
    const width = Math.max(...Object.values(pages).flatMap(p => p.files.map(f => f.path.length)), 5) + 2;
    const row = (name, sizes) => `${name.padEnd(width)}  ${kb(sizes.raw).padStart(10)}  ${kb(sizes.gzip).padStart(10)}  ${kb(sizes.brotli).padStart(10)}`;
    console.log(`${''.padEnd(width)}  ${'raw'.padStart(10)}  ${'gzip'.padStart(10)}  ${'brotli'.padStart(10)}`);
    for(const [page, total] of Object.entries(pages))
    {
        console.log(`${page}:`);
        for(const f of total.files)
            console.log(row(`  ${f.path}`, f));
        console.log(row('  total', total));
    }

    if(budget !== null)
    {
        const page = pages[budgetPage];
        if(!page)
        {
            console.error(`sizes.mjs: FAIL: ${budgetPage} is not a page of ${site}`);
            return 1;
        }
        if(page.gzip > budget)
        {
            console.error(`sizes.mjs: FAIL: ${budgetPage} sends ${mb(page.gzip)} gzip, over the budget of ${mb(budget)}`);
            return 1;
        }
        console.log(`sizes.mjs: ${budgetPage} sends ${mb(page.gzip)} gzip (${mb(page.brotli)} brotli), within the budget of ${mb(budget)}`);
    }
    return 0;
}

// Only when run as a script, so site.mjs can import siteSizes.
if(process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href)
    process.exit(main());
