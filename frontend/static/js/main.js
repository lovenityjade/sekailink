// SekaiLink - Main JavaScript
// Global utilities and helper functions

// Utility: Escape HTML to prevent XSS
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// Utility: Format date to locale string
function formatDate(dateString) {
    const date = new Date(dateString);
    return date.toLocaleDateString();
}

// Utility: Format date and time
function formatDateTime(dateString) {
    const date = new Date(dateString);
    return date.toLocaleString();
}

// Utility: Time ago formatter
function timeAgo(dateString) {
    const date = new Date(dateString);
    const now = new Date();
    const seconds = Math.floor((now - date) / 1000);

    const intervals = {
        year: 31536000,
        month: 2592000,
        week: 604800,
        day: 86400,
        hour: 3600,
        minute: 60,
        second: 1
    };

    for (const [name, value] of Object.entries(intervals)) {
        const interval = Math.floor(seconds / value);
        if (interval >= 1) {
            return `${interval} ${name}${interval > 1 ? 's' : ''} ago`;
        }
    }

    return 'just now';
}

// Utility: Show toast notification
function showToast(message, type = 'info') {
    // Create toast container if it doesn't exist
    let container = document.getElementById('toast-container');
    if (!container) {
        container = document.createElement('div');
        container.id = 'toast-container';
        container.style.cssText = 'position: fixed; top: 20px; right: 20px; z-index: 10000;';
        document.body.appendChild(container);
    }

    // Create toast element
    const toast = document.createElement('div');
    toast.className = `alert alert-${type} fade-in`;
    toast.style.cssText = 'margin-bottom: 10px; min-width: 300px; box-shadow: 0 4px 12px rgba(0,0,0,0.3);';
    toast.innerHTML = `
        <div style="display: flex; align-items: center; gap: var(--space-sm);">
            <span>${escapeHtml(message)}</span>
            <button onclick="this.parentElement.parentElement.remove()" style="background: none; border: none; color: inherit; cursor: pointer; margin-left: auto;">×</button>
        </div>
    `;

    container.appendChild(toast);

    // Auto-remove after 5 seconds
    setTimeout(() => {
        toast.style.opacity = '0';
        setTimeout(() => toast.remove(), 300);
    }, 5000);
}

// Utility: Copy to clipboard
async function copyToClipboard(text) {
    try {
        await navigator.clipboard.writeText(text);
        showToast('Copied to clipboard!', 'success');
        return true;
    } catch (err) {
        console.error('Failed to copy:', err);
        showToast('Failed to copy to clipboard', 'error');
        return false;
    }
}

// Utility: Confirm dialog
function confirmDialog(message) {
    return confirm(message);
}

// ========================================
// WebSocket Connection (Socket.IO)
// ========================================

let socket = null;
let heartbeatInterval = null;

// Initialize Socket.IO connection
function initializeWebSocket() {
    // Only initialize if Socket.IO is available
    if (typeof io === 'undefined') {
        console.warn('Socket.IO client library not loaded');
        return;
    }

    // Connect to server
    socket = io({
        transports: ['websocket', 'polling'],
        reconnection: true,
        reconnectionDelay: 1000,
        reconnectionAttempts: 5
    });

    // Expose socket globally for other pages to use
    window.socket = socket;

    // Connection event handlers
    socket.on('connect', () => {
        console.log('✅ WebSocket connected');

        // Start heartbeat to keep connection alive
        if (heartbeatInterval) clearInterval(heartbeatInterval);
        heartbeatInterval = setInterval(() => {
            socket.emit('heartbeat');
        }, 30000); // Every 30 seconds
    });

    socket.on('disconnect', () => {
        console.log('❌ WebSocket disconnected');
        if (heartbeatInterval) {
            clearInterval(heartbeatInterval);
            heartbeatInterval = null;
        }
    });

    socket.on('connect_error', (error) => {
        console.error('WebSocket connection error:', error);
    });

    socket.on('heartbeat_ack', (data) => {
        console.log('💓 Heartbeat acknowledged');
    });

    // Friend status events
    socket.on('friend_online', (data) => {
        console.log(`🟢 Friend online: ${data.username}`);
        showToast(`${data.username} is now online`, 'success');

        // Update friend list if it exists on the page
        updateFriendOnlineStatus(data.user_id, true);
    });

    socket.on('friend_offline', (data) => {
        console.log(`⚫ Friend offline: ${data.username}`);

        // Update friend list if it exists on the page
        updateFriendOnlineStatus(data.user_id, false);
    });

    // Global lobby events (for homepage)
    socket.on('lobby_created', (data) => {
        console.log('🎮 New lobby created:', data.name);

        // If there's a lobby list on the page, update it
        if (typeof refreshLobbyList === 'function') {
            refreshLobbyList();
        }
    });

    socket.on('lobby_updated', (data) => {
        console.log('🔄 Lobby updated:', data.lobby_id);

        // If there's a lobby list on the page, update it
        if (typeof updateLobbyInList === 'function') {
            updateLobbyInList(data);
        }
    });

    console.log('🔌 WebSocket initialized');
}

// Helper: Update friend online status in UI
function updateFriendOnlineStatus(userId, isOnline) {
    const friendElements = document.querySelectorAll(`[data-friend-id="${userId}"]`);
    friendElements.forEach(element => {
        const statusBadge = element.querySelector('.friend-status');
        if (statusBadge) {
            statusBadge.className = `badge badge-${isOnline ? 'success' : 'secondary'}`;
            statusBadge.textContent = isOnline ? 'Online' : 'Offline';
        }
    });
}

// Join a specific lobby room
function joinLobbyRoom(lobbyId) {
    if (socket && socket.connected) {
        socket.emit('join_lobby', { lobby_id: lobbyId });
        console.log(`🚪 Joined lobby room: ${lobbyId}`);
    }
}

// Leave a specific lobby room
function leaveLobbyRoom(lobbyId) {
    if (socket && socket.connected) {
        socket.emit('leave_lobby', { lobby_id: lobbyId });
        console.log(`🚪 Left lobby room: ${lobbyId}`);
    }
}

// Fade-in animation observer
document.addEventListener('DOMContentLoaded', () => {
    // Initialize WebSocket on page load
    initializeWebSocket();
    const fadeElements = document.querySelectorAll('.fade-in');

    const observer = new IntersectionObserver((entries) => {
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                entry.target.style.opacity = '1';
                entry.target.style.transform = 'translateY(0)';
            }
        });
    }, {
        threshold: 0.1
    });

    fadeElements.forEach(el => {
        el.style.opacity = '0';
        el.style.transform = 'translateY(20px)';
        el.style.transition = 'opacity 0.5s ease, transform 0.5s ease';
        observer.observe(el);
    });
});

// Global error handler
window.addEventListener('unhandledrejection', (event) => {
    console.error('Unhandled promise rejection:', event.reason);
});

// Export utilities for use in other scripts
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        escapeHtml,
        formatDate,
        formatDateTime,
        timeAgo,
        showToast,
        copyToClipboard,
        confirmDialog
    };
}
