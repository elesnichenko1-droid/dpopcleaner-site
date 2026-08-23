(function(root, factory){
  const api = factory();
  if(typeof module === 'object' && module.exports) module.exports = api;
  if(root) root.DPopReleaseManifest = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function(){
  const r4Url = /^https:\/\/github\.com\/elesnichenko1-droid\/dpopcleaner-site\/releases\/download\/v0\.3\.1-beta-r4\/DPopCleaner_Setup_0\.3\.1_BETA_R4\.exe$/;
  function hasArtifactMetadata(m){
    return Boolean(m && m.available === true && typeof m.signed === 'boolean' &&
      typeof m.download_url === 'string' && Number.isFinite(Number(m.size)) &&
      Number(m.size) > 0 && typeof m.sha256 === 'string' && /^[a-f0-9]{64}$/i.test(m.sha256));
  }
  function isUsableR4Manifest(m){
    return Boolean(hasArtifactMetadata(m) && m.product === 'DPopCleaner' && m.channel === 'beta' &&
      m.version === '0.3.1' && Number(m.version_code) === 3014 && Number(m.revision) === 4 &&
      r4Url.test(m.download_url));
  }
  function resolveDownloadManifest(candidate){ return isUsableR4Manifest(candidate) ? candidate : null; }
  return {fallbackManifest:null, hasArtifactMetadata, isUsableManifest:isUsableR4Manifest,
    isUsableCurrentManifest:isUsableR4Manifest, isUsableR4Manifest, resolveDownloadManifest};
});
