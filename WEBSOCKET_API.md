# WebSocket API Documentation

## Overview

SekaiLink uses **Flask-SocketIO** for real-time bidirectional communication between the server and clients. This enables instant lobby updates, chat messages, generation progress, and player status changes without polling.

## Connection

### Client Setup

Include the Socket.IO client library in your HTML:

```html
<script src="https://cdn.socket.io/4.6.0/socket.io.min.js"></script>
```

### Connecting to the Server

```javascript
// Connect to the WebSocket server
const socket = io('http://localhost:7000', {
    transports: ['websocket', 'polling'],
    withCredentials: true
});

// Handle successful connection
socket.on('connect', () => {
    console.log('✅ Connected to SekaiLink');
});

// Handle connection errors
socket.on('connect_error', (error) => {
    console.error('❌ Connection error:', error);
});

// Handle disconnection
socket.on('disconnect', (reason) => {
    console.log('Disconnected:', reason);
});
```

**Note:** Authentication is handled via session cookies. Users must be logged in via Discord OAuth before connecting.

## Events Reference

### 📡 Connection Events

#### Server → Client: `connected`
Sent when client successfully connects.

```javascript
socket.on('connected', (data) => {
    console.log(data.message); // "Connected to SekaiLink"
});
```

**Response:**
```json
{
    "status": "success",
    "message": "Connected to SekaiLink"
}
```

---

### 🏠 Lobby Events

#### Client → Server: `join_lobby`
Join a lobby room to receive real-time updates.

```javascript
socket.emit('join_lobby', {
    lobby_id: 123
});
```

**Server Responses:**

1. **To joining user:** `lobby_state`
```json
{
    "lobby_id": 123,
    "name": "Epic Pokemon Randomizer",
    "status": "pending",
    "host_id": "uuid-here",
    "created_at": "2025-01-15T10:30:00.000Z",
    "seed_url": null
}
```

2. **To other users:** `user_joined`
```json
{
    "user_id": "uuid-here",
    "username": "PlayerName",
    "avatar": "https://cdn.discordapp.com/avatars/...",
    "pronouns": "they/them",
    "timestamp": "2025-01-15T10:30:00.000Z"
}
```

#### Client → Server: `leave_lobby`
Leave a lobby room.

```javascript
socket.emit('leave_lobby', {
    lobby_id: 123
});
```

**Broadcasts:** `user_left`
```json
{
    "user_id": "uuid-here",
    "username": "PlayerName",
    "timestamp": "2025-01-15T10:35:00.000Z"
}
```

---

### ✅ Player Status Events

#### Client → Server: `player_ready`
Mark yourself as ready or not ready.

```javascript
// Mark as ready
socket.emit('player_ready', {
    lobby_id: 123,
    ready: true
});

// Mark as not ready
socket.emit('player_ready', {
    lobby_id: 123,
    ready: false
});
```

**Broadcasts:** `player_status_changed`
```json
{
    "user_id": "uuid-here",
    "username": "PlayerName",
    "ready": true,
    "timestamp": "2025-01-15T10:40:00.000Z"
}
```

**Frontend Example:**
```javascript
socket.on('player_status_changed', (data) => {
    const playerElement = document.querySelector(`[data-user-id="${data.user_id}"]`);
    if (data.ready) {
        playerElement.classList.add('ready');
        playerElement.querySelector('.status').textContent = '✅ Ready';
    } else {
        playerElement.classList.remove('ready');
        playerElement.querySelector('.status').textContent = '⏳ Not Ready';
    }
});
```

#### Client → Server: `player_finished`
Mark yourself as finished or DNF.

```javascript
// Mark as finished
socket.emit('player_finished', {
    lobby_id: 123,
    status: 'finished'
});

// Mark as DNF (Did Not Finish)
socket.emit('player_finished', {
    lobby_id: 123,
    status: 'dnf'
});
```

**Broadcasts:** `player_finished`
```json
{
    "user_id": "uuid-here",
    "username": "PlayerName",
    "status": "finished",
    "timestamp": "2025-01-15T12:00:00.000Z"
}
```

---

### 🎮 Lobby Control Events (Host Only)

#### Client → Server: `start_sync`
Start the sync (only the lobby host can do this).

```javascript
socket.emit('start_sync', {
    lobby_id: 123
});
```

**Broadcasts:** `sync_started`
```json
{
    "lobby_id": 123,
    "started_by": "HostUsername",
    "timestamp": "2025-01-15T10:45:00.000Z"
}
```

**Error Response:** `error`
```json
{
    "message": "Only the host can start the sync"
}
```

