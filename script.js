const HASH = '0b0e7eeec6fe7a28eb046b083700bf7ce94ab4b0ee17bba9ee355203e8c8983e';
const toast = document.getElementById('toast');
let toastTimer;
async function copyHash(){
  try{await navigator.clipboard.writeText(HASH)}catch{const ta=document.createElement('textarea');ta.value=HASH;document.body.appendChild(ta);ta.select();document.execCommand('copy');ta.remove()}
  toast.classList.add('show');clearTimeout(toastTimer);toastTimer=setTimeout(()=>toast.classList.remove('show'),1700);
}
document.getElementById('copyHash')?.addEventListener('click',copyHash);
document.getElementById('copyHash2')?.addEventListener('click',copyHash);
const header=document.querySelector('.site-header');
addEventListener('scroll',()=>header.classList.toggle('scrolled',scrollY>12),{passive:true});
const observer=new IntersectionObserver((entries)=>{for(const e of entries){if(e.isIntersecting){e.target.classList.add('visible');observer.unobserve(e.target)}}},{threshold:.12});
document.querySelectorAll('.reveal').forEach(el=>observer.observe(el));
const menuButton=document.querySelector('.menu-button');const nav=document.querySelector('.nav');
menuButton?.addEventListener('click',()=>{const open=nav.classList.toggle('open');menuButton.setAttribute('aria-expanded',String(open))});
nav?.querySelectorAll('a').forEach(a=>a.addEventListener('click',()=>{nav.classList.remove('open');menuButton?.setAttribute('aria-expanded','false')}));
