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
    if (statusText) {
        statusText.textContent = text;
    }
    if (statusBar) {
        statusBar.className = 'status-bar';
        if (type) {
            statusBar.classList.add('status-' + type);
        } else {
            statusBar.classList.add('status-idle');
        }
    }
}

// ========================================
// 显示消息
// ========================================
function showMessage(text, type) {
    const msg = document.getElementById('msg');
    if (!msg) return;
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

    if (btn) {
        btn.disabled = true;
        btn.innerHTML = '<span class="spinner" style="width:18px;height:18px;border-width:2px;"></span> 扫描中...';
    }
    setStatus('正在扫描附近 WiFi...', 'scanning');
    
    if (hint) {
        hint.classList.remove('hidden');
    }
    if (list) {
        list.innerHTML = '';
    }

    fetch('/scan')
        .then(res => {
            if (!res.ok) throw new Error('扫描失败: ' + res.status);
            return res.json();
        })
        .then(data => {
            if (hint) {
                hint.classList.add('hidden');
            }
            if (!data || data.length === 0) {
                if (list) {
                    list.innerHTML = '<option value="">未发现 WiFi 网络</option>';
                }
                setStatus('未发现 WiFi，请检查设备', 'error');
                return;
            }
            data.sort((a, b) => b.rssi - a.rssi);
            if (list) {
                list.innerHTML = '';
                data.forEach((wifi) => {
                    const option = document.createElement('option');
                    option.value = wifi.ssid;
                    let signalIcon = '📶';
                    if (wifi.rssi > -50) signalIcon = '📶📶📶';
                    else if (wifi.rssi > -65) signalIcon = '📶📶';
                    else if (wifi.rssi > -80) signalIcon = '📶';
                    option.text = signalIcon + ' ' + wifi.ssid;
                    list.appendChild(option);
                });
            }
            setStatus('发现 ' + data.length + ' 个 WiFi 网络，请选择', 'success');
            if (data.length === 1) {
                if (list) {
                    list.options[0].selected = true;
                }
                document.getElementById('ssid').value = data[0].ssid;
            }
        })
        .catch(err => {
            console.error(err);
            if (hint) {
                hint.classList.add('hidden');
            }
            if (list) {
                list.innerHTML = '<option value="">扫描失败，请重试</option>';
            }
            setStatus('扫描失败: ' + err.message, 'error');
            showMessage('WiFi 扫描失败，请重试', 'error');
        })
        .finally(() => {
            if (btn) {
                btn.disabled = false;
                btn.innerHTML = '<span class="btn-icon">🔄</span> 扫描 WiFi';
            }
        });
}

// ========================================
// 选择 WiFi
// ========================================
function select_wifi() {
    const list = document.getElementById('wifi_list');
    const selected = list ? list.options[list.selectedIndex] : null;
    if (selected && selected.value) {
        document.getElementById('ssid').value = selected.value;
        document.getElementById('password').focus();
        setStatus('已选择: ' + selected.value, 'success');
    }
}

// ========================================
// 密码显示切换 ✅ 新增实现
// ========================================
function togglePassword() {
    const input = document.getElementById('password');
    const btn = document.querySelector('.password-toggle');
    if (!input) return;
    if (input.type === 'password') {
        input.type = 'text';
        if (btn) {
            btn.textContent = '🙈';
            btn.title = '隐藏密码';
        }
    } else {
        input.type = 'password';
        if (btn) {
            btn.textContent = '👁️';
            btn.title = '显示密码';
        }
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
    const maxPoll = 25;
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
                    showMessage('✅ WiFi 连接成功！IP: ' + (data.ip || '未知'), 'success');
                    setStatus('🎉 连接成功！', 'success');
                    btn.disabled = false;
                    btn.innerHTML = '<span class="btn-icon">🚀</span> 连接 WiFi';
                } else if (data.status === 'failed') {
                    clearInterval(statusPollTimer);
                    statusPollTimer = null;
                    showMessage('❌ 连接失败: ' + (data.reason || '请检查密码'), 'error');
                    setStatus('连接失败，请重试', 'error');
                    btn.disabled = false;
                    btn.innerHTML = '<span class="btn-icon">🚀</span> 连接 WiFi';
                } else if (data.status === 'connecting') {
                    setStatus('设备正在连接 WiFi... (' + pollCount + '/' + maxPoll + ')', 'connecting');
                }
            })
            .catch(err => {
                console.warn('状态查询失败:', err);
            });
    }, 2000);
}

// ========================================
// 恢复出厂设置
// ========================================
function factoryReset() {
    if (!confirm('⚠️ 确定要恢复出厂设置吗？\n\n此操作将：\n• 清除所有 WiFi 配置\n• 重置设备到出厂状态\n• 设备将重新启动配网模式\n\n确认继续吗？')) {
        return;
    }

    const confirmText = prompt('请输入 "YES" 确认恢复出厂设置：');
    if (confirmText !== 'YES') {
        showMessage('已取消恢复出厂设置', 'warning');
        return;
    }

    setStatus('正在恢复出厂设置...', 'connecting');

    fetch('/factory_reset', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            confirm: 'YES'
        })
    })
        .then(response => {
            if (!response.ok) {
                if (response.status === 405) {
                    throw new Error('方法不允许，请使用 POST 请求');
                }
                throw new Error('恢复失败: ' + response.status);
            }
            return response.text();
        })
        .then(data => {
            showMessage('✅ 恢复完成，请重新配置 WiFi', 'success');
            setStatus('已恢复出厂设置，正在重新扫描...', 'success');
            document.getElementById('ssid').value = '';
            document.getElementById('password').value = '';
            document.getElementById('wifi_list').innerHTML = '<option value="">请重新扫描 WiFi</option>';
            setTimeout(function () {
                scan_wifi();
            }, 3000);
        })
        .catch(err => {
            console.error('恢复出厂设置错误:', err);
            showMessage('❌ 恢复失败: ' + err.message, 'error');
            setStatus('恢复失败', 'error');
        });
}

// ========================================
// 键盘快捷键：回车键触发连接
// ========================================
document.addEventListener('keydown', function (e) {
    if (e.key === 'Enter') {
        const active = document.activeElement;
        if (active && (active.id === 'ssid' || active.id === 'password')) {
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