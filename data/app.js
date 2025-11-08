let wifiInfo = {};
let progressTimer = null;

// ======== QUÉT DANH SÁCH WIFI =========
async function scanWifi() {
  const btn = document.getElementById('scanBtn');
  btn.disabled = true;
  const ul = document.getElementById('wifiList');
  ul.innerHTML = `
    <div class="loader">
      <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200">
        <circle fill="#FF156D" stroke="#FF156D" stroke-width="15" r="15" cx="35" cy="100">
          <animate attributeName="cx" calcMode="spline" dur="2"
            values="35;165;165;35;35"
            keySplines="0 .1 .5 1;0 .1 .5 1;0 .1 .5 1;0 .1 .5 1"
            repeatCount="indefinite"></animate>
        </circle>
      </svg>
      <span>Đang tìm kiếm mạng Wi-Fi...</span>
    </div>
  `;

  try {
    const res = await fetch('/scan');
    const list = await res.json();

    ul.innerHTML = '';
    if (!Array.isArray(list) || list.length === 0) {
      ul.innerHTML = '<li>Không tìm thấy mạng Wi-Fi nào</li>';
    } else {
      list.forEach(item => {
        if (!item.ssid || item.ssid.startsWith("WIFI_SETUP_CONGMINHAUDIO")) return; // loại bỏ chính ESP
        const li = document.createElement('li');
        li.innerHTML = `
          <button class="button-wifi" data-ssid="${item.ssid}">
            <div class="wifi avg-${item.avg}">
              <span class="bar"></span><span class="bar"></span>
              <span class="bar"></span><span class="bar"></span>
            </div>
            <span>${item.ssid}</span>
          </button>`;
        li.querySelector('button').onclick = () => openWifiModal(item.ssid);
        ul.appendChild(li);
      });
    }
  } catch (e) {
    alert('❌ Lỗi khi quét Wi-Fi');
  }

  btn.disabled = false;
  btn.innerText = "🔍 Quét Wi-Fi";
}

document.getElementById('scanBtn').onclick = scanWifi;

// ======== HỘP THOẠI NHẬP MẬT KHẨU =========
const modal = document.getElementById('wifiModal');
const wifiTitle = document.getElementById('wifiTitle');
const wifiPassword = document.getElementById('wifiPassword');

function openWifiModal(ssid) {
  wifiInfo.ssid = ssid;
  wifiTitle.innerText = `🔒 ${ssid}`;
  wifiPassword.value = '';
  modal.classList.remove('hidden');
}

document.getElementById('cancelModal').onclick = () => modal.classList.add('hidden');
document.getElementById('confirmWifi').onclick = () => {
  wifiInfo.pass = wifiPassword.value.trim();
  modal.classList.add('hidden');
  document.getElementById('panel-setup').classList.add('hidden');
  document.getElementById('panel-email').classList.remove('hidden');
};

// ======== KẾT NỐI VÀ THEO DÕI TRẠNG THÁI =========
document.getElementById('connectBtn').onclick = async () => {
  const email = document.getElementById('email').value.trim();
  const ssid = wifiInfo.ssid;
  const pass = wifiInfo.pass;
  const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;

  if (!email || !emailRegex.test(email)) {
    alert("Email không hợp lệ!");
    return;
  }

  wifiInfo.email = email;
  document.getElementById('panel-email').classList.add('hidden');
  document.getElementById('panel-progress').classList.remove('hidden');

  await fetch('/start_connect', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: `ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}&email=${encodeURIComponent(email)}`
  });

  const bar = document.getElementById('bar');
  const txt = document.getElementById('progressText');
  let progress = 0;
  let finished = false;
  const startTime = Date.now();

  // 1️⃣ progress chạy ảo 0 → 70%
  const fakeTimer = setInterval(() => {
    if (progress < 70) {
      progress += Math.random() * 2;
      bar.style.width = progress + '%';
      txt.innerText = Math.floor(progress) + '%';
    }
  }, 150);

  // 2️⃣ Poll trạng thái thật
  const pollStatus = async () => {
    while (!finished && Date.now() - startTime < 10000) {
      try {
        const res = await fetch('/status');
        const s = await res.json();

        // Nếu ESP trả trạng thái thật
        if (s.state === 'connected' && s.wifi_status === 3) {
          finished = true;
          clearInterval(fakeTimer);
          fakeProgressTo100(bar, txt, 3000, () => showSuccess(email));
          return;
        }

        if (s.state === 'failed' || s.wifi_status !== 3) {
          finished = true;
          clearInterval(fakeTimer);
          return handleConnectFail();
        }
      } catch (e) {
        // có thể ESP đang restart, bỏ qua tạm
      }
      await new Promise(r => setTimeout(r, 1000));
    }

    // Timeout không phản hồi
    if (!finished) {
      finished = true;
      clearInterval(fakeTimer);
      handleConnectFail();
    }
  };

  pollStatus();
};

// ======== FAKE PHẦN CÒN LẠI 70 → 100 =========
function fakeProgressTo100(bar, txt, duration = 3000, onComplete) {
  let progress = parseFloat(bar.style.width) || 70;
  const start = performance.now();
  const animate = (t) => {
    const elapsed = t - start;
    const ratio = Math.min(elapsed / duration, 1);
    const val = progress + (100 - progress) * ratio;
    bar.style.width = val + '%';
    txt.innerText = Math.floor(val) + '%';
    if (ratio < 1) requestAnimationFrame(animate);
    else if (onComplete) onComplete();
  };
  requestAnimationFrame(animate);
}

// ======== KẾT NỐI THẤT BẠI =========
function handleConnectFail() {
  document.getElementById('panel-progress').classList.add('hidden');
  document.getElementById('panel-setup').classList.remove('hidden');
  alert('❌ Kết nối Wi-Fi thất bại, vui lòng thử lại!');
  scanWifi();
}

// ======== KẾT NỐI THÀNH CÔNG =========
function showSuccess(email) {
  document.getElementById('panel-progress').classList.add('hidden');
  document.getElementById('panel-result').classList.remove('hidden');
  document.getElementById('successMsg').innerHTML = `
    Thiết bị đã được thêm vào hệ thống quản lý từ xa.<br>
    Hãy kiểm tra hộp thư trong email <b>${email}</b> để tiếp tục cài đặt.
  `;
}