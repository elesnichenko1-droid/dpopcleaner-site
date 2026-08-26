(function(root, factory){
  const api = factory();
  if(typeof module === 'object' && module.exports) module.exports = api;
  if(root) root.DPopReleaseManifest = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function(){
  // Stable publication contract for DPopCleaner 0.4.17 rev.2.
  // Trigger verified live-site publication from a clean PR branch.
  const releaseUrl = /^https:\/\/github\.com\/elesnichenko1-droid\/dpopcleaner-site\/releases\/download\/v0\.4\.17\/DPopCleaner_Setup_0\.4\.17\.exe$/;

  function isUsableManifest(m){
    return Boolean(
      m &&
      m.version === '0.4.17' &&
      m.channel === 'stable' &&
      Number(m.revision) === 2 &&
      m.available === true &&
      typeof m.download_url === 'string' &&
      releaseUrl.test(m.download_url) &&
      Number.isFinite(Number(m.size)) &&
      Number(m.size) > 0 &&
      typeof m.sha256 === 'string' &&
      /^[a-f0-9]{64}$/i.test(m.sha256)
    );
  }

  return { isUsableManifest };
});
