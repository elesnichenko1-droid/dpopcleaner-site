const test = require('node:test');
const assert = require('node:assert/strict');
const { isUsableManifest } = require('../release-manifest.js');

const validManifest = Object.freeze({
  product: 'DPopCleaner',
  channel: 'beta',
  version: '0.2.14',
  version_code: 214,
  revision: 1,
  available: true,
  signed: false,
  download_url: 'https://github.com/elesnichenko1-droid/dpopcleaner-site/releases/download/v0.2.14-clean-r1/DPopCleaner_Setup_0.2.14_BETA_CLEAN_R1.exe',
  sha256: 'a'.repeat(64),
  size: 1,
});

test('accepts the exact published 0.2.14 R1 release contract', () => {
  assert.equal(isUsableManifest(validManifest), true);
});

const invalidCases = [
  ['missing manifest', null],
  ['unavailable release', { ...validManifest, available: false }],
  ['wrong version', { ...validManifest, version: '0.3.1' }],
  ['wrong revision', { ...validManifest, revision: 2 }],
  ['HTTP URL', { ...validManifest, download_url: validManifest.download_url.replace('https://', 'http://') }],
  ['wrong release tag', { ...validManifest, download_url: validManifest.download_url.replace('v0.2.14-clean-r1', 'v0.2.14-beta') }],
  ['wrong asset name', { ...validManifest, download_url: validManifest.download_url.replace('_CLEAN_R1', '') }],
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
