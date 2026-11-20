'use strict';

// `\`-continued if headers are validated as one logical line
// (docs/adr/0014-native-if-header-continuation.md). Native only: the frozen
// Node server still reads each physical line.

const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '..', '..');

const MISSING_THEN = 'If statement must be followed by a then.';
const DANGLING = "Line continuation '\\' must be followed by a code line.";
const PRECEDENCE = 'If condition with both and and or requires parentheses to clarify precedence.';
const INDENT = "This line should be more indented than the matching 'if' or 'loop'.";

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

function related(diagnostic) {
  const info = diagnostic && diagnostic.relatedInformation;
  return Array.isArray(info) && info[0] ? info[0].message : '';
}

function inner(diagnostic) {
  return related(diagnostic) || diagnostic.message;
}

function dump(items) {
  return items
    .map((item) => `${item.range.start.line}:${item.range.start.character}:${item.severity}:${inner(item)}`)
    .join(' | ');
}

function find(items, text, line) {
  return items.filter(
    (item) => inner(item) === text && (line === undefined || item.range.start.line === line)
  );
}

let nextDoc = 1;

async function pull(session, name, text) {
  const uri = pathToFileURL(
    path.join(ROOT, 'test-fixtures', 'if-logical-operators', `.golden-continuation-${name}-${nextDoc}.prg`)
  ).href;
  nextDoc += 1;
  session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
  const report = await session.diagnostic(uri);
  assert(report && report.kind === 'full', `${name} pull kind`);
  return report.items || [];
}

function expectNone(items, name, texts) {
  for (const text of texts) {
    assert(find(items, text).length === 0, `${name}: unexpected "${text}": ${dump(items)}`);
  }
}

async function run(session) {
  await session.initialize();
  session.didChangeConfiguration({ variables: { noteUnusedVariables: true } });

  // system/macros/triggers/ModEHX.p:31-32: the only real form in the corpus.
  const modehx = await pull(
    session,
    'modehx',
    'A$ = "1x"\nB = 0\nif findstr(A$,"1x") or findstr(A$,"2x") or \\\n   findstr(A$,"4x") then\n    B = 1\nendif\n'
  );
  expectNone(modehx, 'modehx', [MISSING_THEN, INDENT, 'if without matching endif']);
  assert(
    !modehx.some((item) => item.range.start.line === 2 || item.range.start.line === 3),
    `modehx: nothing on the header lines (no command syntax on the condition): ${dump(modehx)}`
  );

  const inline = await pull(session, 'inline', 'A = 1\nB = 1\nX = 0\nif A \\\n    B then X = 1\nY = 2\n');
  expectNone(inline, 'inline', [MISSING_THEN, INDENT, 'if without matching endif']);
  assert(
    !inline.some((item) => inner(item).startsWith('Found ')),
    `inline: a split inline if opens no block: ${dump(inline)}`
  );

  const assign = await pull(session, 'assign', 'A = 1\nB = 1\nif A or \\\n    B = 2 then\n    C = 1\nendif\n');
  expectNone(assign, 'assign', [MISSING_THEN, 'Variable B is declared but not used.']);

  const mix = await pull(session, 'mix', 'a = 1\nb = 1\nc = 1\nif a and \\\n    b or c then\n    c = 2\nendif\n');
  assert(find(mix, PRECEDENCE, 4).length === 1, `mix: precedence on the line with \`or\`: ${dump(mix)}`);
  expectNone(mix, 'mix', [MISSING_THEN]);

  const trailing = await pull(
    session,
    'trailing',
    'A = 1\nB = 1\nif A or \\  ;! note\n    B then\n    C = 1\nendif\n'
  );
  expectNone(trailing, 'trailing', [MISSING_THEN, DANGLING, INDENT]);

  const stops = {
    blank: 'A = 1\nif A or \\\n\nX = 1\nendif\n',
    comment: 'A = 1\nif A or \\\n! note\nX = 1\nendif\n',
    eof: 'A = 1\nif A or \\',
  };
  for (const [name, text] of Object.entries(stops)) {
    const items = await pull(session, name, text);
    const dangling = find(items, DANGLING, 1);
    assert(dangling.length === 1, `${name}: one dangling-continuation error: ${dump(items)}`);
    const d = dangling[0];
    assert(d.severity === 1 && d.source === 'ADT-Syntax', `${name}: Error from ADT-Syntax`);
    assert(
      d.range.start.character === 8 && d.range.end.line === 1 && d.range.end.character === 9,
      `${name}: range is the \`\\\`: ${JSON.stringify(d.range)}`
    );
    expectNone(items, name, [MISSING_THEN]);
    if (name !== 'eof') {
      // Still a block opener: the endif pairs with it.
      expectNone(items, name, ['endif without matching if', 'if without matching endif']);
    }
  }

  // Unchanged: one-line headers and and/or-leading continuation lines.
  const single = await pull(session, 'single', 'A = 1\nif A\n    X = 1\nendif\n');
  assert(find(single, MISSING_THEN, 1).length === 1, `single: missing then still reported: ${dump(single)}`);
  expectNone(single, 'single', [DANGLING]);
}

async function main() {
  const session = LspSession.launch();
  try {
    await run(session);
    await session.shutdownAndExit();
  } catch (err) {
    session.kill();
    throw err;
  }
}

module.exports = { run: main };
