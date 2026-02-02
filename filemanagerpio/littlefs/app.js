// ============ SERVER-SIDE AUTHENTICATION (IP-BASED) ============
// Login happens on login.html page via /api/login endpoint
// Server checks IP address on every request
// No tokens needed in filemanager - all auth is server-side

const appContainer = document.getElementById('appContainer');

// Initialize app on page load
document.addEventListener('DOMContentLoaded', () => {
    // Show app (server already verified IP)
    appContainer.style.display = 'block';

    // Load file list and storage info
    loadFileList();
    loadStorageInfo();
});

// Logout function - just redirect to login page
async function logout() {
    try {
        // Notify server about logout
        await fetch('/api/logout', {
            method: 'POST'
        });
    } catch (error) {
        console.error('Logout error:', error);
    }

    // Redirect to login page
    showNotification('👋 Logged out', 'success');
    setTimeout(() => {
        window.location.href = '/';
    }, 500);
}

// ============ PASSWORD VISIBILITY TOGGLE ============
function togglePasswordVisibility(inputId) {
    const input = document.getElementById(inputId);
    const button = event.target;

    if (input.type === 'password') {
        input.type = 'text';
        button.textContent = 'Hide';
    } else {
        input.type = 'password';
        button.textContent = 'Show';
    }
}

// ============ SETTINGS MODAL HANDLING ============
const settingsBtn = document.getElementById('settingsBtn');
const settingsModal = document.getElementById('settingsModal');
const closeModal = document.getElementById('closeModal');
const cancelBtn = document.getElementById('cancelBtn');
const settingsForm = document.getElementById('settingsForm');

// Load settings from ESP32 on page load (only if logged in)
document.addEventListener('DOMContentLoaded', () => {
    // Settings will be loaded after successful login
});

// Open settings modal and load settings
settingsBtn.addEventListener('click', () => {
    loadSettings();
    settingsModal.classList.add('active');
});

// Close modal
closeModal.addEventListener('click', () => {
    settingsModal.classList.remove('active');
});

cancelBtn.addEventListener('click', () => {
    settingsModal.classList.remove('active');
});

// Close modal when clicking outside
settingsModal.addEventListener('click', (e) => {
    if (e.target === settingsModal) {
        settingsModal.classList.remove('active');
    }
});

// Load settings from ESP32 (IP-based auth, no tokens)
async function loadSettings() {
    try {
        const response = await fetch('/api/settings');

        if (!response.ok) {
            showNotification('Failed to load settings', 'error');
            return;
        }

        const settings = await response.json();
        document.getElementById('username').value = settings.username || '';
        document.getElementById('password').value = settings.password || '';
        document.getElementById('apSSID').value = settings.apSSID || 'ESP32-FileManager';
        document.getElementById('apPassword').value = settings.apPassword || '';
        document.getElementById('staSSID').value = settings.staSSID || '';
        document.getElementById('staPassword').value = settings.staPassword || '';
    } catch (error) {
        console.log('Error loading settings:', error);
    }
}

// Save settings to ESP32
settingsForm.addEventListener('submit', async (e) => {
    e.preventDefault();

    const settings = {
        username: document.getElementById('username').value,
        password: document.getElementById('password').value,
        apSSID: document.getElementById('apSSID').value,
        apPassword: document.getElementById('apPassword').value,
        staSSID: document.getElementById('staSSID').value,
        staPassword: document.getElementById('staPassword').value
    };

    try {
        const response = await fetch('/api/settings', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(settings)
        });

        if (response.ok) {
            showNotification('✅ Settings saved successfully!', 'success');
            settingsModal.classList.remove('active');
        } else {
            showNotification('❌ Failed to save settings', 'error');
        }
    } catch (error) {
        console.error('Error saving settings:', error);
        showNotification('❌ Error saving settings', 'error');
    }

    // Load settings again to confirm
    loadSettings();
});

// ============ FILE UPLOAD HANDLING ============
const dropZone = document.getElementById('dropZone');
const fileInput = document.getElementById('fileInput');
const progressBar = document.getElementById('progressBar');
const progressFill = document.getElementById('progressFill');

// ============ OTA UPLOAD HANDLING ============
const otaDropZone = document.getElementById('otaDropZone');
const otaFileInput = document.getElementById('otaFileInput');
const otaProgressBar = document.getElementById('otaProgressBar');
const otaProgressFill = document.getElementById('otaProgressFill');

