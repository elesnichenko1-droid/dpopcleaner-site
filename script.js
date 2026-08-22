const toast = document.getElementById('toast');
let toastTimer;
let currentHash = '';

function formatBytes(bytes){
  if(!Number.isFinite(bytes) || bytes <= 0) return 'размер уточняется';
  const mb = bytes / 1024 / 1024;
  return `${mb.toFixed(mb >= 10 ? 1 : 2)} МБ`;
}

function applyManifest(m){
  if(!m || !m.version || !m.download_url) return;
  document.querySelectorAll('.js-version').forEach(el => el.textContent = m.version);
  document.querySelectorAll('.js-size').forEach(el => el.textContent = formatBytes(Number(m.size)));
  document.querySelectorAll('.js-download').forEach(el => el.href = m.download_url);
  if(typeof m.sha256 === 'string' && /^[a-f0-9]{64}$/i.test(m.sha256)){
    currentHash = m.sha256.toLowerCase();
    const hv = document.getElementById('hashValue');
    if(hv) hv.textContent = currentHash;
  }
}

async function loadManifest(){
  try{
    const response = await fetch('./update/beta.json', {cache:'no-store'});
    if(!response.ok) throw new Error(`HTTP ${response.status}`);
    applyManifest(await response.json());
  }catch(err){
    console.warn('Update manifest is not available yet:', err);
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
