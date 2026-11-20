#!/usr/bin/env node
'use strict';

/**
 * Headless stdio LSP client for language-server executable parity goldens.
 * Invoke from the repo root: npm run test:goldens
 *
 * Assertions for real goldens come from each test-fixtures/ TESTING.md
 * (except header-creator/). Family goldens under scripts/goldens/ run here
 * against server/vertex-adt-ls.exe. Do not harvest a Node JSON dump as the
 * oracle. Do not drive VS Code. Existing test-fixtures run-*-test.js
 * slice-tests stay Node regression only.
 */

const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');

const ROOT = path.resolve(__dirname, '..');
const LANGUAGE_SERVER_EXECUTABLE = path.join(ROOT, 'server', 'vertex-adt-ls.exe');
const CONTENT_TYPE = 'application/vscode-jsonrpc; charset=utf-8';
const HEADER_TERMINATOR = Buffer.from('\r\n\r\n');
const SETTLE_MS = 300;
const DEFAULT_TIMEOUT_MS = 10000;

class AssertionError extends Error {
    constructor(message) {
        super(message);
        this.name = 'AssertionError';
    }
}

function assert(condition, message) {
    if (!condition) {
        throw new AssertionError(message);
    }
}

function assertEqual(actual, expected, message) {
    const actualJson = JSON.stringify(actual);
    const expectedJson = JSON.stringify(expected);
    assert(actualJson === expectedJson, `${message}: expected ${expectedJson}, got ${actualJson}`);
}

