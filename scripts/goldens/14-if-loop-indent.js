'use strict';

const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspSession } = require('../lsp-golden-harness');

const ROOT = path.resolve(__dirname, '..', '..');
const FIXTURE_DIR = path.join(ROOT, 'test-fixtures', 'if-logical-operators');
const EDIT_FORM_PATH = path.join(
  ROOT,
  'test-fixtures',
  'verification-ignored-sections',
  'custom',
  'edit_form_ignored.prg'
);

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
  if (!Array.isArray(info) || !info[0]) {
    return '';
  }
  return info[0].message;
}

function dump(items) {
  return items
    .map((item) => {
      const line = item.range && item.range.start ? item.range.start.line : '?';
      return `${line}:${item.severity}:${item.source}:${related(item) || item.message}`;
    })
    .join(' | ');
}

function hasRelated(items, inner, line) {
  return items.some((item) => {
    if (related(item) !== inner) {
      return false;
    }
    return line === undefined || (item.range && item.range.start && item.range.start.line === line);
  });
}

function hasPlacement(items) {
  return items.some((item) => String(related(item)).startsWith('Logical operator'));
}

function readFixture(name) {
  return fs.readFileSync(path.join(FIXTURE_DIR, name), 'utf8');
}

let nextDoc = 1;

async function pull(session, text, name) {
  const uri = pathToFileURL(
    path.join(ROOT, 'test-fixtures', 'if-logical-operators', `.golden-${name || nextDoc}.prg`)
  ).href;
  nextDoc += 1;
  session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
  const report = await session.diagnostic(uri);
  assert(report && report.kind === 'full', `${name || uri} pull kind`);
  return { uri, items: report.items || [] };
}

async function pullFile(session, filePath) {
  const uri = pathToFileURL(filePath).href;
  const text = fs.readFileSync(filePath, 'utf8');
  session.didOpen({ uri, languageId: 'vertexadt', version: 1, text });
  const report = await session.diagnostic(uri);
  assert(report && report.kind === 'full', `${filePath} pull kind`);
  return { uri, items: report.items || [], text };
}

