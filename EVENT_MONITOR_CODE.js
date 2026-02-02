// ============ ESP32 EVENT MONITORING (ADDED TO END OF app.js) ============

// Firebase Configuration
const firebaseConfig = {
    apiKey: "AIzaSyCiG3ZhMyPEz1cz0Gy61p3fqndTHy8fO0E",
    authDomain: "aseengrs-7f20c.firebaseapp.com",
    databaseURL: "https://aseengrs-7f20c-default-rtdb.asia-southeast1.firebasedatabase.app",
    projectId: "aseengrs-7f20c",
    storageBucket: "aseengrs-7f20c.firebasestorage.app",
    messagingSenderId: "485903353893",
    appId: "1:485903353893:web:d6b09da3dd3084b10a88f9"
};

// Firebase SDK import (add this to filemanager.html just before closing </body>)
// Dynamically load Firebase SDK
function loadFirebaseSDK() {
    return new Promise((resolve) => {
        if (window.firebase) {
            resolve();
            return;
        }

        const script1 = document.createElement('script');
        script1.src = 'https://www.gstatic.com/firebasejs/9.22.0/firebase-app-compat.js';
        script1.onload = () => {
            const script2 = document.createElement('script');
            script2.src = 'https://www.gstatic.com/firebasejs/9.22.0/firebase-database-compat.js';
            script2.onload = resolve;
            document.head.appendChild(script2);
        };
        document.head.appendChild(script1);
    });
}

let firebaseApp = null;
let firebaseDatabase = null;
let eventMonitorInterval = null;

// Initialize Firebase and start monitoring
async function initializeEventMonitoring() {
    try {
        await loadFirebaseSDK();

        // Initialize Firebase
        firebaseApp = firebase.initializeApp(firebaseConfig);
        firebaseDatabase = firebase.database();

        // Start monitoring
        startEventMonitoring();

        console.log('📡 ESP32 Event Monitor initialized');
    } catch (error) {
        console.error('Firebase initialization error:', error);
        updateEventDisplay({
            icon: '❌',
            message: 'Firebase connection failed',
            time: new Date().toLocaleTimeString(),
            status: 'error'
        });
    }
}

// Update event display
function updateEventDisplay(data) {
    const eventStatus = document.getElementById('eventStatus');
    const eventIcon = document.getElementById('eventIcon');
    const event Message = document.getElementById('eventMessage');
    const eventTime = document.getElementById('eventTime');

    if (eventIcon) eventIcon.textContent = data.icon || '⏳';
    if (eventMessage) eventMessage.textContent = data.message || 'Waiting...';
    if (eventTime) eventTime.textContent = data.time || '--';

    // Update status badge
    if (eventStatus) {
        eventStatus.textContent = data.status === 'connected' ? '✅ Connected' :
            data.status === 'error' ? '❌ Error' :
                '⏳ Connecting...';
        eventStatus.className = 'event-status ' + (data.status || '');
    }
}

// Fetch and display ESP32 events
async function fetchESP32Event() {
    try {
        // Fetch Esp33Update timestamp
        const timestampSnapshot = await firebaseDatabase.ref('/Esp33Update').once('value');
        const timestamp = timestampSnapshot.val() || 'Never';

        // Fetch Esp33Event (main event data)
        const eventSnapshot = await firebaseDatabase.ref('/Esp33Event').once('value');
        const eventData = eventSnapshot.val();

        if (eventData) {
            // Update event display
            updateEventDisplay({
                icon: getEventIcon(eventData.type),
                message: eventData.message || 'No status',
                time: eventData.timestamp || timestamp,
                status: isRecentEvent(timestamp) ? 'connected' : 'error'
            });

            // Update stats
            updateEventStats(eventData, timestamp);
        } else {
            // No event data yet
            updateEventDisplay({
                icon: '⏳',
                message: 'Waiting for ESP32...',
                time: timestamp,
                status: isRecentEvent(timestamp) ? 'connected' : 'error'
            });
        }
    } catch (error) {
        console.error('Error fetching ESP32 event:', error);
        updateEventDisplay({
            icon: '❌',
            message: 'Failed to fetch status',
            time: new Date().toLocaleTimeString(),
            status: 'error'
        });
    }
}

// Update stats display
function updateEventStats(eventData, timestamp) {
    const lastSyncTime = document.getElementById('lastSyncTime');
    const uptimeValue = document.getElementById('uptimeValue');
    const freeMemory = document.getElementById('freeMemory');

    if (lastSyncTime) lastSyncTime.textContent = timestamp;
    if (uptimeValue) uptimeValue.textContent = eventData.uptime || '--';
    if (freeMemory) freeMemory.textContent = eventData.freeMemory || '--';
}

// Check if timestamp is recent (< 10 minutes)
function isRecentEvent(timestampStr) {
    if (!timestampStr || timestampStr === 'Never' || timestampStr === 'Time not set') {
        return false;
    }

    try {
        // Parse format: "15 Jan 2026 12:44 AM"
        const parts = timestampStr.match(/(\d+) (\w+) (\d+) (\d+):(\d+) (AM|PM)/);
        if (!parts) return false;

        const months = {
            'Jan': 0, 'Feb': 1, 'Mar': 2, 'Apr': 3, 'May': 4, 'Jun': 5,
            'Jul': 6, 'Aug': 7, 'Sep': 8, 'Oct': 9, 'Nov': 10, 'Dec': 11
        };

        let day = parseInt(parts[1]);
        let month = months[parts[2]];
        let year = parseInt(parts[3]);
        let hour = parseInt(parts[4]);
        let minute = parseInt(parts[5]);
        let ampm = parts[6];

        // Convert to 24-hour format
        if (ampm === 'PM' && hour !== 12) hour += 12;
        if (ampm === 'AM' && hour === 12) hour = 0;

        const eventDate = new Date(year, month, day, hour, minute);
        const now = new Date();
        const diffMinutes = Math.floor((now - eventDate) / 1000 / 60);

        return diffMinutes <= 10;
    } catch (e) {
        return false;
    }
}

// Get icon based on event type
function getEventIcon(type) {
    const icons = {
        'boot': '🚀',
        'wifi_connected': '📶',
        'wifi_failed': '📵',
        'firebase_ready': '🔥',
        'firebase_failed': '❌',
        'sync_start': '🔄',
        'sync_complete': '✅',
        'sync_failed': '⚠️',
        'uploading': '📤',
        'error': '❌',
        'restart': '🔁'
    };
    return icons[type] || '📡';
}

// Start monitoring with 10-second interval
function startEventMonitoring() {
    // Initial fetch
    fetchESP32Event();

    // Fetch every 10 seconds
    if (eventMonitorInterval) {
        clearInterval(eventMonitorInterval);
    }
    eventMonitorInterval = setInterval(fetchESP32Event, 10000);
}

// Initialize when page loads
document.addEventListener('DOMContentLoaded', () => {
    // Wait a bit for page to fully load
    setTimeout(() => {
        initializeEventMonitoring();
    }, 1000);
});

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (eventMonitorInterval) {
        clearInterval(eventMonitorInterval);
    }
});