function delay(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

function createMessageParser(onMessage) {
    let buffer = Buffer.alloc(0);
    return function push(chunk) {
        buffer = Buffer.concat([buffer, chunk]);
        while (true) {
            const headerEnd = buffer.indexOf(HEADER_TERMINATOR);
            if (headerEnd < 0) {
                return;
            }
            const header = buffer.slice(0, headerEnd).toString('ascii');
            const match = /Content-Length:\s*(\d+)/i.exec(header);
            if (!match) {
                throw new Error(`LSP header missing Content-Length:\n${header}`);
            }
            const length = Number(match[1]);
            const bodyStart = headerEnd + HEADER_TERMINATOR.length;
            if (buffer.length < bodyStart + length) {
                return;
            }
            const body = buffer.slice(bodyStart, bodyStart + length).toString('utf8');
            buffer = buffer.slice(bodyStart + length);
            onMessage(JSON.parse(body));
        }
    };
}

class LspSession {
    constructor(child, options) {
        this._child = child;
        this._rootUri = options.rootUri;
        this._extensionUri = options.extensionUri;
        this._workspaceFolders = options.workspaceFolders;
        this._nextId = 1;
        this._pending = new Map();
        this._stderr = '';
        this._exitCode = null;
        this._initializeResult = null;
        this._publishDiagnostics = new Map();
        this._notifications = [];
        this._closed = new Promise((resolve) => {
            this._resolveClosed = resolve;
        });

        const parse = createMessageParser((message) => this._dispatch(message));
        child.stdout.on('data', parse);
        child.stderr.on('data', (chunk) => {
            this._stderr += chunk.toString('utf8');
        });
        child.on('error', (err) => {
            this._failAllPending(err);
        });
        child.on('exit', (code, signal) => {
            this._exitCode = code;
            const err = new Error(
                `language-server executable exited ${code}${signal ? ` signal ${signal}` : ''}` +
                (this._stderr ? `\n${this._stderr}` : '')
            );
            this._failAllPending(err);
            this._resolveClosed({ code, signal });
        });
    }

    static launch(options = {}) {
        // VERTEX_ADT_LS_EXE: run the goldens against another build (the
        // editor may hold server/vertex-adt-ls.exe open).
        const executable =
            options.executable || process.env.VERTEX_ADT_LS_EXE || LANGUAGE_SERVER_EXECUTABLE;
        if (!fs.existsSync(executable)) {
            throw new Error(
                `language-server executable not found at ${executable}. ` +
                'Build it first:\n' +
                '  call "C:\\Program Files\\Microsoft Visual Studio\\18\\Insiders\\Common7\\Tools\\VsDevCmd.bat" -arch=amd64 -host_arch=amd64\n' +
                '  cmake -S server -B server/build -A x64\n' +
                '  cmake --build server/build --config Release'
            );
        }

        const rootUri = options.rootUri || pathToFileURL(ROOT).href;
        const extensionUri = options.extensionUri || pathToFileURL(ROOT).href;
        const workspaceFolders = options.workspaceFolders || [
            { uri: rootUri, name: path.basename(ROOT) },
        ];

        const child = spawn(executable, [], {
            cwd: ROOT,
            stdio: ['pipe', 'pipe', 'pipe'],
            windowsHide: true,
        });
        child.stdin.setDefaultEncoding('utf8');

        return new LspSession(child, { rootUri, extensionUri, workspaceFolders });
    }

    get initializeResult() {
        return this._initializeResult;
    }

    diagnosticsFor(uri) {
        return this._publishDiagnostics.get(uri) || [];
    }

    async initialize(timeoutMs = DEFAULT_TIMEOUT_MS) {
        const result = await this.request('initialize', {
            processId: process.pid,
            clientInfo: { name: 'vertex-adt-golden-harness' },
            locale: 'en',
            rootUri: this._rootUri,
            capabilities: {
                workspace: {
                    configuration: true,
                    workspaceFolders: true,
                },
                textDocument: {
                    hover: { contentFormat: ['markdown', 'plaintext'] },
                    publishDiagnostics: { relatedInformation: true },
                    diagnostic: {},
                },
            },
            initializationOptions: {
                extensionUri: this._extensionUri,
            },
            workspaceFolders: this._workspaceFolders,
        }, timeoutMs);
        this._initializeResult = result;
        this.notify('initialized', {});
        return result;
    }

    didOpen({ uri, languageId = 'vertexadt', version = 1, text = '' }) {
        this.notify('textDocument/didOpen', {
            textDocument: { uri, languageId, version, text },
        });
    }

    didChange({ uri, version, contentChanges }) {
        this.notify('textDocument/didChange', {
            textDocument: { uri, version },
            contentChanges,
        });
    }

    didClose(uri) {
        this.notify('textDocument/didClose', {
            textDocument: { uri },
        });
    }

    hover(uri, line = 0, character = 0, timeoutMs = DEFAULT_TIMEOUT_MS) {
        return this.request('textDocument/hover', {
            textDocument: { uri },
            position: { line, character },
        }, timeoutMs);
    }

    completion(uri, line = 0, character = 0, timeoutMs = DEFAULT_TIMEOUT_MS) {
        return this.request('textDocument/completion', {
            textDocument: { uri },
            position: { line, character },
        }, timeoutMs);
    }

    completionResolve(item, timeoutMs = DEFAULT_TIMEOUT_MS) {
        return this.request('completionItem/resolve', item, timeoutMs);
    }

    didChangeConfiguration(settings) {
        this.notify('workspace/didChangeConfiguration', { settings });
    }

    diagnostic(uri, timeoutMs = DEFAULT_TIMEOUT_MS, previousResultId) {
        const params = { textDocument: { uri } };
        if (previousResultId !== undefined) {
            params.previousResultId = previousResultId;
        }
        return this.request('textDocument/diagnostic', params, timeoutMs);
    }

    diagnosticAllowError(uri, timeoutMs = DEFAULT_TIMEOUT_MS) {
        return this.requestAllowError('textDocument/diagnostic', {
            textDocument: { uri },
        }, timeoutMs);
    }

    semanticTokensFull(uri, timeoutMs = DEFAULT_TIMEOUT_MS) {
        return this.request('textDocument/semanticTokens/full', {
            textDocument: { uri },
        }, timeoutMs);
    }

    semanticTokensFullAllowError(uri, timeoutMs = DEFAULT_TIMEOUT_MS) {
        return this.requestAllowError('textDocument/semanticTokens/full', {
            textDocument: { uri },
        }, timeoutMs);
    }

    async waitQuiet(ms = SETTLE_MS) {
        await delay(ms);
    }

    notify(method, params) {
        const message = { jsonrpc: '2.0', method };
        if (params !== undefined) {
            message.params = params;
        }
        this._write(message);
    }

    request(method, params, timeoutMs = DEFAULT_TIMEOUT_MS) {
        return this._request(method, params, timeoutMs, false).then((payload) => payload.result);
    }

    requestAllowError(method, params, timeoutMs = DEFAULT_TIMEOUT_MS) {
        return this._request(method, params, timeoutMs, true);
    }

    _request(method, params, timeoutMs, allowError) {
        const id = this._nextId++;
        const message = { jsonrpc: '2.0', id, method };
        if (params !== undefined) {
            message.params = params;
        }
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                this._pending.delete(id);
                reject(new Error(`LSP request ${method} (${id}) timed out after ${timeoutMs}ms`));
            }, timeoutMs);
            this._pending.set(id, { resolve, reject, timer, method, allowError });
            this._write(message);
        });
    }

    kill() {
        try {
            this._child.kill();
        } catch {
            // already gone
        }
    }

    async shutdownAndExit(timeoutMs = DEFAULT_TIMEOUT_MS) {
        await this.request('shutdown', undefined, timeoutMs);
        this.notify('exit');
        try {
            this._child.stdin.end();
        } catch {
            // already closed
        }
        let timeoutId;
        const timedOut = new Promise((_, reject) => {
            timeoutId = setTimeout(() => {
                this._child.kill();
                reject(new Error(`language-server executable did not exit after shutdown+exit within ${timeoutMs}ms`));
            }, timeoutMs);
        });
        try {
            const closed = await Promise.race([this._closed, timedOut]);
            assertEqual(closed.code, 0, 'language-server executable exit code');
            return closed;
        } finally {
            clearTimeout(timeoutId);
        }
    }

    _write(message) {
        const body = Buffer.from(JSON.stringify(message), 'utf8');
        const header = Buffer.from(
            `Content-Length: ${body.length}\r\nContent-Type: ${CONTENT_TYPE}\r\n\r\n`,
            'ascii'
        );
        this._child.stdin.write(Buffer.concat([header, body]));
    }

    _dispatch(message) {
        if (Object.prototype.hasOwnProperty.call(message, 'id') && !Object.prototype.hasOwnProperty.call(message, 'method')) {
            const pending = this._pending.get(message.id);
            if (!pending) {
                return;
            }
            this._pending.delete(message.id);
            clearTimeout(pending.timer);
            if (message.error) {
                if (pending.allowError) {
                    pending.resolve({ error: message.error });
                    return;
                }
                pending.reject(new Error(
                    `${pending.method} failed: ${message.error.message} (${message.error.code})`
                ));
                return;
            }
            pending.resolve({ result: message.result });
            return;
        }

        if (message.method && Object.prototype.hasOwnProperty.call(message, 'id')) {
            this._replyToServerRequest(message);
            return;
        }

        if (message.method) {
            this._notifications.push(message);
            if (message.method === 'textDocument/publishDiagnostics' && message.params) {
                this._publishDiagnostics.set(message.params.uri, message.params.diagnostics || []);
            }
        }
    }

    _replyToServerRequest(message) {
        let result = null;
        if (message.method === 'workspace/configuration') {
            const items = (message.params && message.params.items) || [];
            result = items.map(() => ({}));
        } else if (message.method === 'workspace/workspaceFolders') {
            result = this._workspaceFolders;
        } else if (
            message.method === 'client/registerCapability' ||
            message.method === 'client/unregisterCapability' ||
            message.method === 'window/workDoneProgress/create'
        ) {
            result = null;
        } else {
            this._write({
                jsonrpc: '2.0',
                id: message.id,
                error: { code: -32601, message: `Method not found: ${message.method}` },
            });
            return;
        }
        this._write({ jsonrpc: '2.0', id: message.id, result });
    }

    _failAllPending(err) {
        for (const pending of this._pending.values()) {
            clearTimeout(pending.timer);
            pending.reject(err);
        }
        this._pending.clear();
    }
}

