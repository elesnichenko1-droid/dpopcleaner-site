const toast = document.getElementById('toast');
let toastTimer;
let currentHash = '';

const siteConfig = {
  supportEmail: 'elesnichenko1@gmail.com',
  licensePurchaseUrl: String(globalThis.DPopSiteConfig?.licensePurchaseUrl || '').trim()
};

function showToast(message){
  if(!toast) return;
  toast.textContent = message;
  toast.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => toast.classList.remove('show'), 1900);
}

function formatBytes(bytes){
  if(!Number.isFinite(bytes) || bytes <= 0) return 'сборка готовится';
  const mb = bytes / 1024 / 1024;
  return `${mb.toFixed(mb >= 10 ? 1 : 2)} МБ`;
}

function applyRevision(m){
  if(!m) return;
  const revision = Number(m.revision);
  if(!Number.isInteger(revision) || revision < 1) return;
  const label = `rev.${revision}`;
  document.title = document.title.replace(/rev\.\d+/gi, label);
  document.querySelectorAll('meta[content]').forEach(meta => {
    if(/rev\.\d+/i.test(meta.content)) meta.content = meta.content.replace(/rev\.\d+/gi, label);
  });
  const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
  const nodes = [];
  while(walker.nextNode()) nodes.push(walker.currentNode);
  nodes.forEach(node => {
    if(/rev\.\d+/i.test(node.nodeValue || '')) node.nodeValue = node.nodeValue.replace(/rev\.\d+/gi, label);
  });
}

function setDownloadState(available, url, version){
  document.querySelectorAll('.js-download').forEach(el => {
    const label = el.querySelector('.js-download-label');
    if(available && url){
      el.href = url;
      el.classList.remove('is-disabled');
      el.removeAttribute('aria-disabled');
      el.removeAttribute('data-disabled');
      if(label) label.textContent = el.classList.contains('button-small') ? 'Скачать' : `Скачать ${version}`;
    }else{
      el.href = '#download';
      el.classList.add('is-disabled');
      el.setAttribute('aria-disabled','true');
      el.setAttribute('data-disabled','true');
      if(label) label.textContent = el.classList.contains('button-small') ? 'Скоро' : 'Сборка готовится';
    }
  });

  const status = document.getElementById('releaseStatus');
  if(status){
    status.textContent = available
      ? `DPopCleaner ${version} опубликован и готов к загрузке.`
      : `DPopCleaner ${version || ''} проходит проверку. Кнопка скачивания включится после получения корректного стабильного manifest.`.trim();
    status.classList.toggle('ready', !!available);
  }
}

function applyManifest(m){
  const available = globalThis.DPopReleaseManifest?.isUsableManifest(m) === true;
  const currentVersion = document.querySelector('.js-version')?.textContent?.trim() || '';
  const version = available && typeof m.version === 'string' ? m.version : currentVersion;
  if(available) applyRevision(m);
  if(version){
    document.querySelectorAll('.js-version').forEach(el => el.textContent = version);
  }
  document.querySelectorAll('.js-size').forEach(el => el.textContent = formatBytes(available ? Number(m.size) : 0));
  setDownloadState(available, available ? m.download_url : '', version);
  if(available){
    currentHash = m.sha256.toLowerCase();
    const hv = document.getElementById('hashValue');
    if(hv) hv.textContent = currentHash;
  }else{
    currentHash = '';
    const hv = document.getElementById('hashValue');
    if(hv) hv.textContent = 'появится после получения стабильного manifest';
  }
}

async function loadManifest(){
  try{
    const response = await fetch(`./update/stable.json?t=${Date.now()}`, {cache:'no-store'});
    if(!response.ok) throw new Error(`HTTP ${response.status}`);
    applyManifest(await response.json());
  }catch(err){
    console.warn('Stable release manifest is not available yet:', err);
    applyManifest(null);
  }
}

async function copyHash(){
  const value = currentHash || document.getElementById('hashValue')?.textContent || '';
  if(!/^[a-f0-9]{64}$/i.test(value)){
    showToast('Хеш появится после проверки релиза');
    return;
  }
  try{
    await navigator.clipboard.writeText(value);
  }catch{
    const ta = document.createElement('textarea');
    ta.value = value;
    document.body.appendChild(ta);
    ta.select();
    document.execCommand('copy');
    ta.remove();
  }
  showToast('SHA-256 скопирован');
}

function isValidHttpUrl(value){
  try{
    const url = new URL(value);
    return url.protocol === 'https:' || url.protocol === 'http:';
  }catch{
    return false;
  }
}