// ============ FILE UPLOAD EVENTS ============
dropZone.addEventListener('click', () => fileInput.click());

dropZone.addEventListener('dragover', (e) => {
    e.preventDefault();
    dropZone.classList.add('dragover');
});

dropZone.addEventListener('dragleave', () => {
    dropZone.classList.remove('dragover');
});

dropZone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropZone.classList.remove('dragover');
    handleFiles(e.dataTransfer.files);
});

fileInput.addEventListener('change', (e) => {
    handleFiles(e.target.files);
});

// ============ OTA UPLOAD EVENTS ============
otaDropZone.addEventListener('click', () => otaFileInput.click());

otaDropZone.addEventListener('dragover', (e) => {
    e.preventDefault();
    otaDropZone.classList.add('dragover');
});

otaDropZone.addEventListener('dragleave', () => {
    otaDropZone.classList.remove('dragover');
});

otaDropZone.addEventListener('drop', (e) => {
    e.preventDefault();
    otaDropZone.classList.remove('dragover');

    const files = e.dataTransfer.files;
    if (files.length > 0) {
        const file = files[0];
        if (file.name.endsWith('.bin')) {
            handleOTAFile(file);
        } else {
            showNotification('Only .bin files are allowed for OTA', 'error');
        }
    }
});

otaFileInput.addEventListener('change', (e) => {
    if (e.target.files.length > 0) {
        const file = e.target.files[0];
        if (file.name.endsWith('.bin')) {
            handleOTAFile(file);
        } else {
            showNotification('Only .bin files are allowed for OTA', 'error');
            otaFileInput.value = '';
        }
    }
});

// ============ HANDLE REGULAR FILES ============
async function handleFiles(files) {
    for (let file of files) {
        await uploadFile(file);
    }
    loadFileList();
}

// ============ UPLOAD SINGLE FILE WITH PROGRESS ============
async function uploadFile(file) {
    return new Promise((resolve, reject) => {
        const formData = new FormData();
        formData.append('file', file);

        progressBar.classList.add('active');
        const xhr = new XMLHttpRequest();

        xhr.upload.addEventListener('progress', (e) => {
            if (e.lengthComputable) {
                const percentComplete = Math.round((e.loaded / e.total) * 100);
                progressFill.style.width = percentComplete + '%';
                progressFill.textContent = percentComplete + '%';
            }
        });

        xhr.addEventListener('load', () => {
            if (xhr.status === 200) {
                showNotification(`"${file.name}" uploaded successfully!`, 'success');
                progressFill.style.width = '0%';
                progressFill.textContent = '0%';
                progressBar.classList.remove('active');
                resolve();
            } else if (xhr.status === 401) {
                showNotification('Session expired, please login again', 'error');
                window.location.href = '/';
                reject();
            } else {
                showNotification(`Upload failed for "${file.name}"`, 'error');
                reject();
            }
        });

        xhr.addEventListener('error', () => {
            showNotification(`Upload error for "${file.name}"`, 'error');
            progressBar.classList.remove('active');
            reject();
        });

        xhr.open('POST', '/api/upload');
        xhr.send(formData);
    });
}