**Frontend Example:**
```javascript
// Only show start button to host
if (currentUser.id === lobby.host_id) {
    document.getElementById('start-sync-btn').style.display = 'block';
}

socket.on('sync_started', (data) => {
    // Hide ready buttons
    document.querySelectorAll('.ready-btn').forEach(btn => btn.disabled = true);

    // Start timer
    startTimer();

    // Show notification
    showNotification(`Sync started by ${data.started_by}!`);
});
```

#### Client → Server: `stop_sync`
Stop the sync (only the lobby host can do this).

```javascript
socket.emit('stop_sync', {
    lobby_id: 123
});
```

**Broadcasts:** `sync_stopped`
```json
{
    "lobby_id": 123,
    "stopped_by": "HostUsername",
    "timestamp": "2025-01-15T14:30:00.000Z"
}
```

---

### 💬 Chat Events

#### Client → Server: `chat_message`
Send a chat message to the lobby.

```javascript
socket.emit('chat_message', {
    lobby_id: 123,
    message: 'Good luck everyone!'
});
```

**Broadcasts:** `chat_message`
```json
{
    "user_id": "uuid-here",
    "username": "PlayerName",
    "avatar": "https://cdn.discordapp.com/avatars/...",
    "message": "Good luck everyone!",
    "timestamp": "2025-01-15T10:50:00.000Z"
}
```

**Constraints:**
- Maximum message length: 500 characters
- Messages are trimmed of whitespace
- Empty messages are rejected

**Frontend Example:**
```javascript
// Send message
const chatForm = document.getElementById('chat-form');
chatForm.addEventListener('submit', (e) => {
    e.preventDefault();
    const input = document.getElementById('chat-input');
    const message = input.value.trim();

    if (message) {
        socket.emit('chat_message', {
            lobby_id: currentLobbyId,
            message: message
        });
        input.value = '';
    }
});

// Receive messages
socket.on('chat_message', (data) => {
    const chatBox = document.getElementById('chat-box');
    const messageEl = document.createElement('div');
    messageEl.className = 'chat-message';
    messageEl.innerHTML = `
        <img src="${data.avatar}" class="avatar">
        <div class="message-content">
            <span class="username">${data.username}</span>
            <span class="timestamp">${formatTime(data.timestamp)}</span>
            <p>${escapeHtml(data.message)}</p>
        </div>
    `;
    chatBox.appendChild(messageEl);
    chatBox.scrollTop = chatBox.scrollHeight;
});
```

---

### ⚙️ Generation Events

#### Server → Client: `generation_update`
Progress updates during seed generation.

```javascript
socket.on('generation_update', (data) => {
    const progressBar = document.getElementById('progress-bar');
    progressBar.style.width = `${data.progress}%`;
    progressBar.textContent = `${data.progress}%`;

    const statusText = document.getElementById('status-text');
    statusText.textContent = data.message;
});
```

**Payload:**
```json
{
    "lobby_id": 123,
    "progress": 45,
    "message": "Generating world 3/5...",
    "timestamp": "2025-01-15T10:52:30.000Z"
}
```

#### Server → Client: `generation_complete`
Sent when seed generation completes (success or failure).

```javascript
socket.on('generation_complete', (data) => {
    if (data.success) {
        // Show download button
        const downloadBtn = document.getElementById('download-patch-btn');
        downloadBtn.href = data.seed_url;
        downloadBtn.style.display = 'block';

        // Show success message
        showNotification('✅ Seed generated successfully!');
    } else {
        // Show error
        showError(`❌ Generation failed: ${data.error}`);
    }
});
```

**Success Payload:**
```json
{
    "lobby_id": 123,
    "success": true,
    "seed_url": "https://sekailink.xyz/seeds/123/seed.zip",
    "error": null,
    "timestamp": "2025-01-15T10:55:00.000Z"
}
```

**Failure Payload:**
```json
{
    "lobby_id": 123,
    "success": false,
    "seed_url": "",
    "error": "Missing ROM for Pokemon Emerald",
    "timestamp": "2025-01-15T10:55:00.000Z"
}
```

---

### ❌ Error Handling

#### Server → Client: `error`
Generic error event for invalid requests.

```javascript
socket.on('error', (data) => {
    console.error('Socket error:', data.message);
    showNotification(data.message, 'error');
});
```

**Example Errors:**
```json
{"message": "Not authenticated"}
{"message": "Missing lobby_id"}
{"message": "Lobby not found"}
{"message": "Only the host can start the sync"}
{"message": "Invalid message"}
```

---

## Complete Frontend Example