function textDocumentSyncChange(capabilities) {
    const sync = capabilities && capabilities.textDocumentSync;
    if (sync && typeof sync === 'object') {
        return sync.change;
    }
    return sync;
}

async function runEmptyDocumentSmoke(session) {
    const uri = pathToFileURL(path.join(ROOT, 'test-fixtures', '.golden-smoke-empty.prg')).href;
    const result = await session.initialize();

    assert(result && result.capabilities, 'initialize result is missing capabilities');
    assertEqual(textDocumentSyncChange(result.capabilities), 2, 'textDocumentSync.change (Incremental)');
    assert(result.capabilities.hoverProvider, 'hoverProvider is advertised');
    assertEqual(
        result.capabilities.diagnosticProvider,
        { interFileDependencies: false, workspaceDiagnostics: false },
        'diagnosticProvider pull shape'
    );
    assertEqual(result.serverInfo && result.serverInfo.name, 'vertex-adt-ls', 'serverInfo.name');

    session.didOpen({ uri, languageId: 'vertexadt', version: 1, text: '' });
    const hover = await session.hover(uri, 0, 0);
    assertEqual(hover, null, 'empty-document hover');

    const completion = await session.completion(uri, 0, 0);
    const items = completionItems(completion);
    assertEqual(items.length, 1336, 'empty-document keyword catalog size');
    assertEqual(items[0] && items[0].label, 'askdim', 'empty-document first keyword');
    assertEqual(items[0] && items[0].kind, 14, 'empty-document first keyword kind');
    assertEqual(items[0] && items[0].data, 1, 'empty-document first keyword data');
    assert(items[0].insertText === undefined, 'askdim keyword has no insertText');
    assert(items.find((item) => item.label === 'if-structure') === undefined, 'snippets off by default');

    const pull = await session.diagnostic(uri);
    assertEqual(pull && pull.kind, 'full', 'empty-document pull kind');
    assertEqual(pull && pull.items, [], 'empty-document pull items');
    assertEqual(pull && pull.resultId, '1', 'empty-document pull resultId');
    assert(pull.relatedDocuments === undefined, 'empty-document pull has no relatedDocuments');

    await session.waitQuiet();
    assertEqual(session.diagnosticsFor(uri), [], 'empty-document diagnostics');

    await session.shutdownAndExit();
}