function isValidSupportEmail(value){
  return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(value || '');
}

function buildLicenseMailto(){
  const subject = 'Покупка лицензии DPopCleaner';
  const body = [
    'Здравствуйте!',
    '',
    'Хочу приобрести лицензию DPopCleaner.',
    'Пожалуйста, пришлите информацию о стоимости, способе оплаты и получении ключа.',
    '',
    `Версия на сайте: ${document.querySelector('.js-version')?.textContent?.trim() || 'не определена'}`
  ].join('\r\n');
  return `mailto:${siteConfig.supportEmail}?subject=${encodeURIComponent(subject)}&body=${encodeURIComponent(body)}`;
}

function applyContactConfig(){
  document.querySelectorAll('.js-support-email').forEach(el => {
    el.textContent = siteConfig.supportEmail;
  });

  document.querySelectorAll('.js-license-buy').forEach(link => {
    if(isValidHttpUrl(siteConfig.licensePurchaseUrl)){
      link.href = siteConfig.licensePurchaseUrl;
      link.target = '_blank';
      link.rel = 'noopener noreferrer';
      link.classList.remove('is-disabled');
      link.removeAttribute('aria-disabled');
      link.removeAttribute('data-disabled');
      return;
    }

    if(isValidSupportEmail(siteConfig.supportEmail)){
      link.href = buildLicenseMailto();
      link.removeAttribute('target');
      link.removeAttribute('rel');
      link.classList.remove('is-disabled');
      link.removeAttribute('aria-disabled');
      link.removeAttribute('data-disabled');
      return;
    }

    link.href = '#license';
    link.classList.add('is-disabled');
    link.setAttribute('aria-disabled','true');
    link.setAttribute('data-disabled','true');
  });
}

function buildSupportMailto(name, email, message){
  const subject = `Поддержка DPopCleaner — ${name}`;
  const body = [
    `Имя: ${name}`,
    `Email для ответа: ${email}`,
    '',
    'Описание:',
    message,
    '',
    `Версия на сайте: ${document.querySelector('.js-version')?.textContent?.trim() || 'не определена'}`,
    `Страница: ${location.href}`
  ].join('\r\n');
  return `mailto:${siteConfig.supportEmail}?subject=${encodeURIComponent(subject)}&body=${encodeURIComponent(body)}`;
}

function setupSupportForm(){
  const form = document.getElementById('supportForm');
  if(!form) return;
  form.addEventListener('submit', event => {
    event.preventDefault();
    if(!form.reportValidity()) return;
    if(!isValidSupportEmail(siteConfig.supportEmail)){
      showToast('Почта поддержки недоступна');
      return;
    }
    const name = document.getElementById('supportName')?.value.trim() || '';
    const email = document.getElementById('supportEmail')?.value.trim() || '';
    const message = document.getElementById('supportMessage')?.value.trim() || '';
    location.href = buildSupportMailto(name, email, message);
  });
}

document.querySelectorAll('.js-download').forEach(el => el.addEventListener('click', e => {
  if(el.dataset.disabled === 'true'){
    e.preventDefault();
    document.getElementById('download')?.scrollIntoView({behavior:'smooth', block:'center'});
  }
}));

document.querySelectorAll('.js-license-buy').forEach(el => el.addEventListener('click', e => {
  if(el.dataset.disabled === 'true'){
    e.preventDefault();
    showToast('Покупка лицензии временно недоступна');
  }
}));

document.getElementById('copyHash')?.addEventListener('click', copyHash);
document.getElementById('copyHash2')?.addEventListener('click', copyHash);

const header = document.querySelector('.site-header');
addEventListener('scroll', () => header?.classList.toggle('scrolled', scrollY > 12), {passive:true});

const observer = new IntersectionObserver(entries => {
  for(const entry of entries){
    if(entry.isIntersecting){
      entry.target.classList.add('visible');
      observer.unobserve(entry.target);
    }
  }
}, {threshold:.12});
document.querySelectorAll('.reveal').forEach(el => observer.observe(el));

const menuButton = document.querySelector('.menu-button');
const nav = document.querySelector('.nav');
menuButton?.addEventListener('click', () => {
  const open = nav.classList.toggle('open');
  menuButton.setAttribute('aria-expanded', String(open));
});
nav?.querySelectorAll('a').forEach(a => a.addEventListener('click', () => {
  nav.classList.remove('open');
  menuButton?.setAttribute('aria-expanded','false');
}));

applyContactConfig();
setupSupportForm();
loadManifest();
