'use strict';

/**
 * Native slot completion contract:
 * - docs/adr/0001-native-completion-ranks-slot-items-first.md
 * - docs/adr/0002-native-slot-completion-substring-match.md
 * - docs/adr/0007-native-trigger-characters-open-family-sites-only.md
 *
 * When a slot-specific family adds items, every family item gets sortText
 * `0` + family digit + match group + its family key (its old sortText, else
 * its label), and every cheap keyword-variable item gets `1` + label. The match
 * group is `0` for a start-of-label match (or nothing typed) and `1` for a
 * match elsewhere in the label. Family items are kept only when their label
 * contains the typed text (case-insensitive); their filterText is then VS
 * Code's typed text. A response with family items is `isIncomplete`.
 * Responses with no family items leave the cheap list untouched (no sortText),
 * except a typed trigger character, which then gets an empty list
 * (docs/adr/0007-native-trigger-characters-open-family-sites-only.md).
 * Deliberate divergence from the frozen Node server.
 */

// Existing append order: cheap-path families first, then the heavy pool's.
const FAMILY_DIGIT = {
    dbcreateField: '0',
    debugFilename: '1',
    includeCall: '2',
    remainingParam: '3',
    callReturnValue: '4',
    formName: '5',
    objectParameter: '6',
    macroLibrary: '7',
};

const FAMILY_SORT = /^0[0-7][01]/;

function digitOf(family) {
    const digit = FAMILY_DIGIT[family];
    if (digit === undefined) {
        throw new Error(`unknown completion family ${family}`);
    }
    return digit;
}

function specialSortText(family, key, group = '0') {
    return `0${digitOf(family)}${group}${key}`;
}

function isFamilyItem(item, family) {
    return typeof item.sortText === 'string'
        && FAMILY_SORT.test(item.sortText)
        && item.sortText[1] === digitOf(family);
}

// `0` start-of-label match (or nothing typed), `1` match elsewhere.
function familyGroup(item) {
    return item.sortText[2];
}

// The family key the item had before ranking.
function familyKey(item) {
    return item.sortText.slice(3);
}

// Every item is either a family item or a cheap item ranked `1` + label; at
// least one of each.
function assertRankedSpecialsFirst(items, message) {
    let specials = 0;
    let cheap = 0;
    for (const item of items) {
        if (typeof item.sortText === 'string' && item.sortText.startsWith('0')) {
            if (!FAMILY_SORT.test(item.sortText)) {
                throw new Error(`${message}: ${item.label} sortText ${JSON.stringify(item.sortText)} has no family digit and group`);
            }
            specials += 1;
            continue;
        }
        if (item.sortText !== `1${item.label}`) {
            throw new Error(
                `${message}: cheap item ${item.label} sortText expected ${JSON.stringify(`1${item.label}`)}, got ${JSON.stringify(item.sortText)}`
            );
        }
        cheap += 1;
    }
    if (specials === 0 || cheap === 0) {
        throw new Error(`${message}: expected family and cheap items, got ${specials} family and ${cheap} cheap`);
    }
}

// No family item: the cheap list is untouched.
function assertUnranked(items, message) {
    for (const item of items) {
        if (item.sortText !== undefined) {
            throw new Error(`${message}: ${item.label} has sortText ${JSON.stringify(item.sortText)}`);
        }
    }
}

// Checks a whole CompletionList against the contract, then returns `family`'s
// items with sortText reduced to the family key (and `group` added), so
// family-level assertions read as before ranking. A family item whose key is
// its label had no sortText of its own (or its own sortText equaled its label).
function familyItemsOf(result, family, message) {
    const items = result.items;
    if (items.some((item) => typeof item.sortText === 'string' && item.sortText.startsWith('0'))) {
        assertRankedSpecialsFirst(items, message);
        if (result.isIncomplete !== true) {
            throw new Error(`${message}: a response with family items must be isIncomplete`);
        }
    } else {
        assertUnranked(items, message);
    }
    return items
        .filter((item) => isFamilyItem(item, family))
        .map((item) => ({ ...item, sortText: familyKey(item), group: familyGroup(item) }));
}

// familyItemsOf without the `group` field, for whole-item comparisons.
function familyItems(result, family, message) {
    return familyItemsOf(result, family, message).map(({ group, ...item }) => item);
}

module.exports = {
    FAMILY_DIGIT,
    specialSortText,
    isFamilyItem,
    familyGroup,
    familyKey,
    assertRankedSpecialsFirst,
    assertUnranked,
    familyItems,
    familyItemsOf,
};