const SLOW_HEAVY_MARKER = ';!vertex-adt-ls:slow-heavy\n';
const CHEAP_BEHIND_HEAVY_MS = 150;

async function runCheapVsHeavyIsolation(session) {
    const uriA = pathToFileURL(path.join(ROOT, 'test-fixtures', '.golden-isolation-a.prg')).href;
    const uriB = pathToFileURL(path.join(ROOT, 'test-fixtures', '.golden-isolation-b.prg')).href;
    await session.initialize();

    session.didOpen({ uri: uriA, languageId: 'vertexadt', version: 1, text: SLOW_HEAVY_MARKER });
    session.didOpen({ uri: uriB, languageId: 'vertexadt', version: 1, text: '' });

    const stale = session.semanticTokensFullAllowError(uriA);
    const hoverStarted = Date.now();
    const hoverA = await session.hover(uriA, 0, 0, 1000);
    const hoverMs = Date.now() - hoverStarted;
    assert(hoverMs < CHEAP_BEHIND_HEAVY_MS, `cheap hover queued behind heavy (${hoverMs}ms)`);
    assertEqual(hoverA, null, 'hover during heavy on same URI');

    const hoverB = await session.hover(uriB, 0, 0, 1000);
    assertEqual(hoverB, null, 'hover on another URI during heavy');

    session.didChange({
        uri: uriA,
        version: 2,
        contentChanges: [{ text: 'x = 1\n' }],
    });

    const staleResult = await stale;
    assert(staleResult.error, 'newer version should drop that URI’s heavy result');
    assertEqual(staleResult.error.code, -32800, 'stale heavy RequestCancelled');

    const next = await session.semanticTokensFull(uriA);
    assertEqual(next && next.data, [], 'replacement tokens data');
    assertEqual(next.resultId, '2', 'replacement tokens are version 2');

    await session.waitQuiet();
    assertEqual(session.diagnosticsFor(uriA), [], 'no publishDiagnostics from heavy stub');

    await session.shutdownAndExit();
}

function hoverValue(result) {
    if (!result) {
        return null;
    }
    const contents = result.contents;
    if (!contents) {
        return null;
    }
    if (typeof contents === 'string') {
        return contents;
    }
    if (contents.value !== undefined) {
        return contents.value;
    }
    return null;
}

function completionItems(result) {
    if (!result) {
        return [];
    }
    if (Array.isArray(result)) {
        return result;
    }
    return result.items || [];
}

function wordOffset(line, word, occurrence = 1) {
    let from = 0;
    for (let i = 0; i < occurrence; i += 1) {
        const at = line.indexOf(word, from);
        if (at < 0) {
            throw new Error(`word ${JSON.stringify(word)} occurrence ${i + 1} not in ${JSON.stringify(line)}`);
        }
        if (i === occurrence - 1) {
            return at;
        }
        from = at + 1;
    }
    throw new Error(`word ${JSON.stringify(word)} not found`);
}

async function hoverDumpWord(session, uri, lineText, word, occurrence = 1) {
    const text = `START OF DEBUG in FILE\n${lineText}\n`;
    session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
    const hover = await session.hover(uri, 1, wordOffset(lineText, word, occurrence));
    session.didClose(uri);
    return hover;
}