async function run(session) {
  await session.initialize();

  const danglingSame = await pull(session, readFixture('dangling_same_line.prg'), 'dangling_same_line');
  assert(
    hasRelated(
      danglingSame.items,
      'Logical operator must not appear directly before then without a condition.',
      0
    ),
    `dangling_same_line placement: ${dump(danglingSame.items)}`
  );
  assert(danglingSame.items[0] && danglingSame.items[0].severity === 1, 'dangling_same_line Error');
  assert(danglingSame.items[0] && danglingSame.items[0].source === 'ADT-Syntax', 'dangling_same_line source');

  const danglingOr = await pull(session, readFixture('dangling_or.prg'), 'dangling_or');
  assert(
    hasRelated(
      danglingOr.items,
      'Logical operator must not appear directly before then without a condition.',
      0
    ),
    `dangling_or placement: ${dump(danglingOr.items)}`
  );

  const leading = await pull(session, readFixture('leading_operator_same_line.prg'), 'leading');
  assert(
    hasRelated(
      leading.items,
      'Logical operator must not appear directly after if without a preceding condition.',
      0
    ),
    `leading_operator_same_line: ${dump(leading.items)}`
  );

  const validCombined = await pull(session, readFixture('valid_combined.prg'), 'valid_combined');
  assert(!hasPlacement(validCombined.items), `valid_combined placement: ${dump(validCombined.items)}`);
  assert(
    hasRelated(validCombined.items, 'endif without matching if', 1),
    `valid_combined extra endif: ${dump(validCombined.items)}`
  );

  const validMultiline = await pull(session, readFixture('valid_multiline.prg'), 'valid_multiline');
  assert(!hasPlacement(validMultiline.items), `valid_multiline placement: ${dump(validMultiline.items)}`);
  assert(
    hasRelated(validMultiline.items, 'If statement must be followed by a then.', 0),
    `valid_multiline pairing: ${dump(validMultiline.items)}`
  );
  assert(
    hasRelated(
      validMultiline.items,
      "This line should be more indented than the matching 'if' or 'loop'.",
      1
    ),
    `valid_multiline indent continuation: ${dump(validMultiline.items)}`
  );

  const danglingMulti = await pull(session, readFixture('dangling_multiline.prg'), 'dangling_multiline');
  assert(
    hasRelated(
      danglingMulti.items,
      'Logical operator must not appear directly before then without a condition.',
      1
    ),
    `dangling_multiline continuation: ${dump(danglingMulti.items)}`
  );

  const precedence =
    'If condition with both and and or requires parentheses to clarify precedence.';

  const validMix = await pull(session, readFixture('valid_parenthesized_mix.prg'), 'valid_mix');
  assert(!hasRelated(validMix.items, precedence), `valid_parenthesized_mix mix: ${dump(validMix.items)}`);
  assert(!hasPlacement(validMix.items), `valid_parenthesized_mix placement: ${dump(validMix.items)}`);

  const rootMix = await pull(session, readFixture('invalid_root_mix.prg'), 'root_mix');
  assert(hasRelated(rootMix.items, precedence, 0), `invalid_root_mix: ${dump(rootMix.items)}`);

  const innerMix = await pull(session, readFixture('invalid_inner_mix.prg'), 'inner_mix');
  assert(hasRelated(innerMix.items, precedence, 0), `invalid_inner_mix: ${dump(innerMix.items)}`);

  const parenTerms = await pull(session, readFixture('invalid_paren_terms_mix.prg'), 'paren_terms');
  assert(hasRelated(parenTerms.items, precedence, 0), `invalid_paren_terms_mix: ${dump(parenTerms.items)}`);

  const wordSymbol = await pull(session, 'if a and b && c then\n    X = 1\nendif\n', 'word_symbol');
  assert(
    hasRelated(
      wordSymbol.items,
      'If condition must not mix word operators (and/or) with symbol operators (&&/||). Use one style consistently or add parentheses.',
      0
    ),
    `word/symbol mix: ${dump(wordSymbol.items)}`
  );

  const grouped = await pull(session, 'if (a and b) or c then\n    X = 1\nendif\n', 'grouped');
  assert(!hasRelated(grouped.items, precedence), `grouped mix should be valid: ${dump(grouped.items)}`);

  const clean = await pull(session, 'A = 1\nB = 1\nif A then\n    B = 1\nendif\n', 'clean');
  assert(!hasRelated(clean.items, 'if without matching endif'), `clean leftover if: ${dump(clean.items)}`);
  assert(!hasRelated(clean.items, 'endif without matching if'), `clean extra endif: ${dump(clean.items)}`);
  assert(
    !clean.items.some((item) => String(related(item)).includes("Found ")),
    `clean leftover indent: ${dump(clean.items)}`
  );

  const leftover = await pull(session, 'A = 1\nX = 1\nif A then\n    X = 1\n', 'leftover');
  assert(hasRelated(leftover.items, 'if without matching endif', 2), `leftover if: ${dump(leftover.items)}`);
  assert(
    leftover.items.some(
      (item) =>
        item.severity === 2 &&
        related(item) === "Found 1 'if' or 'loop' without a matching 'endif' or 'endloop'." &&
        item.message === " has an error: Found 1 'if' or 'loop' without a matching 'endif' or 'endloop'."
    ),
    `leftover indent EOF: ${dump(leftover.items)}`
  );

  const loopMismatch = await pull(session, 'X = 1\nloop 1\n    X = 1\n endloop\n', 'loop_mismatch');
  assert(
    hasRelated(loopMismatch.items, 'endloop at different indentation level'),
    `loop indent mismatch: ${dump(loopMismatch.items)}`
  );
  assert(
    !hasRelated(loopMismatch.items, 'loop without matching endloop'),
    `loop mismatch still pops: ${dump(loopMismatch.items)}`
  );

  const extraEndloop = await pull(session, 'endloop\n', 'extra_endloop');
  assert(
    hasRelated(extraEndloop.items, 'endloop without matching loop', 0),
    `extra endloop: ${dump(extraEndloop.items)}`
  );

  const elsewhere = await pull(session, 'elsewhere = 1\n', 'elsewhere');
  assert(
    hasRelated(
      elsewhere.items,
      "Found an 'endif', 'endloop', or 'else' without a matching 'if' or 'loop'.",
      0
    ),
    `elsewhere prefix-else: ${dump(elsewhere.items)}`
  );

  const ifdef = await pull(session, 'ifdef x\n', 'ifdef');
  assert(hasRelated(ifdef.items, 'if without matching endif', 0), `ifdef leftover if: ${dump(ifdef.items)}`);
  assert(
    !ifdef.items.some((item) => String(related(item)).includes("Found ")),
    `ifdef should not leftover-indent: ${dump(ifdef.items)}`
  );

  const inlineEndif = await pull(session, 'if A = 1 and B = 2 then C = 3\nendif\n', 'inline_endif');
  assert(
    hasRelated(inlineEndif.items, 'endif without matching if', 1),
    `inline if-then extra endif: ${dump(inlineEndif.items)}`
  );
  assert(
    !hasRelated(inlineEndif.items, 'if without matching endif'),
    `inline if-then should not leftover-if: ${dump(inlineEndif.items)}`
  );

  const leadingDigit = await pull(session, '1bad = 1\n', 'leading_digit');
  assert(
    leadingDigit.items.some(
      (item) =>
        item.severity === 1 &&
        item.source === 'ADT-Syntax' &&
        item.message === '1bad =  has an error.' &&
        related(item) === "Variable can't start with a number."
    ),
    `leading-digit: ${dump(leadingDigit.items)}`
  );

  const equalsAngle = await pull(session, 'X =< 1\nY => 2\n', 'equals_angle');
  assert(
    hasRelated(equalsAngle.items, 'Incorrect operator =<, try: <=', 0),
    `=< : ${dump(equalsAngle.items)}`
  );
  assert(
    hasRelated(equalsAngle.items, 'Incorrect operator =>, try: >=', 1),
    `=> : ${dump(equalsAngle.items)}`
  );

  const includeIndent = await pull(session, '  # include foo\n', 'include_indent');
  assert(
    hasRelated(includeIndent.items, "Lines starting with '# include' should not be indented.", 0),
    `indented include: ${dump(includeIndent.items)}`
  );

  const quotedThen = await pull(session, 'if x = "then"\n    Y = 1\nendif\n', 'quoted_then');
  assert(
    !hasRelated(quotedThen.items, 'If statement must be followed by a then.'),
    `quoted then suppresses pairing: ${dump(quotedThen.items)}`
  );

  const editForm = await pullFile(session, EDIT_FORM_PATH);
  const bodyStart = 5;
  const bodyEnd = 11;
  const inBody = editForm.items.filter((item) => {
    const line = item.range && item.range.start ? item.range.start.line : -1;
    return line >= bodyStart && line <= bodyEnd;
  });
  assert(inBody.length === 0, `EDIT_FORM body must be silent: ${dump(inBody)}`);
  assert(
    hasRelated(
      editForm.items,
      'Logical operator must not appear directly before then without a condition.',
      0
    ),
    `EDIT_FORM outside dangling and: ${dump(editForm.items)}`
  );
  assert(
    hasRelated(
      editForm.items,
      'Logical operator must not appear directly before then without a condition.',
      14
    ),
    `EDIT_FORM outside dangling or: ${dump(editForm.items)}`
  );
  assert(
    !editForm.items.some((item) => related(item) === "Variable can't start with a number."),
    `EDIT_FORM leading-digit suppressed: ${dump(editForm.items)}`
  );

  session.didChangeConfiguration({ diagnostics: { maxNumberOfProblems: 2 } });
  const cap = await pull(
    session,
    [
      'A = 1',
      'Y = 1',
      'Z = 1',
      'W = 1',
      'x = 1',
      'if A then',
      '    Y = 1',
      'if and x then',
      '    Z = 1',
      'if and x then',
      '    W = 1',
      'endif',
      'endif',
      'endif',
      '',
    ].join('\n'),
    'cap'
  );
  const placementCount = cap.items.filter((item) => String(related(item)).startsWith('Logical operator')).length;
  assert(placementCount === 2, `cap 2 placement diagnostics, got ${dump(cap.items)}`);
  assert(
    cap.items.some((item) => related(item) === 'if without matching endif'),
    `cap leftover-if ignores cap: ${dump(cap.items)}`
  );

  await session.shutdownAndExit();
}

async function main() {
  const session = LspSession.launch();
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
      console.log('14-if-loop-indent goldens passed');
    })
    .catch((err) => {
      console.error(err.stack || err.message || err);
      process.exit(1);
    });
}

module.exports = { run: main };
