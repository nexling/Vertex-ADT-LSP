'use strict';

/**
 * Server half of docs/adr/0021-native-slot-popup-reopens.md: path segments
 * (`.`, `-`, mid-path edits), hyphenated sections and the quiet slot check
 * (vertexadt/slotCompletionProbe). Run alone or via lsp-golden-harness.js.
 */

const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');
const { familyItems } = require('../completion-ranking');

const ROOT = path.resolve(__dirname, '../..');
const INVOKED = { triggerKind: 1 };
const TYPED = { triggerKind: 2 };
const INCOMPLETE = { triggerKind: 3 };

class Probe {
    constructor(session, uri) {
        this.session = session;
        this.uri = uri;
        this.version = 0;
    }

    // `|` marks the cursor.
    async send(method, marked, context) {
        const character = marked.indexOf('|');
        const line = marked.replace('|', '');
        this.version += 1;
        if (this.version === 1) {
            this.session.didOpen({ uri: this.uri, languageId: 'vertexadt', version: 1, text: line });
        } else {
            this.session.didChange({ uri: this.uri, version: this.version, contentChanges: [{ text: line }] });
        }
        return this.session.request(method, {
            textDocument: { uri: this.uri },
            position: { line: 0, character },
            context,
        });
    }

    // Items as `label => line after accepting`, sorted.
    async items(marked, context = TYPED) {
        const result = await this.send('textDocument/completion', marked, context);
        const line = marked.replace('|', '');
        return familyItems(result, 'includeCall', marked).map((item) => {
            const { range, newText } = item.textEdit;
            return `${item.label} => ${line.slice(0, range.start.character)}${newText}${line.slice(range.end.character)}`;
        }).sort();
    }

    async opens(marked) {
        const result = await this.send('vertexadt/slotCompletionProbe', marked, INCOMPLETE);
        return Boolean(result && result.items.length > 0);
    }
}

