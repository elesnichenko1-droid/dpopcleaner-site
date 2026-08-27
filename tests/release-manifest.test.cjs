const test = require('node:test');
const assert = require('node:assert/strict');
const { isUsableManifest } = require('../release-manifest.js');

const validManifest = Object.freeze({
  product: 'DPopCleaner',
  channel: 'stable',
  version: '0.4.18',
  version_code: 418,
  revision: 1,
  available: true,
  signed: false,
  download_url: 'https://github.com/elesnichenko1-droid/dpopcleaner-site/releases/download/v0.4.18/DPopCleaner_Setup_0.4.18.exe',
  sha256: 'a'.repeat(64),
  size: 3000000,
});

test('accepts the exact published 0.4.18 rev.1 release contract', () => {
  assert.equal(isUsableManifest(validManifest), true);
});

const invalidCases = [
  ['missing manifest', null],
  ['unavailable release', { ...validManifest, available: false }],
  ['wrong version', { ...validManifest, version: '0.4.17' }],
  ['wrong revision', { ...validManifest, revision: 2 }],
  ['zero revision', { ...validManifest, revision: 0 }],
  ['HTTP URL', { ...validManifest, download_url: validManifest.download_url.replace('https://', 'http://') }],
  ['wrong release tag', { ...validManifest, download_url: validManifest.download_url.replace('v0.4.18', 'v0.4.17') }],
  ['wrong asset name', { ...validManifest, download_url: validManifest.download_url.replace('DPopCleaner_Setup_0.4.18.exe', 'DPopCleaner_Setup.exe') }],
  ['zero size', { ...validManifest, size: 0 }],
  ['non-numeric size', { ...validManifest, size: 'unknown' }],
  ['short SHA-256', { ...validManifest, sha256: 'a'.repeat(63) }],
  ['non-hex SHA-256', { ...validManifest, sha256: 'z'.repeat(64) }],
];

for (const [name, manifest] of invalidCases) {
  test(`rejects ${name}`, () => {
    assert.equal(isUsableManifest(manifest), false);
  });
}
