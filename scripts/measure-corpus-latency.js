#!/usr/bin/env node
'use strict';

/**
 * Measure the language-server executable on the V32 corpus against
 * "Set interactive latency budgets" (cpp-language-server-spec ticket 07).
 *
 * Stdio wall-clock from this client, package.json defaults (no settings push;
 * the native defaults equal package.json), warm language server, median of 5.
 * "Warm" = the startup index walk's `workspace/codeLens/refresh` has arrived.
 *
 * Usage: node scripts/measure-corpus-latency.js [--root C:\vxbd320] [--runs 5] [--exe path	oertex-adt-ls.exe]
 * Writes a JSON report to stdout's last line block and a table above it.
 */

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('./lsp-golden-harness');

const args = process.argv.slice(2);
function arg(name, fallback) {
    const i = args.indexOf(name);
    return i >= 0 ? args[i + 1] : fallback;
}

const CORPUS = arg('--root', 'C:\\vxbd320');
const RUNS = Number(arg('--runs', '5'));
const FOLDERS = ['customa', 'system', 'usera']; // customaV32.code-workspace order
const FIXTURE_DIR = path.join(CORPUS, 'customa', 'complibs', 'frmtool_profcoloring_frame');
const LONG = 600000;
// --strict pushes the user's Cursor settings (strict-mode latency bar, impl ticket 60).
const STRICT = args.includes('--strict');
const STRICT_SETTINGS = {
    vertexadt: {
        strictMode: { enabled: true },
        calls: { requireCommonIndexes: true },
        diagnostics: { maxNumberOfProblems: 1000 },
    },
};

const BUDGETS = {
    typical: { diagnostics: 100, hover: 20, completion: 20, tabSwitch: 50 },
    outlier: { diagnostics: 200, hover: 20, completion: 20, tabSwitch: 100 },
};

// Hover on a variable (extract path, the heavier cheap hover), completion right
// after an identifier, edit = one identifier gets/loses a trailing character.
const CASES = {
    typical: {
        file: 'PICK_DET_AREA_EMI.prg',
        hover: { line: 'This = Pce', word: 'Pce' }, // loLP1 hovers null; Pce is non-null
        completion: { line: 'vec_copy(loEP2) loLP2', word: 'loLP2' },
        edit: { line: 'loLP1=  get("LP1")', word: 'loLP1' },
    },
    outlier: {
        file: 'walldetailing.prg',
        hover: { line: 'ok2 = is_in_vxgeom(Geom,CP1,0,1)', word: 'CP1' },
        completion: { line: 'dist = vec_len(D)', word: 'dist' },
        edit: { line: 'mindist = dist', word: 'mindist' },
    },
};

const now = () => Number(process.hrtime.bigint()) / 1e6;
const median = (xs) => {
    const s = [...xs].sort((a, b) => a - b);
    return s.length % 2 ? s[(s.length - 1) / 2] : (s[s.length / 2 - 1] + s[s.length / 2]) / 2;
};
const r1 = (x) => Math.round(x * 10) / 10;
const T0 = now();
const log = (msg) => process.stderr.write(`[${r1(now() - T0)} ms] ${msg}
`);

function locate(lines, spec) {
    const line = lines.findIndex((l) => l.trim() === spec.line);
    if (line < 0) throw new Error(`fixture line not found: ${spec.line}`);
    const character = lines[line].indexOf(spec.word);
    return { line, character, end: character + spec.word.length };
}

function launch() {
    const folders = FOLDERS.map((name) => ({
        uri: pathToFileURL(path.join(CORPUS, name)).href,
        name,
    }));
    const session = LspSession.launch({ executable: arg('--exe', undefined), rootUri: folders[0].uri, workspaceFolders: folders });
    session.codeLensRefreshAt = [];
    const original = session._replyToServerRequest.bind(session);
    session._replyToServerRequest = (message) => {
        if (/^workspace\/.*\/refresh$/.test(message.method)) {
            if (message.method === 'workspace/codeLens/refresh') session.codeLensRefreshAt.push(now());
            session._write({ jsonrpc: '2.0', id: message.id, result: null });
            return;
        }
        original(message);
    };
    return session;
}

async function waitFor(predicate, timeoutMs) {
    const start = now();
    while (!predicate()) {
        if (now() - start > timeoutMs) throw new Error('timed out waiting');
        await new Promise((r) => setTimeout(r, 5));
    }
}