async function runDebugThenAssignmentGoldens(session) {
    const fixtureDir = path.join(ROOT, 'test-fixtures', 'debug-then-assignment');
    await session.initialize();

    const sameLine = async (file, lineText, word, value, occurrence = 1) => {
        const uri = pathToFileURL(path.join(fixtureDir, file)).href;
        const hover = await hoverDumpWord(session, uri, lineText, word, occurrence);
        assertEqual(hoverValue(hover), `**${word}** = \`${value}\``, file);
    };
    const empty = async (file, lineText, word, occurrence = 1) => {
        const uri = pathToFileURL(path.join(fixtureDir, file)).href;
        const hover = await hoverDumpWord(session, uri, lineText, word, occurrence);
        assertEqual(hover, null, file);
    };

    await sameLine('standalone.prg', 'CTopZ = -1000000    <-1e+06>', 'CTopZ', '-1e+06');
    await sameLine('then-assignment.prg', 'if TZ > CTopZ then CTopZ = TZ    <5127.43>', 'CTopZ', '5127.43', 2);
    await empty('then-true.prg', 'if TZ > CTopZ then CTopZ = TZ    <true> _if_begin_', 'CTopZ', 2);
    await empty('then-false.prg', 'if TZ > CTopZ then CTopZ = TZ    <false>', 'CTopZ', 2);
    await sameLine(
        'then-last-value.prg',
        'if TZ > CTopZ then CTopZ = TZ    <true> _if_begin_ <5127.43>',
        'CTopZ',
        '5127.43',
        2
    );
    await sameLine('condition-equals.prg', 'if A = B then CC = D    <100>', 'CC', '100');
    await empty('else-without-then.prg', 'else CTopZ = TZ    <5127.43>', 'CTopZ');
    await sameLine('else-if-then.prg', 'else if TZ > CTopZ then CTopZ = TZ    <5127.43>', 'CTopZ', '5127.43', 2);
    await sameLine('boolean-standalone.prg', 'Flag = TRUE    <true>', 'Flag', 'true');
    await sameLine('if-case.prg', 'IF TZ > CTopZ THEN CTopZ = TZ    <5127.43>', 'CTopZ', '5127.43', 2);
    await sameLine('string-then.prg', 'if X then Name$ = "A"    <"A">', 'Name$', '"A"');
    await sameLine(
        'function-paren.prg',
        'vec_sub(NSP) SidPceSP,WP                     <-3300,254,2259.5>',
        'NSP',
        '-3300,254,2259.5'
    );
    await sameLine('nested-then.prg', 'if a then if b then XX = Y    <9>', 'XX', '9');

    const otherUri = pathToFileURL(path.join(fixtureDir, 'other-line.prg')).href;
    session.didOpen({
        uri: otherUri,
        languageId: 'vertexadt',
        version: 1,
        text: 'START OF DEBUG in FILE\nCTopZ = -1000000    <-1e+06>\nCTopZ\n',
    });
    const other = await session.hover(otherUri, 2, 0);
    assertEqual(
        hoverValue(other),
        '**CTopZ** = `-1e+06`\n\n_Set on line 2_',
        'other-line debug hover'
    );

    await session.shutdownAndExit();
}

const ASKDIM_DOCS = 'Example:\n\n```vertexadt\nDimension=askdim(Question$,Dimension)\n```\nExample2:\n\n```vertexadt\nDimension=askdim("header/question for user",default value)\n```';
const ASKDIM_DETAIL = 'Asks user via prompt for a dimension value, returns the value';
const ASKDIM_HOVER = `${ASKDIM_DOCS}\n\n---\n${ASKDIM_DETAIL}`;
const PNT_INSIDE_DOCS = 'Example:\n\n```vertexadt\nYes = pnt_inside_poly(Poly,P,D)\n```';
const PNT_INSIDE_DETAIL = 'checks if given point is inside the given polygon, Poly: polygon to checek, P: point to check, D: direction in which P is projected to plane of polygon (optional, normal of plane is used as default). Returns 1 if point is inside polygon';
const PNT_INSIDE_HOVER = `${PNT_INSIDE_DOCS}\n\n---\n${PNT_INSIDE_DETAIL}`;

function fenceHover(heading, name, comment) {
    return `${heading}:\n\`\`\`vertexadt \n${name}\n\`\`\`\n\n${comment}`;
}

function fenceCompletion(heading, name, comment) {
    return `${heading}:\n\`\`\`vertexadt\n${name}\n\`\`\`\n\n${comment}`;
}

function findItem(items, label) {
    return items.find((item) => item.label === label);
}

