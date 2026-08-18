// ========================================
// 配置
// ========================================
var CONFIG = {
    BACKEND_URL: 'http://www.baidu.com',
    REDIRECT_PATH: '/',
    REDIRECT_DELAY: 5000,
};
// ========================================
// 页面加载完成后自动扫描 WiFi
// ========================================
document.addEventListener('DOMContentLoaded', function () {
    setTimeout(function () {
        scan_wifi();
    }, 500);
});

// ========================================
// 状态栏管理
// ========================================
function setStatus(text, type) {
    const statusBar = document.getElementById('statusBar');
    const statusText = document.getElementById('statusText');
    statusText.textContent = text;
    statusBar.className = 'status-bar';
    if (type) {
        statusBar.classList.add('status-' + type);
    } else {
        statusBar.classList.add('status-idle');
    }
}

// ========================================
// 显示消息
// ========================================
function showMessage(text, type) {
    const msg = document.getElementById('msg');
    msg.textContent = text;
    msg.className = 'message show ' + type;
    clearTimeout(msg._timer);
    msg._timer = setTimeout(function () {
        msg.className = 'message';
        msg.textContent = '';
    }, 5000);
}

// ========================================
// 扫描 WiFi
// ========================================
function scan_wifi() {
    const btn = document.getElementById('scanBtn');
    const list = document.getElementById('wifi_list');
    const hint = document.getElementById('scanningHint');

    btn.disabled = true;
    btn.innerHTML = '<span class="spinner" style="width:18px;height:18px;border-width:2px;"></span> 扫描中...';
    setStatus('正在扫描附近 WiFi...', 'scanning');
    hint.classList.remove('hidden');
    list.innerHTML = '';

    fetch('/scan')
        .then(res => {
            if (!res.ok) throw new Error('扫描失败: ' + res.status);
            return res.json();
        })
        .then(data => {
            hint.classList.add('hidden');
            if (!data || data.length === 0) {
                list.innerHTML = '<option value="">未发现 WiFi 网络</option>';
                setStatus('未发现 WiFi，请检查设备', 'error');
                return;
            }
            data.sort((a, b) => b.rssi - a.rssi);
            list.innerHTML = '';
            data.forEach((wifi) => {
                const option = document.createElement('option');
                option.value = wifi.ssid;
                let signalIcon = '📶';
                if (wifi.rssi > -50) signalIcon = '📶📶📶';
                else if (wifi.rssi > -65) signalIcon = '📶📶';
                else if (wifi.rssi > -80) signalIcon = '📶';
                // option.text = signalIcon + ' ' + wifi.ssid + ' (' + wifi.rssi + 'dBm)';
                option.text = signalIcon + ' ' + wifi.ssid;
                list.appendChild(option);
            });
            setStatus('发现 ' + data.length + ' 个 WiFi 网络，请选择', 'success');
            if (data.length === 1) {
                list.options[0].selected = true;
                document.getElementById('ssid').value = data[0].ssid;
            }
        })
        .catch(err => {
            console.error(err);
            hint.classList.add('hidden');
            list.innerHTML = '<option value="">扫描失败，请重试</option>';
            setStatus('扫描失败: ' + err.message, 'error');
            showMessage('WiFi 扫描失败，请重试', 'error');
        })
        .finally(() => {
            btn.disabled = false;
            btn.innerHTML = '<span class="btn-icon">🔄</span> 扫描 WiFi';
        });
}

// ========================================
// 选择 WiFi
// ========================================
function select_wifi() {
    const list = document.getElementById('wifi_list');
    const selected = list.options[list.selectedIndex];
    if (selected && selected.value) {
        document.getElementById('ssid').value = selected.value;
        document.getElementById('password').focus();
        setStatus('已选择: ' + selected.value, 'success');
    }
}

// ========================================
// 连接 WiFi
// ========================================
let statusPollTimer = null;

function connect_wifi() {
    const ssid = document.getElementById('ssid').value.trim();
    const password = document.getElementById('password').value;
    const btn = document.getElementById('connectBtn');

    if (!ssid) {
        showMessage('请输入或选择 WiFi 名称', 'error');
        document.getElementById('ssid').focus();
        return;
    }
    if (!password || password.length < 8) {
        showMessage('WiFi 密码至少需要 8 位', 'error');
        document.getElementById('password').focus();
        return;
    }

    // 清除之前的轮询
    if (statusPollTimer) {
        clearInterval(statusPollTimer);
        statusPollTimer = null;
    }

    btn.disabled = true;
    btn.innerHTML = '<span class="spinner"></span> 连接中...';
    setStatus('正在发送配置并连接 ' + ssid + '...', 'connecting');

    fetch('/wifi_config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid: ssid, password: password })
    })
        .then(res => {
            if (!res.ok) throw new Error('请求失败: ' + res.status);
            return res.json();
        })
        .then(data => {
            if (data.status === 'connecting') {
                showMessage('⏳ ' + data.msg, 'success');
                setStatus('设备正在连接 WiFi，请稍候...', 'connecting');
                // 开始轮询真实状态
                startStatusPolling();
            } else {
                throw new Error(data.msg || '未知错误');
            }
        })
        .catch(err => {
            console.error('连接错误:', err);
            showMessage('❌ ' + err.message, 'error');
            setStatus('连接失败', 'error');
            btn.disabled = false;
            btn.innerHTML = '<span class="btn-icon">🚀</span> 连接 WiFi';
        });
}

