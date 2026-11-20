'use strict';

/**
 * Recorded answers from the removed Node language server.
 *
 * Goldens 41-47 compared the native server against the live Node server
 * (server/out/server.js). Node was removed on 2026-11-20 (ADR 0018). Before
 * that, each oracle session's request results were recorded in call order to
 * scripts/goldens/oracle-tapes/<name>.json, one array per oracle session in
 * launch order. replayOracle() now plays them back.
 *
 * Absolute paths in the recorded answers are stored as anchor tokens:
 * @@A<anchor>S<style>@@. Anchors are the oracle workspace root, its ancestors
 * inside the temp directory, and the repo root. Style records the spelling:
 * separator, case, drive form, file:/// prefix, %20. On replay each token
 * is spelled the same way for the current run's paths, so the fresh mkdtemp
 * names match. Files Node wrote at a path named in an answer are stored with
 * that answer and written back on replay.
 */

const { EventEmitter } = require('events');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { fileURLToPath } = require('url');

const ROOT = path.resolve(__dirname, '..');
const TAPE_DIR = path.join(__dirname, 'goldens', 'oracle-tapes');
const TOKEN = /@@A(\d+)S(\d+)@@/g;
const sessionsLaunched = new Map();

const STYLES = [];
for (const prefix of ['', 'file:///']) {
    for (const sep of ['\\', '/']) {
        for (const lower of [false, true]) {
            for (const drive of ['C:', 'c:', 'c%3A', 'C%3A']) {
                for (const space of [' ', '%20']) {
                    STYLES.push({ prefix, sep, lower, drive, space });
                }
            }
        }
    }
}

function render(fsPath, style) {
    let s = fsPath.replace(/[\\/]+/g, style.sep);
    if (style.lower) {
        s = s.toLowerCase();
    }
    s = s.replace(/^([A-Za-z]):/, (_, letter) =>
        (style.drive[0] === 'C' ? letter.toUpperCase() : letter.toLowerCase()) + style.drive.slice(1));
    s = s.replace(/ /g, style.space);
    return style.prefix + s;
}

function anchorsFor(session) {
    const anchors = [];
    const tmp = path.resolve(os.tmpdir()).toLowerCase();
    if (session._rootUri) {
        let dir = path.resolve(fileURLToPath(session._rootUri));
        while (dir.toLowerCase().startsWith(tmp + path.sep)) {
            anchors.push(dir);
            dir = path.dirname(dir);
        }
        if (anchors.length === 0) {
            anchors.push(path.resolve(fileURLToPath(session._rootUri)));
        }
    }
    anchors.push(ROOT);
    return anchors;
}

function decode(value, anchors) {
    const walk = (v) => {
        if (typeof v === 'string') {
            return v.replace(TOKEN, (_, a, s) => {
                const anchor = anchors[Number(a)];
                if (!anchor) {
                    throw new Error(`oracle tape: anchor ${a} has no counterpart in this run`);
                }
                return render(anchor, STYLES[Number(s)]);
            });
        }
        if (Array.isArray(v)) {
            return v.map(walk);
        }
        if (v && typeof v === 'object') {
            const o = {};
            for (const [k, x] of Object.entries(v)) {
                o[walk(k)] = walk(x);
            }
            return o;
        }
        return v;
    };
    return walk(value);
}

function fakeChild() {
    const child = new EventEmitter();
    child.stdout = new EventEmitter();
    child.stderr = new EventEmitter();
    child.stdin = { write() {}, end() {}, setDefaultEncoding() {} };
    child.kill = () => {};
    return child;
}

/**
 * A SessionClass instance whose requests are answered from the tape.
 * Notifications and server-initiated traffic are dropped.
 */
function replayOracle(SessionClass, options, name) {
    const tapePath = path.join(TAPE_DIR, `${name}.json`);
    const sessionIndex = sessionsLaunched.get(name) || 0;
    sessionsLaunched.set(name, sessionIndex + 1);
    const tape = JSON.parse(fs.readFileSync(tapePath, 'utf8'))[sessionIndex];
    if (!tape) {
        throw new Error(`oracle tape ${name}: no recording for oracle session ${sessionIndex}`);
    }
    let next = 0;

    class ReplaySession extends SessionClass {
        _write() {}

        _request(method, params) {
            const index = next++;
            const entry = tape[index];
            if (!entry || entry.method !== method) {
                return Promise.reject(new Error(
                    `oracle tape ${name}[${sessionIndex}]: call ${index} is ${method}, tape has ${entry ? entry.method : 'nothing'}. ` +
                    'The golden changed its oracle calls; the Node oracle is gone, so assert native output directly.'
                ));
            }
            if (entry.rejected !== undefined) {
                return Promise.reject(new Error(decode(entry.rejected, this._anchors)));
            }
            // Files Node wrote and the golden reads back (47's CSV exports).
            for (const [file, text] of Object.entries(entry.files || {})) {
                const target = decode(file, this._anchors);
                fs.mkdirSync(path.dirname(target), { recursive: true });
                fs.writeFileSync(target, decode(text, this._anchors));
            }
            return Promise.resolve(decode(entry.payload, this._anchors));
        }

        async waitQuiet() {}

        async shutdownAndExit() {
            return { code: 0, signal: null };
        }

        kill() {}
    }

    const session = new ReplaySession(fakeChild(), options);
    session._anchors = anchorsFor(session);
    return session;
}

// Goldens 36-39 called pure helpers from server/out (targetSectionSupport,
// formNameCompletion) to build their expected output. Each call's result is
// recorded under its function name and arguments, with the repo root and temp
// directories spelled as <ROOT> and <TMP>. A call the tape does not know fails.
function callKey(fn, args) {
    const tmp = path.resolve(os.tmpdir());
    // JSON spells a backslash as two characters; fold both separators to '/'.
    let key = `${fn}(${JSON.stringify(args)})`.split('\\\\').join('/');
    const escape = (text) => text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    const slashed = (dir) => dir.split(path.sep).join('/');
    key = key.replace(new RegExp(escape(slashed(ROOT)), 'gi'), '<ROOT>');
    key = key.replace(new RegExp(escape(slashed(tmp)) + '/[^/"]+', 'gi'), '<TMP>');
    return key;
}

// Repeated calls with the same key replay their recorded results in order (a
// helper that reads the disk can answer differently after a file is added);
// calls past the end repeat the last result.
function replayModule(name, tapeName) {
    const file = `${tapeName}-${name}.json`;
    const tape = JSON.parse(fs.readFileSync(path.join(TAPE_DIR, file), 'utf8'));
    const seen = new Map();
    return new Proxy({}, {
        get(_, fn) {
            return (...args) => {
                const key = callKey(fn, args);
                if (!Object.prototype.hasOwnProperty.call(tape, key)) {
                    throw new Error(
                        `oracle tape ${file}: no recorded result for ${key.slice(0, 300)}. ` +
                        'The Node helper is gone, so assert native output directly.'
                    );
                }
                const count = seen.get(key) || 0;
                seen.set(key, count + 1);
                const entry = tape[key][Math.min(count, tape[key].length - 1)];
                return entry.undefined ? undefined : JSON.parse(JSON.stringify(entry.result));
            };
        },
    });
}

module.exports = { replayOracle, replayModule, callKey, TAPE_DIR };