async function measureFile(session, name, spec) {
    const filePath = path.join(FIXTURE_DIR, spec.file);
    const uri = pathToFileURL(filePath).href;
    const text = fs.readFileSync(filePath, 'utf8');
    const lines = text.split(/\r?\n/);
    const hoverPos = locate(lines, spec.hover);
    const completionPos = locate(lines, spec.completion);
    const editPos = locate(lines, spec.edit);
    let version = 0;
    const out = { open: [], editSettle: [], hover: [], completion: [], tabSwitch: [], counts: {} };

    // Open diagnostics: didOpen -> pull for that version returns.
    for (let i = 0; i < RUNS; i++) {
        version += 1;
        const t = now();
        session.didOpen({ uri, version, text });
        const report = await session.diagnostic(uri, LONG);
        out.open.push(now() - t);
        log(`${name} open ${i + 1}: ${r1(now() - t)} ms`);
        out.counts.diagnostics = (report.items || []).length;
        session.didClose(uri);
        await session.waitQuiet(150);
    }

    version += 1;
    session.didOpen({ uri, version, text });
    let last = await session.diagnostic(uri, LONG);
    let edited = false;
    const editOnce = () => {
        version += 1;
        const at = { line: editPos.line, character: editPos.end };
        const change = edited
            ? { range: { start: at, end: { line: at.line, character: at.character + 1 } }, text: '' }
            : { range: { start: at, end: at }, text: 'x' };
        edited = !edited;
        session.didChange({ uri, version, contentChanges: [change] });
    };

    // Edit-settle: one-identifier didChange -> diagnostic pull AND tokens returned.
    for (let i = 0; i < RUNS; i++) {
        const t = now();
        editOnce();
        const [report, tokens] = await Promise.all([
            session.diagnostic(uri, LONG),
            session.semanticTokensFull(uri, LONG),
        ]);
        out.editSettle.push(now() - t);
        log(`${name} editSettle ${i + 1}: ${r1(now() - t)} ms`);
        last = report;
        out.counts.tokenInts = tokens && tokens.data ? tokens.data.length : 0;
        await session.waitQuiet(150);
    }

    // Hover / completion: sent right after didChange while this document validates.
    for (const kind of ['hover', 'completion']) {
        for (let i = 0; i < RUNS; i++) {
            editOnce();
            const heavy = Promise.all([session.diagnostic(uri, LONG), session.semanticTokensFull(uri, LONG)]);
            const pos = kind === 'hover' ? hoverPos : completionPos;
            const char = kind === 'hover' ? pos.character + 1 : pos.end;
            const t = now();
            const result = await session[kind](uri, pos.line, char, LONG);
            out[kind].push(now() - t);
            log(`${name} ${kind} ${i + 1}: ${r1(now() - t)} ms`);
            if (kind === 'hover') out.counts.hoverNonNull = result != null;
            else out.counts.completionItems = Array.isArray(result) ? result.length : (result && result.items ? result.items.length : 0);
            const heavyStart = now();
            const [report] = await heavy;
            out.counts[`${kind}HeavyTailMs`] = r1(now() - heavyStart);
            last = report;
            await session.waitQuiet(150);
        }
    }

    // Tab-switch to an already-open, unchanged document: pull with previousResultId + tokens.
    for (let i = 0; i < RUNS; i++) {
        const t = now();
        const [report] = await Promise.all([
            session.diagnostic(uri, LONG, last.resultId),
            session.semanticTokensFull(uri, LONG),
        ]);
        out.tabSwitch.push(now() - t);
        log(`${name} tabSwitch ${i + 1}: ${r1(now() - t)} ms`);
        out.counts.tabSwitchKind = report.kind;
        await session.waitQuiet(150);
    }

    session.didClose(uri);
    return out;
}

async function main() {
    const session = launch();
    const spawnAt = now();
    await session.initialize(LONG);
    if (STRICT) session.didChangeConfiguration(STRICT_SETTINGS);
    log(`initialized${STRICT ? ' (strict settings pushed)' : ''}; waiting for startup index walk`);
    const initializedAt = now();
    await waitFor(() => session.codeLensRefreshAt.length > 0, LONG);
    const walkMs = session.codeLensRefreshAt[0] - initializedAt;
    log(`startup index walk done: ${r1(walkMs)} ms`);
    await session.waitQuiet(500);

    const report = {
        executable: 'server/vertex-adt-ls.exe',
        workspace: FOLDERS.map((f) => path.join(CORPUS, f)),
        runs: RUNS,
        settings: STRICT ? STRICT_SETTINGS : 'package.json defaults',
        initializeMs: r1(initializedAt - spawnAt),
        startupIndexWalkMs: r1(walkMs),
        files: {},
    };

    for (const [name, spec] of Object.entries(CASES)) {
        const m = await measureFile(session, name, spec);
        const b = BUDGETS[name];
        const row = (samples, budget) => ({
            samples: samples.map(r1),
            median: r1(median(samples)),
            budget,
            pass: median(samples) <= budget,
        });
        report.files[name] = {
            file: spec.file,
            open: row(m.open, b.diagnostics),
            editSettle: row(m.editSettle, b.diagnostics),
            hover: row(m.hover, b.hover),
            completion: row(m.completion, b.completion),
            tabSwitch: row(m.tabSwitch, b.tabSwitch),
            counts: m.counts,
        };
    }

    await session.shutdownAndExit(LONG);

    console.log(`startup index walk: ${report.startupIndexWalkMs} ms (initialize ${report.initializeMs} ms)`);
    for (const [name, f] of Object.entries(report.files)) {
        console.log(`\n${name}: ${f.file}`);
        for (const cls of ['open', 'editSettle', 'hover', 'completion', 'tabSwitch']) {
            const c = f[cls];
            console.log(`  ${cls.padEnd(11)} median ${String(c.median).padStart(8)} ms / ${String(c.budget).padStart(3)} ms  ${c.pass ? 'PASS' : 'FAIL'}  [${c.samples.join(', ')}]`);
        }
        console.log(`  counts ${JSON.stringify(f.counts)}`);
    }
    console.log('\n' + JSON.stringify(report, null, 2));
}

main().catch((err) => {
    console.error(err && err.stack ? err.stack : err);
    process.exit(1);
});