```javascript
// Initialize Socket.IO
const socket = io('http://localhost:7000', {
    withCredentials: true
});

let currentLobbyId = null;

// Connection handlers
socket.on('connect', () => {
    console.log('✅ Connected');
    if (currentLobbyId) {
        joinLobby(currentLobbyId);
    }
});

socket.on('disconnect', () => {
    console.log('❌ Disconnected');
});

// Join a lobby
function joinLobby(lobbyId) {
    currentLobbyId = lobbyId;
    socket.emit('join_lobby', { lobby_id: lobbyId });
}

// Leave a lobby
function leaveLobby() {
    if (currentLobbyId) {
        socket.emit('leave_lobby', { lobby_id: currentLobbyId });
        currentLobbyId = null;
    }
}

// Lobby state
socket.on('lobby_state', (data) => {
    console.log('Lobby state:', data);
    updateLobbyUI(data);
});

// User joined
socket.on('user_joined', (data) => {
    addPlayerToList(data);
    addChatMessage({
        type: 'system',
        message: `${data.username} joined the lobby`
    });
});

// User left
socket.on('user_left', (data) => {
    removePlayerFromList(data.user_id);
    addChatMessage({
        type: 'system',
        message: `${data.username} left the lobby`
    });
});

// Player ready status
socket.on('player_status_changed', (data) => {
    updatePlayerStatus(data.user_id, data.ready);
});

// Sync started
socket.on('sync_started', (data) => {
    startGameTimer();
    disableReadyButtons();
    addChatMessage({
        type: 'system',
        message: `Sync started by ${data.started_by}!`
    });
});

// Sync stopped
socket.on('sync_stopped', (data) => {
    stopGameTimer();
    addChatMessage({
        type: 'system',
        message: `Sync stopped by ${data.stopped_by}`
    });
});

// Chat messages
socket.on('chat_message', (data) => {
    addChatMessage(data);
});

// Generation progress
socket.on('generation_update', (data) => {
    updateProgressBar(data.progress, data.message);
});

// Generation complete
socket.on('generation_complete', (data) => {
    if (data.success) {
        showDownloadButton(data.seed_url);
    } else {
        showError(data.error);
    }
});

// Error handling
socket.on('error', (data) => {
    showNotification(data.message, 'error');
});

// Ready button click
document.getElementById('ready-btn').addEventListener('click', () => {
    const isReady = !getCurrentUserReadyStatus();
    socket.emit('player_ready', {
        lobby_id: currentLobbyId,
        ready: isReady
    });
});

// Start sync button (host only)
document.getElementById('start-sync-btn').addEventListener('click', () => {
    socket.emit('start_sync', { lobby_id: currentLobbyId });
});

// Chat form
document.getElementById('chat-form').addEventListener('submit', (e) => {
    e.preventDefault();
    const input = document.getElementById('chat-input');
    const message = input.value.trim();

    if (message) {
        socket.emit('chat_message', {
            lobby_id: currentLobbyId,
            message: message
        });
        input.value = '';
    }
});

// Clean up on page unload
window.addEventListener('beforeunload', () => {
    leaveLobby();
});
```

## Testing

### Using Browser Console

```javascript
// Connect
const socket = io('http://localhost:7000', { withCredentials: true });

// Join lobby 1
socket.emit('join_lobby', { lobby_id: 1 });

// Send chat message
socket.emit('chat_message', { lobby_id: 1, message: 'Hello!' });

// Mark as ready
socket.emit('player_ready', { lobby_id: 1, ready: true });

// Listen to all events
socket.onAny((eventName, ...args) => {
    console.log(`Event: ${eventName}`, args);
});
```

## Security Notes

1. **Authentication Required:** All WebSocket connections require a valid session cookie from Discord OAuth
2. **Authorization:** Host-only actions (start/stop sync) are validated server-side
3. **Input Validation:** Chat messages are limited to 500 characters and sanitized
4. **Rate Limiting:** Consider implementing client-side rate limiting for chat messages

## Troubleshooting

### Connection fails with CORS error
- Ensure `ALLOWED_ORIGINS` in `.env` includes your frontend domain
- Check that `withCredentials: true` is set on the client

### Messages not received
- Verify you've joined the lobby room with `join_lobby`
- Check browser console for errors
- Ensure backend is running with eventlet worker

### Only host sees events
- Make sure all clients call `join_lobby` after connecting
- Check that room names match (`lobby_{id}`)

### WebSocket disconnects frequently
- Increase `ping_timeout` and `ping_interval` in SocketIO config
- Check network stability
- Verify proxy/load balancer supports WebSocket

## Production Deployment

For production with multiple servers, configure Redis as a message queue:

```python
# In main.py
socketio = SocketIO(
    app,
    message_queue=os.getenv('REDIS_URL'),
    async_mode='eventlet'
)
```

This allows WebSocket messages to sync across multiple server instances.

## Additional Resources

- [Flask-SocketIO Documentation](https://flask-socketio.readthedocs.io/)
- [Socket.IO Client API](https://socket.io/docs/v4/client-api/)
- [Eventlet Documentation](https://eventlet.net/)