// ========================================
// 轮询 WiFi 连接状态
// ========================================
function startStatusPolling() {
    let pollCount = 0;
    const maxPoll = 25; // 最多轮询 25 次 × 2 秒 = 50 秒
    const btn = document.getElementById('connectBtn');

    statusPollTimer = setInterval(() => {
        pollCount++;
        if (pollCount > maxPoll) {
            clearInterval(statusPollTimer);
            statusPollTimer = null;
            showMessage('⏰ 连接超时，请检查密码或信号后重试', 'error');
            setStatus('连接超时', 'error');
            btn.disabled = false;
            btn.innerHTML = '<span class="btn-icon">🚀</span> 连接 WiFi';
            return;
        }

        fetch('/wifi_status')
            .then(res => {
                if (!res.ok) throw new Error('status ' + res.status);
                return res.json();
            })
            .then(data => {
                if (data.status === 'connected') {
                    clearInterval(statusPollTimer);
                    statusPollTimer = null;

                    // ===== 连接成功 =====
                    showMessage('✅ WiFi 连接成功！IP: ' + (data.ip || '未知'), 'success');
                    setStatus('🎉 连接成功！', 'success');
                    // 注释掉下面这一段（倒计时跳转）
                    //    let countdown = 5;
                    //    const statusText = document.getElementById('statusText');
                    //    const timer = setInterval(() => {
                    //        countdown--;
                    //        if (countdown > 0) {
                    //            statusText.textContent = '🎉 连接成功！' + countdown + '秒后跳转...';
                    //        } else {
                    //            clearInterval(timer);
                    //            window.location.href = CONFIG.BACKEND_URL;
                    //        }
                    //    }, 1000);
                    btn.disabled = false;
                    btn.innerHTML = '<span class="btn-icon">🚀</span> 连接 WiFi';
                } else if (data.status === 'failed') {
                    clearInterval(statusPollTimer);
                    statusPollTimer = null;

                    // ===== 连接失败（密码错误等）=====
                    showMessage('❌ 连接失败: ' + (data.reason || '请检查密码'), 'error');
                    setStatus('连接失败，请重试', 'error');
                    btn.disabled = false;
                    btn.innerHTML = '<span class="btn-icon">🚀</span> 连接 WiFi';

                } else if (data.status === 'connecting') {
                    // 仍在连接中，继续轮询，UI 保持现状
                    setStatus('设备正在连接 WiFi... (' + pollCount + ')', 'connecting');
                }
            })
            .catch(err => {
                // 请求失败可能是 AP 已关闭（设备切换网络成功）
                console.warn('状态查询失败:', err);
                // 继续轮询几次，如果持续失败则可能是网络已切换
            });
    }, 2000);
}

// ========================================
// 密码显示切换
// ========================================
function togglePassword() {
    const input = document.getElementById('password');
    const btn = document.querySelector('.password-toggle');
    if (input.type === 'password') {
        input.type = 'text';
        btn.textContent = '🙈';
    } else {
        input.type = 'password';
        btn.textContent = '👁️';
    }
}

// ========================================
// 恢复出厂设置
// ========================================
function factoryReset() {
    if (!confirm('⚠️ 确定要恢复出厂设置吗？\n这将清除所有 WiFi 配置。')) {
        return;
    }
    setStatus('正在恢复出厂设置...', 'connecting');
    fetch('/factory_reset', { method: 'GET' })
        .then(response => {
            if (!response.ok) throw new Error('恢复失败: ' + response.status);
            return response.text();
        })
        .then(data => {
            showMessage('✅ 恢复完成，请重新配置 WiFi', 'success');
            setStatus('已恢复出厂设置', 'success');
            document.getElementById('ssid').value = '';
            document.getElementById('password').value = '';
            document.getElementById('wifi_list').innerHTML = '<option value="">请重新扫描 WiFi</option>';
            setTimeout(function () { scan_wifi(); }, 3000);
        })
        .catch(err => {
            console.error(err);
            showMessage('恢复失败: ' + err.message, 'error');
            setStatus('恢复失败', 'error');
        });
}

// ========================================
// 键盘快捷键：回车键触发连接
// ========================================
document.addEventListener('keydown', function (e) {
    if (e.key === 'Enter') {
        const active = document.activeElement;
        if (active.id === 'ssid' || active.id === 'password') {
            connect_wifi();
        }
    }
});

// ========================================
// 输入框自动补全提示
// ========================================
document.getElementById('ssid').addEventListener('input', function () {
    if (this.value) {
        setStatus('准备连接: ' + this.value, 'idle');
    }
});