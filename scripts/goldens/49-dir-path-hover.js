'use strict';

/**
 * Native goldens for $DIR_* path hover (docs/adr/0003).
 * Assertions from test-fixtures/dir-hover/TESTING.md. Native only: the Node
 * server is frozen and has no $DIR_* path hover.
 */

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '../..');
const FIXTURE = path.join(ROOT, 'test-fixtures', 'dir-hover');
const CALLER = path.join(FIXTURE, 'custom', 'macros', 'secframing', 'caller.prg');

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

function hoverText(result) {
    return result && result.contents ? result.contents.value : null;
}

// The `→ …` path line of a hover, or null.
function pathLine(result) {
    const text = hoverText(result);
    if (!text) {
        return null;
    }
    const line = text.split('\n').find((l) => l.startsWith('→ '));
    // The server encodes the drive colon (C%3A) like VS Code's Uri does.
    return line ? decodeURIComponent(line.slice(2)) : null;
}

function linked(relative, absolute) {
    return decodeURIComponent(`[\`${relative}\`](${pathToFileURL(absolute).href})`);
}

async function main() {
    const lines = fs.readFileSync(CALLER, 'utf8').split(/\r?\n/);
    const rootUri = pathToFileURL(FIXTURE).href;
    const uri = pathToFileURL(CALLER).href;
    const session = LspSession.launch({
        rootUri,
        extensionUri: pathToFileURL(ROOT).href,
        workspaceFolders: [{ uri: rootUri, name: 'dir-hover' }],
    });
    // Hover at the `nth` occurrence of `needle` on the line that contains it.
    const hoverAt = (needle, nth = 0, offset = 0) => {
        const line = lines.findIndex((l) => l.includes(needle));
        assert(line >= 0, `fixture has ${needle}`);
        let column = -1;
        for (let i = 0; i <= nth; i += 1) {
            column = lines[line].indexOf(needle, column + 1);
        }
        return session.hover(uri, line, column + offset);
    };
    const hoverVar = (name) => {
        const line = lines.findIndex((l) => l.startsWith(`${name} =`));
        assert(line >= 0, `fixture assigns ${name}`);
        return session.hover(uri, line, 0);
    };
    const expectPath = async (promise, expected, what) => {
        const got = pathLine(await promise);
        assert(got === expected, `${what}: expected ${expected}, got ${got}`);
    };
    try {
        await session.initialize();
        session.didOpen({ uri, text: lines.join('\r\n') });
        await session.waitQuiet();

        const callHover = await hoverAt('$DIR_MACROS/triggers');
        assert(hoverText(callHover).includes('Returns the directory where the macro-ADT files are located.'),
            'token hover carries the catalog detail');
        await expectPath(Promise.resolve(callHover),
            linked('custom/macros/triggers/save_building', path.join(FIXTURE, 'custom/macros/triggers/save_building')),
            'call path resolves in custom, no extension added');
        await expectPath(hoverAt('$DIR_MACROS/triggers', 0, 1),
            linked('custom/macros/triggers/save_building', path.join(FIXTURE, 'custom/macros/triggers/save_building')),
            'hover inside the token');
        await expectPath(hoverAt('$DIR_MACROS/secframing'),
            linked('system/macros/secframing/globalsizeminpmaxp.prg',
                path.join(FIXTURE, 'system/macros/secframing/globalsizeminpmaxp.prg')),
            'quoted call path falls back to system');
        await expectPath(hoverAt('$DIR_SETUP', 1), '`none/setup/GEOMPROP.CUSTOM`', '#..# interpolation, in neither tree');
        await expectPath(hoverAt('$DIR_PICTS_SHARED'), '`shared/picts`', 'shared without a shared folder');
        await expectPath(hoverAt('$DIR_SYMBOLS'),
            linked('system/symbols', path.join(FIXTURE, 'system/symbols')), 'SYMBOLS falls back to system');
        await expectPath(hoverAt('$DIR_THIS'),
            '`custom/macros/secframing/beam_web.prg`', 'THIS is the edited file\'s folder');

        await expectPath(hoverVar('pics$'), '`shared/picts/`', 'variable from get() ends with a slash');
        await expectPath(hoverVar('base$'),
            linked('custom/macros', path.join(FIXTURE, 'custom/macros')), 'variable from a literal');
        await expectPath(hoverVar('trig$'),
            linked('custom/macros/triggers/save_building', path.join(FIXTURE, 'custom/macros/triggers/save_building')),
            'variable through & and #var# interpolation');
        await expectPath(hoverVar('proj$'), '`shared/projects/<actproj>/<name$>`', 'ACTPROJ and an unknown variable');
        await expectPath(hoverVar('lib$'),
            linked('custom/complibs/', path.join(FIXTURE, 'custom/complibs')), 'COMPLIBS from get()');

        // Building on a resolved path (lines 15-20 of caller.prg).
        const complibs = path.join(FIXTURE, 'custom/complibs');
        const door = path.join(complibs, 'DOOR_CUSTOM');
        await expectPath(hoverAt('Dir$ = get'), linked('custom/complibs/', complibs), 'first Dir$');
        await expectPath(hoverAt('Dir$ = Dir$'), linked('custom/complibs/DOOR_CUSTOM/', door), 'Dir$ left of =');
        await expectPath(hoverAt('Dir$&Lib$'), linked('custom/complibs/', complibs), 'Dir$ right of = is the old value');
        await expectPath(hoverAt('Dir$&"door'), linked('custom/complibs/DOOR_CUSTOM/', door), 'Dir$ in Path$');
        await expectPath(hoverVar('Path$'),
            linked('custom/complibs/DOOR_CUSTOM/door_d.vxp', path.join(door, 'door_d.vxp')), 'Path$');
        await expectPath(hoverVar('dup$'), linked('custom/complibs//DOOR_CUSTOM', door), 'duplicate slash is shown');
        await expectPath(hoverVar('glued$'), '`none/macrosDOOR_CUSTOM`', 'missing slash is shown glued');
        assert(pathLine(await hoverVar('Lib$')) === null, 'Lib$ holds no token');
        const varText = hoverText(await hoverVar('base$'));
        assert(varText.startsWith('String Variable:'), `variable hover heading, got ${varText}`);

        assert((await hoverAt('$dir_macros')) === null, 'lowercase token has no hover');
        assert(pathLine(await hoverVar('low$')) === null, 'lowercase token in a variable has no path');
        assert(pathLine(await hoverVar('plus$')) === null, '+ is not string concatenation');
        assert(pathLine(await hoverVar('sub$')) === null, 'plain string variable has no path');
        const keyword = hoverText(await hoverAt('text_edit'));
        assert(keyword && !keyword.includes('→ '), 'keyword hover unchanged');

        await session.shutdownAndExit();
    } catch (err) {
        session.kill();
        throw err;
    }
}

if (require.main === module) {
    main()
        .then(() => {
            console.log('lsp golden: 49-dir-path-hover passed');
        })
        .catch((err) => {
            console.error(err.stack || err.message || err);
            process.exit(1);
        });
}

module.exports = { run: main, main };
