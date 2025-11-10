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
    const MAX_DURATION = 25000; // 25s chờ tối đa
    const INTERVAL = 1000;      // 1s/poll
    const MAX_FAIL_COUNT = 3;   // fail liên tiếp 3 lần mới coi là thật
    let finished = false;
    let failCount = 0;
    let lastResponseOk = false; // ESP có từng phản hồi thành công không?
    const startTime = Date.now();

    // Đợi ESP khởi động (tránh fetch quá sớm)
    await new Promise(r => setTimeout(r, 1500));

    while (!finished && Date.now() - startTime < MAX_DURATION) {
      try {
        // 🧠 Gửi request /status
        const res = await fetch('/status?_=' + Date.now(), { cache: 'no-store' });
        const s = await res.json();

        lastResponseOk = true; // Đã nhận được phản hồi => ESP vẫn đang chạy server
        console.log('[ESP]', s);

        // ✅ Nếu ESP báo "connected" rõ ràng
        if (s.state === 'connected' && s.wifi_status === 3) {
          finished = true;
          clearInterval(fakeTimer);
          console.log('✅ ESP báo connected (trực tiếp)');
          fakeProgressTo100(bar, txt, 3000, () => showSuccess(email));
          return;
        }

        // ⚠️ Nếu ESP báo "failed" => tăng đếm
        if (s.state === 'failed') failCount++;
        else failCount = 0;

        // Nếu thất bại 3 lần liên tiếp => coi là lỗi thật
        if (failCount >= MAX_FAIL_COUNT) {
          finished = true;
          clearInterval(fakeTimer);
          console.log('❌ ESP báo failed 3 lần liên tiếp');
          return handleConnectFail();
        }

        // Nếu vẫn đang connecting, fake tiến độ
        if (s.state === 'connecting') {
          fakeProgressStep(bar, txt, 5);
        }

      } catch (err) {
        console.warn('⚠️ Không fetch được /status:', err.message);

        // ✅ ESP từng phản hồi => giờ không phản hồi => server đã tắt => thành công!
        if (lastResponseOk) {
          finished = true;
          clearInterval(fakeTimer);
          console.log('✅ ESP tắt WebServer → kết nối thành công');
          fakeProgressTo100(bar, txt, 3000, () => showSuccess(email));
          return;
        }

        // ❌ Nếu chưa từng phản hồi được lần nào → có thể ESP chưa khởi động xong
        // => bỏ qua và thử lại
      }

      // Đợi 1 giây rồi tiếp tục poll
      await new Promise(r => setTimeout(r, INTERVAL));
    }

    // ⏱️ Nếu hết thời gian mà vẫn không connected => fail
    if (!finished) {
      finished = true;
      clearInterval(fakeTimer);
      console.log('⏱️ Timeout: ESP không phản hồi đủ lâu');
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