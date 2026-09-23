/**
 * Conventional Commits enforcement for CI.
 *
 * The local enforcer is tools/hooks/commit-msg (POSIX sh - a C++ repository
 * must not require `npm install` before you can commit). Both read the SAME
 * tools/hooks/scopes.txt, so they cannot disagree about what a valid scope is.
 *
 * See docs/contributing/commit-convention.md and ADR-0013.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
const fs = require('fs');
const path = require('path');

const scopesFile = path.join(__dirname, 'tools', 'hooks', 'scopes.txt');
const scopes = fs
  .readFileSync(scopesFile, 'utf8')
  .split('\n')
  .map((l) => l.trim())
  .filter((l) => l && !l.startsWith('#'));

module.exports = {
  extends: ['@commitlint/config-conventional'],
  rules: {
    'type-enum': [
      2,
      'always',
      ['feat', 'fix', 'perf', 'refactor', 'docs', 'style', 'test', 'build', 'ci', 'chore', 'revert'],
    ],
    'scope-enum': [2, 'always', scopes],
    'scope-case': [2, 'always', 'lower-case'],
    'subject-case': [2, 'always', 'lower-case'],
    'subject-full-stop': [2, 'never', '.'],
    'subject-empty': [2, 'never'],
    'type-empty': [2, 'never'],
    'header-max-length': [2, 'always', 72],
    'body-max-line-length': [2, 'always', 100],
    'body-leading-blank': [2, 'always'],
    'footer-leading-blank': [2, 'always'],
  },
  plugins: [
    {
      rules: {
        /**
         * English-only, checked the only way a linter can: the header must be
         * ASCII. Mirrors the same check in tools/hooks/commit-msg.
         */
        'header-ascii-only': ({ header }) => [
          /^[\x20-\x7E]*$/.test(header || ''),
          'header must be ASCII - this project commits in English',
        ],
        /**
         * A scope is required for anything that changes behaviour. Housekeeping
         * types (docs, chore, ci, style, test, revert) may omit it.
         */
        'scope-required-for-behaviour': ({ type, scope }) => [
          !['feat', 'fix', 'perf', 'refactor'].includes(type) || Boolean(scope),
          `a scope is required for "${type}" - e.g. ${type}(semver): ...`,
        ],
      },
    },
  ],
};