async function runCheapHoverCompletionGoldens(session) {
    const fixtureDir = path.join(ROOT, 'test-fixtures', 'cheap-hover-completion');
    const mainPath = path.join(fixtureDir, 'custom', 'main.prg');
    const uri = pathToFileURL(mainPath).href;
    const text = fs.readFileSync(mainPath, 'utf8');
    const lines = text.split(/\r?\n/);

    await session.initialize();
    session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });

    const hoverAskdim = await session.hover(uri, 3, wordOffset(lines[3], 'askdim'));
    assertEqual(hoverValue(hoverAskdim), ASKDIM_HOVER, 'keyword hover askdim');
    assertEqual(hoverAskdim && hoverAskdim.contents && hoverAskdim.contents.kind, 'markdown', 'keyword hover kind');

    const hoverFoo = await session.hover(uri, 0, wordOffset(lines[0], 'foo'));
    assertEqual(hoverValue(hoverFoo), fenceHover('Variable', 'foo', 'comment'), 'live-buffer variable hover');

    const hoverBar = await session.hover(uri, 1, wordOffset(lines[1], 'bar$'));
    assertEqual(hoverValue(hoverBar), fenceHover('String Variable', 'bar$', 's'), 'live-buffer string-variable hover');

    const hoverKorsregel = await session.hover(uri, 2, wordOffset(lines[2], 'korsregel'));
    const korsregelValue = hoverValue(hoverKorsregel);
    assert(
        typeof korsregelValue === 'string' && korsregelValue.startsWith(fenceHover('Variable', 'korsregel', 'korsregel layer')),
        `IGNORE TYPE strip: ${JSON.stringify(korsregelValue)}`
    );
    assert(!korsregelValue.includes('['), 'stripped [area] tag');
    assert(!/IGNORE TYPE/i.test(korsregelValue), 'stripped IGNORE TYPE');

    const hoverImage = await session.hover(uri, 4, wordOffset(lines[4], 'pnt_inside_poly'));
    const imageValue = hoverValue(hoverImage);
    assert(typeof imageValue === 'string' && imageValue.startsWith('![Description]('), 'hover image prefix');
    assert(imageValue.includes('ADT-documentation/'), 'hover image folder');
    assert(
        imageValue.includes('pnt_inside_poly%20explainer.png') || imageValue.includes('pnt_inside_poly explainer.png'),
        'hover image filename'
    );
    assert(imageValue.endsWith('\n\n' + PNT_INSIDE_HOVER) || imageValue.includes(PNT_INSIDE_HOVER), 'hover image keyword body');

    const plainUri = pathToFileURL(path.join(fixtureDir, 'custom', 'plain.log')).href;
    session.didOpen({ uri: plainUri, languageId: 'plaintext', version: 1, text });
    assertEqual(await session.hover(plainUri, 0, 0), null, 'plaintext hover');
    assertEqual(await session.completion(plainUri, 0, 0), null, 'plaintext completion');

    await session.waitQuiet();
    const hoverBaz = await session.hover(uri, 5, 0);
    assertEqual(hoverBaz, null, 'no word on include line');
    const completionAfter = completionItems(await session.completion(uri, 0, 0));
    const fooItem = findItem(completionAfter, 'foo');
    const barItem = findItem(completionAfter, 'bar$');
    const bazItem = findItem(completionAfter, 'baz');
    const quxItem = findItem(completionAfter, 'qux$');
    assert(fooItem && fooItem.kind === 6, 'foo completion kind Variable');
    assertEqual(
        fooItem && fooItem.documentation && fooItem.documentation.value,
        fenceCompletion('Variable', 'foo', 'comment'),
        'foo completion docs (live comment wins)'
    );
    assert(barItem && barItem.kind === 1, 'bar$ completion kind Text');
    assertEqual(
        barItem && barItem.documentation && barItem.documentation.value,
        fenceCompletion('String Variable', 'bar$', 's'),
        'bar$ completion docs'
    );
    assert(bazItem && bazItem.kind === 6, 'included baz completion');
    assertEqual(
        bazItem && bazItem.documentation && bazItem.documentation.value,
        fenceCompletion('Variable', 'baz', 'from include'),
        'included baz docs'
    );
    assert(quxItem && quxItem.kind === 1, 'included qux$ completion');
    assert(quxItem && quxItem.documentation === undefined, 'qux$ has no documentation');
    assert(findItem(completionAfter, 'if-structure') === undefined, 'no structure snippets by default');
    assert(findItem(completionAfter, 'askdim (snippet)') === undefined, 'no keyword snippets by default');

    const hoverIncluded = await session.hover(
        uri,
        0,
        wordOffset(lines[0], 'foo')
    );
    assertEqual(hoverValue(hoverIncluded), fenceHover('Variable', 'foo', 'comment'), 'hover still live comment after include');

    const openInc = pathToFileURL(path.join(fixtureDir, 'custom', 'macros', 'inc.prg')).href;
    session.didOpen({
        uri: openInc,
        languageId: 'vertexadt',
        version: 1,
        text: fs.readFileSync(path.join(fixtureDir, 'custom', 'macros', 'inc.prg'), 'utf8'),
    });
    await session.waitQuiet();
    const hoverBazName = await session.hover(openInc, 1, wordOffset('baz = 3 ;!"from include"', 'baz'));
    assertEqual(hoverValue(hoverBazName), fenceHover('Variable', 'baz', 'from include'), 'included file hover');

    const resolved = await session.completionResolve(findItem(completionAfter, 'askdim'));
    assertEqual(
        resolved && resolved.documentation && resolved.documentation.value,
        ASKDIM_HOVER,
        'completionItem/resolve askdim'
    );
    assertEqual(resolved && resolved.documentation && resolved.documentation.kind, 'markdown', 'resolve kind');

    session.didChangeConfiguration({
        snippets: { useExpandedSyntax: true },
    });
    const expanded = completionItems(await session.completion(uri, 0, 0));
    const askdimIndex = expanded.findIndex((item) => item.label === 'askdim');
    assert(askdimIndex >= 0, 'askdim still present with snippets on');
    assertEqual(expanded[askdimIndex + 1] && expanded[askdimIndex + 1].label, 'askdim (snippet)', 'snippet duplicate after keyword');
    assertEqual(expanded[askdimIndex + 1] && expanded[askdimIndex + 1].kind, 15, 'snippet kind');
    assertEqual(
        expanded[askdimIndex + 1] && expanded[askdimIndex + 1].insertText,
        'askdim(${1:question},${2:suggestion})',
        'askdim snippet insertText'
    );
    const ifStructure = findItem(expanded, 'if-structure');
    const loopStructure = findItem(expanded, 'loop-structure');
    assert(ifStructure && ifStructure.kind === 15, 'if-structure present');
    assertEqual(ifStructure.insertText, 'if ${1} then\n\t${2}\nendif', 'if-structure insertText');
    assertEqual(loopStructure && loopStructure.insertText, 'loop ${1} \n\t${2}\nendloop', 'loop-structure insertText');
    const firstVariable = expanded.findIndex((item) => item.kind === 6);
    const ifIndex = expanded.findIndex((item) => item.label === 'if-structure');
    assert(ifIndex >= 0 && firstVariable > ifIndex, 'structure snippets before variables');

    const dbcreate = findItem(expanded, 'dbcreate');
    assert(typeof dbcreate.insertText === 'string' && dbcreate.insertText.includes('\n'), 'dbcreate keyword multiline insertText');
    assertEqual(dbcreate.insertTextFormat, 1, 'dbcreate insertTextFormat PlainText');

    await session.shutdownAndExit();
}