// ============ HANDLE OTA FIRMWARE FILE ============
async function handleOTAFile(file) {
    showNotification('Starting firmware upload...', 'info');

    return new Promise((resolve, reject) => {
        const formData = new FormData();
        formData.append('file', file);

        otaProgressBar.classList.add('active');
        const xhr = new XMLHttpRequest();

        xhr.upload.addEventListener('progress', (e) => {
            if (e.lengthComputable) {
                const percentComplete = Math.round((e.loaded / e.total) * 100);
                otaProgressFill.style.width = percentComplete + '%';
                otaProgressFill.textContent = percentComplete + '%';
            }
        });

        xhr.addEventListener('load', () => {
            if (xhr.status === 200) {
                otaProgressFill.style.width = '100%';
                otaProgressFill.textContent = '100%';
                showNotification('✅ Firmware updated successfully! Device restarting...', 'success');

                // Wait longer for device to restart
                setTimeout(() => {
                    otaFileInput.value = '';
                    otaProgressBar.classList.remove('active');
                    otaProgressFill.style.width = '0%';
                    otaProgressFill.textContent = '0%';
                    showNotification('Device should be back online now!', 'info');
                }, 8000);
                resolve();
            } else if (xhr.status === 401) {
                showNotification('Session expired, please login again', 'error');
                window.location.href = '/';
                reject();
            } else {
                showNotification('❌ Firmware update failed: ' + xhr.responseText, 'error');
                otaProgressBar.classList.remove('active');
                otaProgressFill.style.width = '0%';
                otaProgressFill.textContent = '0%';
                reject();
            }
        });

        xhr.addEventListener('error', () => {
            if (otaProgressFill.style.width !== '100%') {
                showNotification('❌ Firmware upload error', 'error');
                otaProgressBar.classList.remove('active');
                otaProgressFill.style.width = '0%';
                otaProgressFill.textContent = '0%';
                reject();
            } else {
                resolve();
            }
        });

        xhr.addEventListener('abort', () => {
            if (otaProgressFill.style.width !== '100%') {
                showNotification('❌ Firmware upload aborted', 'error');
                otaProgressBar.classList.remove('active');
                otaProgressFill.style.width = '0%';
                otaProgressFill.textContent = '0%';
                reject();
            } else {
                resolve();
            }
        });

        xhr.open('POST', '/api/ota');
        xhr.send(formData);
    });
}

// ============ LOAD AND DISPLAY FILE LIST ============
async function loadFileList() {
    try {
        const response = await fetch('/api/files');

        if (response.status === 401) {
            showNotification('Session expired, please login again', 'error');
            window.location.href = '/';
            return;
        }

        const files = await response.json();

        const fileListContainer = document.getElementById('fileListContainer');

        if (files.length === 0) {
            fileListContainer.innerHTML = '<div class="empty-message">No files uploaded yet</div>';
            return;
        }

        let html = '<div class="file-list">';
        files.forEach(file => {
            const sizeStr = formatFileSize(file.size);
            html += `
                <div class="file-item">
                    <div class="file-name">📄 ${escapeHtml(file.name)}</div>
                    <div class="file-size">${sizeStr}</div>
                    <div class="file-actions">
                        <button class="btn btn-download" onclick="downloadFile('${escapeHtml(file.name)}')">⬇️ Download</button>
                        <button class="btn btn-delete" onclick="deleteFile('${escapeHtml(file.name)}')">🗑️ Delete</button>
                    </div>
                </div>
            `;
        });
        html += '</div>';

        fileListContainer.innerHTML = html;
    } catch (error) {
        const fileListContainer = document.getElementById('fileListContainer');
        fileListContainer.innerHTML = '<div class="empty-message">Error loading files</div>';
        console.error('Error:', error);
    }
}

// ============ DELETE FILE WITH CONFIRMATION ============
async function deleteFile(filename) {
    if (!confirm(`Delete "${filename}"`)) {
        return;
    }

    try {
        const formData = new FormData();
        formData.append('filename', filename);

        const response = await fetch('/api/delete', {
            method: 'POST',
            body: formData
        });

        if (response.status === 401) {
            showNotification('Session expired, please login again', 'error');
            window.location.href = '/';
            return;
        }

        if (response.ok) {
            showNotification('File deleted successfully!', 'success');
            loadFileList();
        } else {
            showNotification('Failed to delete file', 'error');
        }
    } catch (error) {
        showNotification('Error deleting file: ' + error.message, 'error');
    }
}

// ============ DOWNLOAD FILE ============
function downloadFile(filename) {
    // For downloads, use fetch with blob then download
    fetch('/api/download?filename=' + encodeURIComponent(filename))
        .then(response => {
            if (response.status === 401) {
                showNotification('Session expired, please login again', 'error');
                window.location.href = '/';
                return;
            }
            if (!response.ok) throw new Error('Download failed');
            return response.blob();
        })
        .then(blob => {
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = filename;
            document.body.appendChild(a);
            a.click();
            window.URL.revokeObjectURL(url);
            document.body.removeChild(a);
        })
        .catch(error => {
            console.error('Download error:', error);
            showNotification('Download failed', 'error');
        });
}

// ============ FORMAT BYTES TO READABLE SIZE ============
function formatFileSize(bytes) {
    if (bytes === 0) return '0 Bytes';

    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));

    return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
}

