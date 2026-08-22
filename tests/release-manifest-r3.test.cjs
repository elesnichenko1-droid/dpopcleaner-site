const test = require('node:test');
const assert = require('node:assert/strict');
const {
  fallbackManifest,
  isUsableR3Manifest,
  resolveDownloadManifest,
} = require('../release-manifest.js');

const validR3 = Object.freeze({
  product: 'DPopCleaner',
  channel: 'beta',
  version: '0.3.1',
  version_code: 3013,
  revision: 3,
  mandatory: false,
  available: true,
  signed: false,
  download_url: 'https://github.com/elesnichenko1-droid/dpopcleaner-site/releases/download/v0.3.1-beta-r3/DPopCleaner_Setup_0.3.1_BETA_R3.exe',
  sha256: 'b'.repeat(64),
  size: 123456,
  notes_url: 'https://elesnichenko1-droid.github.io/dpopcleaner-site/',
  install_args: '/SILENT /NORESTART',
});

test('accepts the exact R3 release contract', () => {
  assert.equal(isUsableR3Manifest(validR3), true);
  assert.deepEqual(resolveDownloadManifest(validR3), validR3);
});

const mutations = [
  ['missing manifest', null],
  ['unavailable release', { ...validR3, available: false }],
  ['wrong product', { ...validR3, product: 'Other' }],
  ['wrong channel', { ...validR3, channel: 'stable' }],
  ['wrong version', { ...validR3, version: '0.3.2' }],
  ['wrong version code', { ...validR3, version_code: 3012 }],
  ['wrong revision', { ...validR3, revision: 2 }],
  ['HTTP URL', { ...validR3, download_url: validR3.download_url.replace('https://', 'http://') }],
  ['wrong release tag', { ...validR3, download_url: validR3.download_url.replace('v0.3.1-beta-r3', 'v0.3.1-beta') }],
  ['wrong asset name', { ...validR3, download_url: validR3.download_url.replace('_R3.exe', '.exe') }],
  ['zero size', { ...validR3, size: 0 }],
  ['non-numeric size', { ...validR3, size: 'unknown' }],
  ['short SHA-256', { ...validR3, sha256: 'b'.repeat(63) }],
  ['non-hex SHA-256', { ...validR3, sha256: 'z'.repeat(64) }],
  ['non-boolean signature flag', { ...validR3, signed: 'false' }],
];

for (const [name, manifest] of mutations) {
  test(`falls back to verified 0.2.14 for ${name}`, () => {
    assert.equal(isUsableR3Manifest(manifest), false);
    assert.deepEqual(resolveDownloadManifest(manifest), fallbackManifest);
    assert.equal(resolveDownloadManifest(manifest).sha256,
      'dac34a1f1697dbc9f7f5a953ade9e1e4f09b39a04d66fb18e404ba511543900e');
  });
}