function readFixture(dir, name) {
    return fs.readFileSync(path.join(dir, name), 'utf8');
}

async function runFullDocumentValidateCompositionGoldens(session) {
    const fixtureDir = path.join(ROOT, 'test-fixtures', 'vxdbase-winstyle-shell');
    await session.initialize();

    const shellUri = pathToFileURL(path.join(fixtureDir, 'shell_no_sections.prg')).href;
    session.didOpen({
        uri: shellUri,
        languageId: 'vertexadt',
        version: 1,
        text: readFixture(fixtureDir, 'shell_no_sections.prg'),
    });
    const shellPull = await session.diagnostic(shellUri);
    assertEqual(shellPull && shellPull.kind, 'full', 'vxdbase shell pull kind');
    assertEqual(shellPull && shellPull.items, [], 'vxdbase shell excluded items');
    assert(shellPull.relatedDocuments === undefined, 'vxdbase shell has no relatedDocuments');

    const sectionedUri = pathToFileURL(path.join(fixtureDir, 'with_sections.prg')).href;
    session.didOpen({
        uri: sectionedUri,
        languageId: 'vertexadt',
        version: 1,
        text: readFixture(fixtureDir, 'with_sections.prg'),
    });
    const sectionedPull = await session.diagnostic(sectionedUri);
    assertEqual(sectionedPull && sectionedPull.kind, 'full', 'sectioned winstyle pull kind');
    assert(
        Array.isArray(sectionedPull.items) && sectionedPull.items.length > 0,
        'sectioned winstyle still validates'
    );

    const dumpUri = pathToFileURL(path.join(ROOT, 'test-fixtures', '.golden-debug-dump.prg')).href;
    session.didOpen({
        uri: dumpUri,
        languageId: 'vertexadt',
        version: 1,
        text: 'START OF DEBUG in FILE\nX = 1 and Y or Z\n',
    });
    const dumpPull = await session.diagnostic(dumpUri);
    assertEqual(dumpPull && dumpPull.kind, 'full', 'debug dump pull kind');
    assertEqual(dumpPull && dumpPull.items, [], 'debug dump excluded items');

    const cacheUri = pathToFileURL(path.join(ROOT, 'test-fixtures', '.golden-validate-cache.prg')).href;
    session.didOpen({ uri: cacheUri, languageId: 'vertexadt', version: 1, text: 'x = 1\n' });
    const first = await session.diagnostic(cacheUri);
    assertEqual(first && first.kind, 'full', 'first pull kind');
    assertEqual(first && first.resultId, '1', 'first pull resultId');
    const cached = await session.diagnostic(cacheUri, DEFAULT_TIMEOUT_MS, first.resultId);
    assertEqual(cached && cached.kind, 'unchanged', 'URI+version cache Unchanged');
    assertEqual(cached && cached.resultId, '1', 'Unchanged resultId');
    assert(cached.items === undefined, 'Unchanged has no items');
    assert(cached.relatedDocuments === undefined, 'Unchanged has no relatedDocuments');

    session.didChange({
        uri: cacheUri,
        version: 2,
        contentChanges: [{ text: 'x = 2\n' }],
    });
    const afterEdit = await session.diagnostic(cacheUri, DEFAULT_TIMEOUT_MS, '1');
    assertEqual(afterEdit && afterEdit.kind, 'full', 'didChange invalidates cache');
    assertEqual(afterEdit && afterEdit.resultId, '2', 'edited pull resultId');

    const cancelUri = pathToFileURL(path.join(ROOT, 'test-fixtures', '.golden-validate-cancel.prg')).href;
    session.didOpen({
        uri: cancelUri,
        languageId: 'vertexadt',
        version: 1,
        text: SLOW_HEAVY_MARKER,
    });
    const stale = session.diagnosticAllowError(cancelUri);
    session.didChange({
        uri: cancelUri,
        version: 2,
        contentChanges: [{ text: 'x = 1\n' }],
    });
    const staleResult = await stale;
    assert(staleResult.error, 'didChange should drop that URI’s heavy diagnostic');
    assertEqual(staleResult.error.code, -32800, 'stale diagnostic RequestCancelled');

    const next = await session.diagnostic(cancelUri);
    assertEqual(next && next.kind, 'full', 'replacement diagnostic kind');
    assertEqual(next && next.resultId, '2', 'replacement diagnostic version');

    await session.shutdownAndExit();
}