// ============ ESCAPE HTML TO PREVENT XSS ============
function escapeHtml(text) {
    const map = {
        '&': '&amp;',
        '<': '&lt;',
        '>': '&gt;',
        '"': '&quot;',
        "'": '&#039;'
    };
    return text.replace(/[&<>"']/g, m => map[m]);
}

// ============ SHOW NOTIFICATION TOAST ============
function showNotification(message, type = 'info') {
    const notification = document.createElement('div');
    notification.className = `notification ${type}`;
    notification.textContent = message;
    document.body.appendChild(notification);

    setTimeout(() => {
        notification.remove();
    }, 4000);
}

// ============ LOAD STORAGE INFO ============
async function loadStorageInfo() {
    try {
        const response = await fetch('/api/storage');

        if (response.status === 401) {
            showNotification('Session expired, please login again', 'error');
            window.location.href = '/';
            return;
        }

        const data = await response.json();

        const usedMB = (data.used / 1024 / 1024).toFixed(2);
        const availableMB = (data.available / 1024 / 1024).toFixed(2);
        const percent = Math.round((data.used / data.total) * 100);

        document.getElementById('usedSpace').textContent = usedMB + ' MB';
        document.getElementById('availableSpace').textContent = availableMB + ' MB';
        document.getElementById('storagePercent').textContent = percent + '%';
        document.getElementById('storageBarFill').style.width = percent + '%';
    } catch (error) {
        console.error('Error loading storage info:', error);
    }
}

// ============ AUTO-REFRESH ============
// Auto-refresh every 5 seconds
setInterval(() => {
    loadFileList();
    loadStorageInfo();
}, 5000);

// ============ ESP32 STATUS MONITORING (DIRECT FROM ESP32) ============

let statusMonitorInterval = null;

// Fetch and display ESP32 status directly from ESP32
async function fetchESP32Status() {
    try {
        const response = await fetch('/api/esp32status');

        if (!response.ok) {
            throw new Error('Failed to fetch status');
        }

        const data = await response.json();

        // Update event display
        updateEventDisplay({
            icon: data.icon || '⏳',
            message: data.message || 'No status',
            time: data.timestamp || '--',
            status: 'connected'
        });

        // Update stats
        const lastSyncTime = document.getElementById('lastSyncTime');
        const uptimeValue = document.getElementById('uptimeValue');
        const freeMemory = document.getElementById('freeMemory');

        if (lastSyncTime) lastSyncTime.textContent = data.timestamp || '--';
        if (uptimeValue) uptimeValue.textContent = data.uptime || '--';
        if (freeMemory) freeMemory.textContent = data.freeMemory || '--';

    } catch (error) {
        console.error('Error fetching ESP32 status:', error);
        updateEventDisplay({
            icon: '❌',
            message: 'Failed to connect to ESP32',
            time: new Date().toLocaleTimeString(),
            status: 'error'
        });
    }
}

// Update event display
function updateEventDisplay(data) {
    const eventStatus = document.getElementById('eventStatus');
    const eventIcon = document.getElementById('eventIcon');
    const eventMessage = document.getElementById('eventMessage');
    const eventTime = document.getElementById('eventTime');

    if (eventIcon) eventIcon.textContent = data.icon || '⏳';
    if (eventMessage) eventMessage.textContent = data.message || 'Waiting...';
    if (eventTime) eventTime.textContent = data.time || '--';

    // Update status badge
    if (eventStatus) {
        eventStatus.textContent = data.status === 'connected' ? '✅ Connected' : '❌ Error';
        eventStatus.className = 'event-status ' + (data.status || '');
    }
}

// Start monitoring when page loads
(function initESP32Monitor() {
    // Wait for DOM to be fully loaded
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', startMonitoring);
    } else {
        startMonitoring();
    }

    function startMonitoring() {
        setTimeout(() => {
            // Initial fetch
            fetchESP32Status();

            // Fetch every 10 seconds
            if (statusMonitorInterval) {
                clearInterval(statusMonitorInterval);
            }
            statusMonitorInterval = setInterval(fetchESP32Status, 10000);
        }, 1000);
    }
})();

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (statusMonitorInterval) {
        clearInterval(statusMonitorInterval);
    }
});