async function run(session, customFolder) {
    await session.initialize();
    const p = new Probe(session, pathToFileURL(path.join(customFolder, 'probe.prg')).href);
    const eq = async (marked, expected, context) =>
        assert.deepStrictEqual(await p.items(marked, context), expected, marked);

    // No cached listing needed: a wrong letter deleted, then a fresh request.
    await eq('call(macros/secfx|)', []);
    await eq('call(macros/secf|)', ['Secframing/ => call(macros/Secframing/)']);
    await eq('call(macros/|)', [
        'Secframing/ => call(macros/Secframing/)',
        'foo-bar.prg => call(macros/foo-bar.prg)',
        'foo.prg => call(macros/foo.prg)',
    ]);

    // `.` and `-` are typed text; the whole segment is replaced.
    await eq('call("macros/foo.p|")', ['foo.prg => call("macros/foo.prg")']);
    await eq('# include macros/foo-|', ['foo-bar.prg => # include macros/foo-bar.prg']);
    await eq('# include macros/fo|o.prg TOP-SIDE', [
        'foo-bar.prg => # include macros/foo-bar.prg TOP-SIDE',
        'foo.prg => # include macros/foo.prg TOP-SIDE',
    ]);
    await eq('##included_from macros/foo.p|', ['foo.prg => ##included_from macros/foo.prg']);
    await eq('call macros\\foo.p|', ['foo.prg => call macros\\foo.prg']);
    await eq('call("macros/foo.p|,x)', ['foo.prg => call("macros/foo.prg,x)']);

    // An earlier segment: the rest stays, no doubled slash.
    await eq('call($DIR_MACR|/sec)', ['$DIR_MACROS => call($DIR_MACROS/sec)']);
    await eq('call(macros/sec|/in)', ['Secframing/ => call(macros/Secframing/in)']);
    await eq('call(macrs/sec|)', []);

    // Hyphenated sections; a space still ends the path.
    await eq('# include macros/foo.prg TOP-|', ['TOP-LEFT => # include macros/foo.prg TOP-LEFT', 'TOP-SIDE => # include macros/foo.prg TOP-SIDE']);
    await eq('call("macros/foo.prg TOP-S|IDE")', ['TOP-SIDE => call("macros/foo.prg TOP-SIDE")']);

    // Reused listing: the end of the replace range follows the text.
    await eq('# include macros/|.prg TOP-SIDE', [
        'Secframing/ => # include macros/Secframing/ TOP-SIDE',
        'foo-bar.prg => # include macros/foo-bar.prg TOP-SIDE',
        'foo.prg => # include macros/foo.prg TOP-SIDE',
    ]);
    const reused = ['foo-bar.prg => # include macros/foo-bar.prg TOP-SIDE', 'foo.prg => # include macros/foo.prg TOP-SIDE'];
    await eq('# include macros/foo|.prg TOP-SIDE', reused, INCOMPLETE);
    await eq('# include macros/f|.prg TOP-SIDE', reused.concat('Secframing/ => # include macros/Secframing/ TOP-SIDE').sort(), INCOMPLETE);
    await eq('# include macros/fo|.prg TOP-SIDE', reused, INCOMPLETE);

    // Ctrl+Space sections aren't reused for the path.
    assert.strictEqual((await p.items('# include macros/foo.prg|', INVOKED)).length, 2, 'sections on Ctrl+Space');
    await eq('# include macros/foo.pr|', ['foo.prg => # include macros/foo.prg'], INCOMPLETE);

    // Quiet check.
    for (const [marked, expected] of [
        ['call(macros/secfx|)', false],
        ['call(macros/secf|)', true],
        ['call(macros/|)', true],
        ['call(macros/foo.p|)', true],
        ['call(macros/foo.prg|)', false], // only match, exactly typed
        ['call(macros/FOO.prg|)', true], // case differs
        ['call($DIR_MACR|/sec)', true],
        ['call(macros/foo.prg TOP-|)', true],
        ['call(macros/foo.prg TOP-SIDE|)', false],
        ['call(|)', true],
        ['dbedit(1,"frm|")', true],
        ['dbedit(1,"zzz|")', false],
        ['x = as|', false], // keywords never count
        ['x = 1|', false],
    ]) {
        assert.strictEqual(await p.opens(marked), expected, `probe ${marked}`);
    }
    // Call return values: the whole next line is the site, so never.
    const uri = pathToFileURL(path.join(customFolder, 'ret.prg')).href;
    session.didOpen({ uri, languageId: 'vertexadt', version: 1, text: 'Ret$ = call(callee/ret.prg)\nx' });
    const at = { textDocument: { uri }, position: { line: 1, character: 0 }, context: INCOMPLETE };
    const listed = await session.request('textDocument/completion', at);
    assert(familyItems(listed, 'callReturnValue', 'return values').length > 0, 'the site has return values');
    const quiet = await session.request('vertexadt/slotCompletionProbe', at);
    assert.deepStrictEqual(quiet.items, [], 'return values never reopen');
    await session.shutdownAndExit();
}

async function main() {
    const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'vertex-adt-slot-reopen-'));
    const custom = path.join(temp, 'custom');
    const write = (rel, text) => {
        const file = path.join(custom, rel);
        fs.mkdirSync(path.dirname(file), { recursive: true });
        fs.writeFileSync(file, text);
    };
    write('macros/foo.prg', '#TOP-SIDE#\nq = 1\n#TOP-SIDE#\n#TOP-LEFT#\nq = 1\n#TOP-LEFT#\n');
    write('macros/foo-bar.prg', 'q = 1\n');
    write('callee/ret.prg', 'Ret$ = "W=" & W & "|H=" & H\nexit Ret$\n');
    write('macros/Secframing/in.prg', 'q = 1\n');
    write('forms/frm_a.vxf', '');
    fs.mkdirSync(path.join(temp, 'system', 'forms'), { recursive: true });
    const session = LspSession.launch({
        rootUri: pathToFileURL(temp).href,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: pathToFileURL(temp).href, name: 'reopen' }],
    });
    try {
        await run(session, custom);
    } catch (err) {
        session.kill();
        throw err;
    } finally {
        fs.rmSync(temp, { recursive: true, force: true });
    }
}

module.exports = { run: main };

if (require.main === module) {
    main().then(
        () => console.log('lsp golden: 63-slot-popup-reopens passed'),
        (err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        }
    );
}