const FAMILY_GOLDENS = [
    '13-type-timeline',
    '14-if-loop-indent',
    '15-command-syntax',
    '16-used-before',
    '17-unused-variables',
    '18-call-param',
    '19-unknown-section',
    '20-unclosed-resources',
    '21-in-file-decl',
    '22-local-slots',
    '23-callee-common',
    '24-cross-file',
    '25-joint-section',
    '28-definition',
    '29-references',
    '30-semantic-tokens',
    '31-document-links',
    '32-include-call-completion',
    '33-dbcreate-field-completion',
    '34-debug-filename-completion',
    '35-newline-cleanup',
    '36-remaining-param-completion',
    '37-signature-help',
    '38-call-return-value-completion',
    '39-form-name-completion',
    '41-object-parameter-completion',
    '42-macro-library-code-completion',
    '43-ignore-close-code-action',
    '44-call-param-code-actions',
    '45-local-slot-rename-and-assign',
    '46-impact-codelens',
    '47-workspace-csv-exports',
    '48-slot-substring-completion',
    '49-dir-path-hover',
    '50-concat-whitespace',
    '51-substring-index',
    '52-bracket-balance',
    '53-trigger-character-family-only',
    '54-macro-code-first-completion',
    '55-cross-call-types',
    '56-control-block-pairing',
    '57-findstr-value-expression',
    '58-indentation-formatter',
    '59-if-header-continuation',
    '60-inferred-included-from',
    '61-program-move',
];

async function runFamilyGoldens() {
    for (const name of FAMILY_GOLDENS) {
        const family = require(`./goldens/${name}`);
        if (typeof family.run !== 'function') {
            throw new Error(`scripts/goldens/${name}.js must export run()`);
        }
        await family.run();
        console.log(`lsp golden harness: ${name} passed`);
    }
}

async function withSession(fn, launchOptions) {
    const session = LspSession.launch(launchOptions);
    try {
        await fn(session);
    } catch (err) {
        session.kill();
        throw err;
    }
}

async function main() {
    await withSession(runEmptyDocumentSmoke);
    console.log('lsp golden harness: initialize/shutdown + empty-document smoke passed');
    await withSession(runCheapVsHeavyIsolation);
    console.log('lsp golden harness: cheap-vs-heavy isolation + stale-URI cancel passed');
    await withSession(runDebugThenAssignmentGoldens);
    console.log('lsp golden harness: debug-then-assignment hover passed');
    const cheapRoot = path.join(ROOT, 'test-fixtures', 'cheap-hover-completion');
    const cheapRootUri = pathToFileURL(cheapRoot).href;
    await withSession(runCheapHoverCompletionGoldens, {
        rootUri: cheapRootUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: cheapRootUri, name: 'cheap-hover-completion' }],
    });
    console.log('lsp golden harness: cheap hover and keyword-variable completion passed');
    await withSession(runFullDocumentValidateCompositionGoldens);
    console.log('lsp golden harness: full-document validate composition passed');
    await runFamilyGoldens();
}

module.exports = {
    LANGUAGE_SERVER_EXECUTABLE,
    LspSession,
    runEmptyDocumentSmoke,
    runCheapVsHeavyIsolation,
    runDebugThenAssignmentGoldens,
    runCheapHoverCompletionGoldens,
    runFullDocumentValidateCompositionGoldens,
    runFamilyGoldens,
};

if (require.main === module) {
    main().catch((err) => {
        console.error(err.stack || err.message || err);
        process.exit(1);
    });
}
