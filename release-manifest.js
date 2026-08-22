(function(root, factory){
  const api = factory();
  if(typeof module === 'object' && module.exports) module.exports = api;
  if(root) root.DPopReleaseManifest = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function(){
  const fallbackManifest = Object.freeze({
    product: 'DPopCleaner',
    channel: 'beta',
    version: '0.2.14',
    version_code: 214,
    revision: 1,
    mandatory: false,
    download_url: 'https://github.com/elesnichenko1-droid/dpopcleaner-site/releases/download/v0.2.14-clean-r1/DPopCleaner_Setup_0.2.14_BETA_CLEAN_R1.exe',
    sha256: 'dac34a1f1697dbc9f7f5a953ade9e1e4f09b39a04d66fb18e404ba511543900e',
    size: 2207759,
    signed: false,
    available: true,
    notes_url: 'https://elesnichenko1-droid.github.io/dpopcleaner-site/',
    install_args: '/SILENT /NORESTART',
  });

  const fallbackUrl = /^https:\/\/github\.com\/elesnichenko1-droid\/dpopcleaner-site\/releases\/download\/v0\.2\.14-clean-r1\/DPopCleaner_Setup_0\.2\.14_BETA_CLEAN_R1\.exe$/;
  const r3Url = /^https:\/\/github\.com\/elesnichenko1-droid\/dpopcleaner-site\/releases\/download\/v0\.3\.1-beta-r3\/DPopCleaner_Setup_0\.3\.1_BETA_R3\.exe$/;

  function hasArtifactMetadata(m){
    return Boolean(
      m &&
      m.available === true &&
      typeof m.signed === 'boolean' &&
      typeof m.download_url === 'string' &&
      Number.isFinite(Number(m.size)) &&
      Number(m.size) > 0 &&
      typeof m.sha256 === 'string' &&
      /^[a-f0-9]{64}$/i.test(m.sha256)
    );
  }

  function isUsableManifest(m){
    return Boolean(
      hasArtifactMetadata(m) &&
      m.product === 'DPopCleaner' &&
      m.channel === 'beta' &&
      m.version === '0.2.14' &&
      Number(m.version_code) === 214 &&
      Number(m.revision) === 1 &&
      fallbackUrl.test(m.download_url)
    );
  }

  function isUsableR3Manifest(m){
    return Boolean(
      hasArtifactMetadata(m) &&
      m.product === 'DPopCleaner' &&
      m.channel === 'beta' &&
      m.version === '0.3.1' &&
      Number(m.version_code) === 3013 &&
      Number(m.revision) === 3 &&
      r3Url.test(m.download_url)
    );
  }

  function resolveDownloadManifest(candidate){
    return isUsableR3Manifest(candidate) ? candidate : fallbackManifest;
  }

  return { fallbackManifest, isUsableManifest, isUsableR3Manifest, resolveDownloadManifest };
});
