const toast = document.getElementById('toast');
let toastTimer;
let currentHash = '';

function formatBytes(bytes){
  if(!Number.isFinite(bytes) || bytes <= 0) return 'сборка готовится';
  const mb = bytes / 1024 / 1024;
  return `${mb.toFixed(mb >= 10 ? 1 : 2)} МБ`;
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
      el.href = '#release-status';
      el.classList.add('is-disabled');
      el.setAttribute('aria-disabled','true');
      el.setAttribute('data-disabled','true');
      if(label) label.textContent = el.classList.contains('button-small') ? 'Скоро' : 'Сборка готовится';
    }
  });
  const status=document.getElementById('releaseStatus');
  if(status){
    status.textContent = available
      ? `DPopCleaner ${version} опубликован и готов к загрузке.`
      : `DPopCleaner ${version} проходит финальную проверку на GitHub. Кнопка скачивания включится автоматически после публикации проверенного manifest.`;
    status.classList.toggle('ready', !!available);
  }
}

function applyManifest(m){
  const available = globalThis.DPopReleaseManifest?.isUsableManifest(m) === true;
  const version = '0.4.17';
  document.querySelectorAll('.js-version').forEach(el => el.textContent = version);
  document.querySelectorAll('.js-size').forEach(el => el.textContent = formatBytes(available ? Number(m.size) : 0));
  setDownloadState(available, available ? m.download_url : '', version);
  if(available){
    currentHash = m.sha256.toLowerCase();
    const hv = document.getElementById('hashValue');
    if(hv) hv.textContent = currentHash;
  }else{
    currentHash='';
    const hv = document.getElementById('hashValue');
    if(hv) hv.textContent = 'появится автоматически после финальной сборки';
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
    if(toast){ toast.textContent='Хеш появится после сборки'; toast.classList.add('show'); clearTimeout(toastTimer); toastTimer=setTimeout(()=>toast.classList.remove('show'),1700); }
    return;
  }
  try{await navigator.clipboard.writeText(value)}catch{const ta=document.createElement('textarea');ta.value=value;document.body.appendChild(ta);ta.select();document.execCommand('copy');ta.remove()}
  if(toast){ toast.textContent='SHA-256 скопирован'; toast.classList.add('show'); clearTimeout(toastTimer); toastTimer=setTimeout(()=>toast.classList.remove('show'),1700); }
}

document.querySelectorAll('.js-download').forEach(el => el.addEventListener('click', e => {
  if(el.dataset.disabled === 'true'){
    e.preventDefault();
    document.getElementById('release-status')?.scrollIntoView({behavior:'smooth', block:'center'});
  }
}));
document.getElementById('copyHash')?.addEventListener('click',copyHash);
document.getElementById('copyHash2')?.addEventListener('click',copyHash);
const header=document.querySelector('.site-header');
addEventListener('scroll',()=>header?.classList.toggle('scrolled',scrollY>12),{passive:true});
const observer=new IntersectionObserver((entries)=>{for(const e of entries){if(e.isIntersecting){e.target.classList.add('visible');observer.unobserve(e.target)}}},{threshold:.12});
document.querySelectorAll('.reveal').forEach(el=>observer.observe(el));
const menuButton=document.querySelector('.menu-button');const nav=document.querySelector('.nav');
menuButton?.addEventListener('click',()=>{const open=nav.classList.toggle('open');menuButton.setAttribute('aria-expanded',String(open))});
nav?.querySelectorAll('a').forEach(a=>a.addEventListener('click',()=>{nav.classList.remove('open');menuButton?.setAttribute('aria-expanded','false')}));
loadManifest();
