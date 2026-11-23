'use strict';

/**
 * Signature help at cursor positions inside finished commands
 * (docs/adr/0022-signature-box-follows-the-cursor.md): nesting, quotes,
 * brackets, comments, `\` lines, input comments and vertexadt/signatureHelpSites.
 */

const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const CALL = 'call(path, mainDBid, sortmethod$)';
const SR = 'set_regenerable(input object) regenerable value';
const SR_AFTER = 'set_regenerable regenerable value';
const CUBE = 'cube length x, length y, length z';
const P = '"sortDB.prg"';

// [text with `|` at the cursor, [label, active] or null]
const ROWS = [
    // The finished line from the feature request.
    [`dblen$=call("sort|DB.prg",,l25liftholeDB,"XPOS")`, [CALL, 0]],
    [`dblen$=call(${P},|,l25liftholeDB,"XPOS")`, [CALL, 1]],
    [`dblen$=call(${P},,l25lift|holeDB,"XPOS")`, [CALL, 2]],
    [`dblen$=call(${P},,l25liftholeDB,"X|POS")`, null],
    [`dblen$=ca|ll(${P},,l25liftholeDB)`, null],
    [`dblen$=call(${P},a,b)|`, null],
    [`dbl|en$=call(${P},a,b)`, null],
    // No callee, or no inputs.
    ['call("miss|ing.prg",a)', null],
    ['call("noinputs.prg",|a)', null],
    // Nesting.
    [`call(${P},map_size("|n"),b)`, ['map_size(map name)', 0]],
    [`call(${P},map_size("n"),|b)`, [CALL, 2]],
    [`call(${P},map_size("n")|,b)`, [CALL, 1]],
    [`call(${P},foo(a,|b),c)`, [CALL, 1]],
    [`call(${P},(a|),c)`, [CALL, 1]],
    [`call(${P},a$[1,2],|c)`, [CALL, 2]],
    [`call(${P},a$[1,|2],c)`, [CALL, 1]],
    [`call(${P},'x,(y',|c)`, [CALL, 2]],
    [`call(${P},"x,(y'",|c)`, [CALL, 2]],
    [`if call(${P},a,|b) then`, [CALL, 2]],
    // After-paren and unparenthesized arguments.
    ['set_regenerable(She|et) 0', [SR, 0]],
    ['set_regenerable(Sheet) |0', [SR_AFTER, 0]],
    ['set_regenerable(Sheet) foo(a,|b)', [SR_AFTER, 0]],
    ['set_regenerable(Sheet) 0 ; x = |1', null],
    ['if a then cube 1, |2, 3', [CUBE, 1]],
    ['cube 1, abs(2), |3', [CUBE, 2]],
    ['cu|be 1, 2, 3', null],
    ['cube = |1', null],
    // Comments.
    [`call(${P},a ;! b,|c`, null],
    [`!call(${P},|a)`, null],
    [`call(${P},";!",|c)`, [CALL, 2]],
    [`if (!call(${P},|a)) then`, [CALL, 1]],
    // Split lines.
    [`r$=call(${P},a,\\ \n   |b)`, [CALL, 2]],
    [`r$=call(${P},\\ ;! note, with (\n   a, \\\n |`, [CALL, 2]],
    [`r$=call(${P},a,\n   |b)`, null],
    [`! call(${P},a,\\\n   |b)`, null],
    [`set_regenerable(Sheet) \\\n  |0`, [SR_AFTER, 0]],
];

async function run(session, custom) {
    await session.initialize();
    await session.waitQuiet();
    const uri = pathToFileURL(path.join(custom, 'caller.prg')).href;
    let version = 0;
    const ask = async (method, marked) => {
        const lines = marked.split('\n');
        const line = lines.findIndex((row) => row.includes('|'));
        const character = lines[line].indexOf('|');
        lines[line] = lines[line].replace('|', '');
        const text = lines.join('\n');
        version += 1;
        if (version === 1) {
            session.didOpen({ uri, languageId: 'vertexadt', version, text });
        } else {
            session.didChange({ uri, version, contentChanges: [{ text }] });
        }
        return session.request(method, { textDocument: { uri }, position: { line, character } });
    };
    const help = (marked) => ask('textDocument/signatureHelp', marked);
    const sites = (marked) => ask('vertexadt/signatureHelpSites', marked);

    for (const [marked, expected] of ROWS) {
        const result = await help(marked);
        assert.deepStrictEqual(
            result && [result.signatures[0].label, result.activeParameter], expected, marked);
        // The sites agree on whether anything shows.
        assert.strictEqual((await sites(marked)).length > 0, expected !== null, `sites: ${marked}`);
    }

    const result = await help(`call(${P},|a)`);
    assert.deepStrictEqual(result.signatures[0].parameters, [
        { label: 'path' },
        { label: 'mainDBid', documentation: 'Input Parameter 1: Database to sort' },
        { label: 'sortmethod$' },
    ], 'input comments');

    assert.deepStrictEqual(
        await sites(`x=call(${P},map_size("|n"))`),
        [{ line: 0, character: 20 }, { line: 0, character: 2 }],
        'innermost site first');
    assert.deepStrictEqual(
        await sites(`x=1\nx=call(${P},\\\n  map_size("|n"))`),
        [{ line: 2, character: 2 }, { line: 1, character: 2 }],
        'sites on split lines');

    await session.shutdownAndExit();
}

async function main() {
    const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'vertex-adt-signature-follow-'));
    const custom = path.join(temp, 'custom');
    fs.mkdirSync(custom, { recursive: true });
    fs.writeFileSync(
        path.join(custom, 'sortDB.prg'),
        'mainDBid = get($CALLPARAM1) ;! "Database to sort"\nsortmethod$ = get($CALLPARAM2)\n');
    fs.writeFileSync(path.join(custom, 'noinputs.prg'), 'q = 1\n');
    const session = LspSession.launch({
        rootUri: pathToFileURL(temp).href,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: pathToFileURL(temp).href, name: 'signature-follow' }],
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
        () => console.log('lsp golden: 64-signature-box-follows-the-cursor passed'),
        (err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        }
    );
}
