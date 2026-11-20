'use strict';

/**
 * Native-only goldens: a typed trigger character opens a popup only where a
 * completion family has items
 * (docs/adr/0007-native-trigger-characters-open-family-sites-only.md). No Node
 * oracle: the Node server is frozen and always answers with the full list.
 */

const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');
const ranking = require('../completion-ranking');

const ROOT = path.resolve(__dirname, '../..');
const INVOKED = 1;
const TYPED = 2;
const INCOMPLETE = 3;
const NO_POPUP = { isIncomplete: false, items: [] };

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

function assertFullList(result, message) {
    assert(result && Array.isArray(result.items), `${message}: returns a CompletionList`);
    assert(result.items.some((item) => item.label === 'askdim'), `${message}: has the keyword list`);
    assert(result.items.some((item) => item.label === 'width'), `${message}: has buffer variables`);
}

class Probe {
    constructor(session, uri) {
        this.session = session;
        this.uri = uri;
        this.version = 0;
    }

    // `|` marks the cursor on the last line; without one it is at the end.
    async complete(textWithCursor, context) {
        const lines = textWithCursor.split('\n');
        const line = lines.length - 1;
        let character = lines[line].indexOf('|');
        if (character >= 0) {
            lines[line] = lines[line].replace('|', '');
        } else {
            character = lines[line].length;
        }
        const text = lines.join('\n');
        this.version += 1;
        if (this.version === 1) {
            this.session.didOpen({ uri: this.uri, languageId: 'vertexadt', version: 1, text });
        } else {
            this.session.didChange({ uri: this.uri, version: this.version, contentChanges: [{ text }] });
        }
        return this.session.request('textDocument/completion', {
            textDocument: { uri: this.uri },
            position: { line, character },
            context,
        });
    }

    typed(text, triggerCharacter) {
        return this.complete(text, { triggerKind: TYPED, triggerCharacter });
    }
}

async function main() {
    const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'vertex-adt-trigger-family-'));
    try {
        const custom = path.join(temp, 'customa');
        fs.mkdirSync(path.join(custom, 'forms'), { recursive: true });
        fs.mkdirSync(path.join(custom, 'complibs'), { recursive: true });
        fs.mkdirSync(path.join(temp, 'system', 'forms'), { recursive: true });
        fs.mkdirSync(path.join(temp, 'system', 'complibs'), { recursive: true });
        fs.writeFileSync(path.join(custom, 'forms', 'wall.vxf'), '');
        const rootUri = pathToFileURL(temp).href;
        const session = LspSession.launch({
            rootUri,
            extensionUri: pathToFileURL(ROOT).href,
            workspaceFolders: [{ uri: rootUri, name: path.basename(temp) }],
        });
        try {
            await session.initialize();
            await session.waitQuiet();
            const probe = new Probe(session, pathToFileURL(path.join(custom, 'probe.prg')).href);
            const head = 'width = 1\n';

            // Trigger characters off every family site: no popup.
            assertEqual(await probe.typed(`${head}y = `, ' '), NO_POPUP, 'space after =');
            assertEqual(await probe.typed(`${head}if `, ' '), NO_POPUP, 'space after keyword');
            assertEqual(await probe.typed(`${head}y = max(`, '('), NO_POPUP, 'open paren');
            assertEqual(await probe.typed(`${head}y = max(1,`, ','), NO_POPUP, 'comma');
            assertEqual(await probe.typed(`${head}s$ = "`, '"'), NO_POPUP, 'quote');

            // A family site with items: family items first, keyword list below them.
            let result = await probe.typed(`${head}dbedit(1,"`, '"');
            assertEqual(
                ranking.familyItemsOf(result, 'formName', 'form slot').map((item) => item.label),
                ['wall.vxf'],
                'form slot items'
            );
            assertFullList(result, 'form slot');
            ranking.assertRankedSpecialsFirst(result.items, 'form slot');

            // A family site whose listing is empty counts as no site.
            assertEqual(await probe.typed(`${head}M = new_macro(`, '('), NO_POPUP, 'empty macro library listing');

            // The first word character after a space (quick suggestions): full list.
            assertFullList(await probe.complete(`${head}y = w`, { triggerKind: INVOKED }), 'first letter');
            ranking.assertUnranked((await probe.complete(`${head}y = w`, { triggerKind: INVOKED })).items, 'first letter');

            // Typing on in a family slot keeps the keyword list.
            result = await probe.complete(`${head}dbedit(1,"wa`, { triggerKind: INCOMPLETE });
            assertFullList(result, 'typing in form slot');

            // Ctrl+Space right after a space: full list.
            assertFullList(await probe.complete(`${head}y = `, { triggerKind: INVOKED }), 'Ctrl+Space after space');
            // No context (older clients): full list.
            assertFullList(await probe.complete(`${head}y = `, undefined), 'no context');

            await session.shutdownAndExit();
        } catch (err) {
            session.kill();
            throw err;
        }
    } finally {
        fs.rmSync(temp, { recursive: true, force: true });
    }
}

module.exports = { run: main, main };

if (require.main === module) {
    main().then(
        () => console.log('lsp golden: 53-trigger-character-family-only passed'),
        (err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        }
    );
}
