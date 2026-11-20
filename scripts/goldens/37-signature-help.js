'use strict';

/**
 * Parity goldens for signature help.
 * Assertions from test-fixtures/signature-help/TESTING.md.
 *
 * Oracle: server.js's winning onSignatureHelp (getCommandContext + the label
 * assembler) with the spec deltas from
 * scratch/cpp-language-server-spec/issues/54-specify-signature-help.md:
 * lowercase command-catalog lookup, labels from catalog structure (min prefix
 * when a max is unbounded, never keyword-catalog insertText), and
 * parenthesized `call(` labelled `path` plus the callee's buffer-wins
 * remaining-param extract.
 */

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');
const targetSectionSupport = require('../../server/out/targetSectionSupport');
const COMMANDS = require('../../server/catalog/commands.json');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'signature-help');
const CUSTOM = path.join(FIXTURE, 'custom');

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

function isCatalogCommand(name) {
    return Object.prototype.hasOwnProperty.call(COMMANDS, name.toLowerCase());
}

// server.js getCommandContext with a lowercase lookup and `call(` admitted.
function commandContext(linePrefix) {
    const regex = /([A-Za-z][A-Za-z0-9_]*)\s*\(/g;
    let match;
    let last = null;
    while ((match = regex.exec(linePrefix)) !== null) {
        last = { name: match[1], openParenIndex: regex.lastIndex - 1 };
    }
    if (last && (last.name.toLowerCase() === 'call' || isCatalogCommand(last.name))) {
        let depth = 1;
        let inString = null;
        let insideCommas = 0;
        let afterCommas = 0;
        let closed = false;
        for (let i = last.openParenIndex + 1; i < linePrefix.length; i++) {
            const ch = linePrefix[i];
            if (inString) {
                if (ch === inString) {
                    inString = null;
                }
                continue;
            }
            if (ch === '"') {
                inString = ch;
                continue;
            }
            if (ch === '(') {
                depth++;
                continue;
            }
            if (ch === ')') {
                depth--;
                if (depth === 0) {
                    closed = true;
                }
                continue;
            }
            if (ch === ',') {
                if (!closed) {
                    if (depth === 1) {
                        insideCommas++;
                    }
                } else {
                    afterCommas++;
                }
            }
        }
        return {
            name: last.name,
            openParenIndex: last.openParenIndex,
            hadParens: true,
            inParens: !closed,
            insideCommas,
            afterCommas,
        };
    }
    const noParenMatch = linePrefix.match(/(?:^|\s)([A-Za-z][A-Za-z0-9_]*)\s+([^()]*)$/);
    if (!noParenMatch || !isCatalogCommand(noParenMatch[1])) {
        return null;
    }
    return {
        name: noParenMatch[1],
        hadParens: false,
        inParens: false,
        insideCommas: 0,
        afterCommas: (noParenMatch[2].match(/,/g) || []).length,
    };
}

function finite(value) {
    return typeof value === 'number' && Number.isFinite(value) ? value : undefined;
}

// Structure labels: finite max, else min / paramsOutside (unbounded is the finite prefix).
function catalogLabels(name) {
    const entry = COMMANDS[name.toLowerCase()];
    const structure = entry.structure || {};
    const strict = entry.strictMode || {};
    const inside = finite(structure.maxParamsInside) ?? finite(structure.minParamsInside) ?? 0;
    const after = finite(structure.maxParamsOutside) ?? finite(structure.paramsOutside) ?? 0;
    const label = (slots, i) => {
        const hit = (slots || []).find((slot) => slot.paramIndex === i);
        return hit && hit.paramName ? hit.paramName : `param${i + 1}`;
    };
    return {
        insideParams: Array.from({ length: inside }, (_, i) => label(strict.inputTypes, i)),
        afterParams: Array.from({ length: after }, (_, i) => label(strict.afterParamTypes, i)),
    };
}

function calleeText(filePath, buffers) {
    const key = path.basename(filePath).toLowerCase();
    if (buffers.has(key)) {
        return buffers.get(key);
    }
    const diskPath = path.join(CUSTOM, filePath);
    return fs.existsSync(diskPath) && fs.statSync(diskPath).isFile()
        ? fs.readFileSync(diskPath, 'utf8')
        : null;
}

function callLabels(linePrefix, openParenIndex, buffers) {
    const rest = linePrefix.slice(openParenIndex + 1);
    const match = rest.match(/^\s*((?:"[^"]+"|'[^']+'|[^,)]+))?/);
    const raw = match && match[1] !== undefined ? match[1].trim().replace(/^["']|["']$/g, '') : '';
    const parsed = targetSectionSupport.parseIncludeCallPathAndSection(raw, '');
    const insideParams = ['path'];
    if (parsed.filePath) {
        const text = calleeText(parsed.filePath, buffers);
        if (text) {
            for (const row of targetSectionSupport.extractCallParametersFromText(text, parsed.targetSection)) {
                insideParams.push(row.variableName);
            }
        }
    }
    return { insideParams, afterParams: [] };
}

function oracleHelp(linePrefix, buffers) {
    const context = commandContext(linePrefix);
    if (!context) {
        return null;
    }
    const { insideParams, afterParams } = context.name.toLowerCase() === 'call' && context.hadParens
        ? callLabels(linePrefix, context.openParenIndex, buffers)
        : catalogLabels(context.name);
    if (!insideParams.length && !afterParams.length) {
        return null;
    }
    if (!context.inParens && context.hadParens && afterParams.length === 0) {
        return null;
    }
    const effectiveAfter = context.inParens ? afterParams : (afterParams.length ? afterParams : insideParams);
    const all = context.inParens ? [...insideParams, ...afterParams] : [...effectiveAfter];
    let active;
    if (context.inParens) {
        if (context.insideCommas >= insideParams.length) {
            return null;
        }
        active = context.insideCommas;
    } else {
        if (context.afterCommas >= effectiveAfter.length) {
            return null;
        }
        active = context.afterCommas;
    }
    const label = context.inParens
        ? `${context.name}(${insideParams.join(', ')})${afterParams.length ? ' ' + afterParams.join(', ') : ''}`
        : `${context.name} ${effectiveAfter.join(', ')}`;
    return {
        signatures: [{ label, parameters: all.map((name) => ({ label: name })) }],
        activeSignature: 0,
        activeParameter: active,
    };
}

class Probe {
    constructor(session, fileName) {
        this.session = session;
        this.uri = pathToFileURL(path.join(CUSTOM, fileName)).href;
        this.version = 0;
    }

    async help(text, line, character, context) {
        this.version += 1;
        if (this.version === 1) {
            this.session.didOpen({ uri: this.uri, languageId: 'vertexadt', version: 1, text });
        } else {
            this.session.didChange({ uri: this.uri, version: this.version, contentChanges: [{ text }] });
        }
        const params = { textDocument: { uri: this.uri }, position: { line, character } };
        if (context) {
            params.context = context;
        }
        return this.session.request('textDocument/signatureHelp', params);
    }

    // `|` marks the cursor; without one the cursor is at the end of the last line.
    async at(textWithCursor, context) {
        const lines = textWithCursor.split('\n');
        let line = lines.findIndex((row) => row.includes('|'));
        let character;
        if (line >= 0) {
            character = lines[line].indexOf('|');
            lines[line] = lines[line].replace('|', '');
        } else {
            line = lines.length - 1;
            character = lines[line].replace(/\r$/, '').length;
        }
        const result = await this.help(lines.join('\n'), line, character, context);
        return { result, prefix: lines[line].replace(/\r$/, '').substring(0, character) };
    }
}

function summary(help) {
    return help ? [help.signatures[0].label, help.activeParameter] : null;
}

// Native help equals the oracle; optional `expected` pins [label, activeParameter] or null.
async function check(probe, buffers, text, name, expected, context) {
    const { result, prefix } = await probe.at(text, context);
    assertEqual(result, oracleHelp(prefix, buffers), `${name}: matches the oracle`);
    if (expected !== undefined) {
        assertEqual(summary(result), expected, `${name}: label and active parameter`);
    }
    return result;
}

const SR_OPEN = 'set_regenerable(input object) regenerable value';
const SR_AFTER = 'set_regenerable regenerable value';
const THREE = 'callee_three_params.prg';
const THREE_LABEL = (name = 'call') => `${name}(path, obj, Panel, iswall)`;

const ROWS = [
    // Active command and comma walk.
    ['set_regenerable(', [SR_OPEN, 0]],
    ['set_regenerable (', [SR_OPEN, 0]],
    ['Ret$=set_regenerable(', [SR_OPEN, 0]],
    ['    set_regenerable(', [SR_OPEN, 0]],
    ['x = 1; set_regenerable(', [SR_OPEN, 0]],
    ['!set_regenerable(', [SR_OPEN, 0]],
    ['SET_REGENERABLE(', ['SET_REGENERABLE(input object) regenerable value', 0]],
    ['set_regenerable(Sheet,', null],
    ['set_regenerable("a,b"', [SR_OPEN, 0]],
    ["set_regenerable('a,b'", null],
    ['set_regenerable(Sheet)', [SR_AFTER, 0]],
    ['set_regenerable(Sheet) 0,', null],
    ['set_regenerable(map_size(', ['map_size(map name)', 0]],
    ['set_regenerable(foo(', null],
    ['map_size("n"', ['map_size(map name)', 0]],
    ['map_size("n")', null],
    ['map_size("n", extra', null],
    ['ask("q",', ['ask(param1, param2)', 1]],
    ['recall(', null],
    ['call_virtual("x.prg",', null],
    // No-paren form and inside-only parens.
    ['cube 1, 2', ['cube length x, length y, length z', 1]],
    ['cube 1, 2, 3,', null],
    ['x = cube 1, 2', null],
    ['set a cube 1', undefined],
    ['cube(', null],
    ['cube() a, b', ['cube length x, length y, length z', 1]],
    // Unbounded maxima emit the finite prefix.
    ['add_aux_task(', ['add_aux_task(input object) param1', 0]],
    ['add_aux_task(o) a', ['add_aux_task param1', 0]],
    ['add_aux_task(o) a,', null],
    ['set_sheet_shape(s) a, b,', ['set_sheet_shape param1, param2, param3', 2]],
    ['set_sheet_shape(s) a, b, c,', null],
    ['f = new_feature(a, b,', ['new_feature(param1, param2, param3)', 2]],
    ['f = new_feature(a, b, c,', null],
    // Parenthesized `call(`.
    ['call(', ['call(path)', 0]],
    [`call("${THREE}"`, [THREE_LABEL(), 0]],
    [`call("${THREE}",`, [THREE_LABEL(), 1]],
    [`call("${THREE}", Area`, [THREE_LABEL(), 1]],
    [`call("${THREE}", a, b,`, [THREE_LABEL(), 3]],
    [`call("${THREE}", a, b, c,`, null],
    [`call("${THREE}", a, b)`, null],
    [`CALL("${THREE}",`, [THREE_LABEL('CALL'), 1]],
    [`call ("${THREE}",`, [THREE_LABEL(), 1]],
    [`Ret$=call("${THREE}",`, [THREE_LABEL(), 1]],
    [`x = 1; call("${THREE}",`, [THREE_LABEL(), 1]],
    [`!call("${THREE}",`, [THREE_LABEL(), 1]],
    [`call(${THREE},`, [THREE_LABEL(), 1]],
    [`call('${THREE}',`, [THREE_LABEL(), 1]],
    ['call("file,name.prg",', [THREE_LABEL(), 1]],
    [`call("${THREE}", "a,b",`, [THREE_LABEL(), 2]],
    ['call("sectional_callee.prg MACROS",', ['call(path, mac)', 1]],
    ['call("sectional_callee.prg", MACROS,', ['call(path, mac, form, whole)', 2]],
    ['call("sectional_callee.prg SECTION",', ['call(path, mac, form, whole)', 1]],
    ['call("crlf_callee.prg",', ['call(path, Name$, count)', 1]],
    ['call("missing.prg"', ['call(path)', 0]],
    ['call("missing.prg",', null],
    ['call("empty_callee.prg",', null],
    [`call ${THREE},`, null],
    [`my_call("${THREE}",`, null],
    [`set_regenerable(call("${THREE}",`, [THREE_LABEL(), 1]],
    [`call("${THREE}", map_size(`, ['map_size(map name)', 0]],
];

async function run(session) {
    const init = await session.initialize();
    assertEqual(
        init.capabilities.signatureHelpProvider,
        { triggerCharacters: ['(', ',', ' '], retriggerCharacters: [',', ' '] },
        'signature help triggers and retriggers'
    );
    await session.waitQuiet();

    const buffers = new Map();
    const probe = new Probe(session, 'caller.prg');

    for (const [prefix, expected] of ROWS) {
        await check(probe, buffers, prefix, prefix, expected);
    }

    // Payload shape: one signature, parameters are `{label}` only, inside then after while in parens.
    let help = await check(probe, buffers, 'set_regenerable(', 'payload in parens');
    assertEqual(help, {
        signatures: [{
            label: SR_OPEN,
            parameters: [{ label: 'input object' }, { label: 'regenerable value' }],
        }],
        activeSignature: 0,
        activeParameter: 0,
    }, 'payload in parens');
    help = await check(probe, buffers, 'set_regenerable(Sheet)', 'payload after paren');
    assertEqual(help.signatures[0].parameters, [{ label: 'regenerable value' }], 'after-paren parameters only');

    // Every catalog command, in several shapes, against the structure oracle.
    for (const name of Object.keys(COMMANDS)) {
        for (const prefix of [`${name}(`, `${name}(a,`, `${name}(a)`, `${name}(a) b,`, `${name} a, b`]) {
            await check(probe, buffers, prefix, `catalog sweep ${prefix}`);
        }
    }

    // Current line only; mid-line cursor; CRLF caller; context is ignored.
    await check(probe, buffers, 'set_regenerable(\nmap_size(', 'previous line ignored', ['map_size(map name)', 0]);
    await check(probe, buffers, `call("${THREE}", |Area, b)`, 'mid-line cursor', [THREE_LABEL(), 1]);
    await check(probe, buffers, `x = 1\r\ncall("${THREE}",|\r\ny = 2`, 'CRLF caller', [THREE_LABEL(), 1]);
    await check(probe, buffers, `call("${THREE}",`, 'retrigger context', [THREE_LABEL(), 1], {
        triggerKind: 3,
        isRetrigger: true,
        activeSignatureHelp: { signatures: [{ label: 'stale' }], activeSignature: 0, activeParameter: 9 },
    });

    // Buffer wins over disk for the callee, and disk returns after close.
    const calleeUri = pathToFileURL(path.join(CUSTOM, 'buffer_wins_callee.prg')).href;
    await check(probe, buffers, 'call("buffer_wins_callee.prg",', 'disk callee', ['call(path, disk1, disk2)', 1]);
    const bufferText = 'buf1 = get($CALLPARAM1)\nbuf2$ = get($CALLPARAM2)\nbuf3 = get($CALLPARAM3)\n';
    session.didOpen({ uri: calleeUri, languageId: 'vertexadt', version: 1, text: bufferText });
    buffers.set('buffer_wins_callee.prg', bufferText);
    await check(probe, buffers, 'call("buffer_wins_callee.prg",', 'buffer callee', ['call(path, buf1, buf2$, buf3)', 1]);
    const edited = 'only = get($CALLPARAM1)\n';
    session.didChange({ uri: calleeUri, version: 2, contentChanges: [{ text: edited }] });
    buffers.set('buffer_wins_callee.prg', edited);
    await check(probe, buffers, 'call("buffer_wins_callee.prg",', 'edited buffer callee', ['call(path, only)', 1]);
    session.didClose(calleeUri);
    buffers.delete('buffer_wins_callee.prg');
    await check(probe, buffers, 'call("buffer_wins_callee.prg",', 'closed callee reads disk', ['call(path, disk1, disk2)', 1]);

    // Non-vertexadt and unknown documents are null.
    const plainUri = pathToFileURL(path.join(CUSTOM, 'other.txt')).href;
    session.didOpen({ uri: plainUri, languageId: 'plaintext', version: 1, text: 'set_regenerable(' });
    assertEqual(
        await session.request('textDocument/signatureHelp', {
            textDocument: { uri: plainUri },
            position: { line: 0, character: 16 },
        }),
        null,
        'plaintext is null'
    );
    assertEqual(
        await session.request('textDocument/signatureHelp', {
            textDocument: { uri: pathToFileURL(path.join(CUSTOM, 'never_opened.prg')).href },
            position: { line: 0, character: 0 },
        }),
        null,
        'missing document is null'
    );

    await session.shutdownAndExit();
}

async function main() {
    const fixtureUri = pathToFileURL(FIXTURE).href;
    const session = LspSession.launch({
        rootUri: fixtureUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: fixtureUri, name: 'signature-help' }],
    });
    try {
        await run(session);
    } catch (err) {
        session.kill();
        throw err;
    }
}

if (require.main === module) {
    main()
        .then(() => {
            console.log('lsp golden: 37-signature-help passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main };
