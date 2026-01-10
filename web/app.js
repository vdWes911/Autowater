const RELAY_NAMES = ['Plants', 'Grass', 'Patio Grass', 'Front Lawn'];

const ICONS = {
    timer: `<svg class="status-icon" viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"></circle><polyline points="12 6 12 12 16 14"></polyline></svg>`,
    manual: `<svg class="status-icon" viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"><path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"></path><circle cx="12" cy="7" r="4"></circle></svg>`
};

async function toggleRelay(id, action, durationMinutes) {
    const card = document.getElementById('relay-' + id);
    const status = document.getElementById('status-' + id);
    const buttons = card.querySelectorAll('button');

    buttons.forEach(btn => {
        btn.disabled = true;
        btn.classList.add('loading');
    });

    try {
        let url = `/api/relay?id=${id}&action=${action}`;
        if (durationMinutes) {
            const seconds = durationMinutes * 60;
            url += `&duration=${seconds}`;
            localStorage.setItem('lastDurationMinutes', durationMinutes);
        }
        const response = await fetch(url);
        const data = await response.json();

        if (data.success) {
            if (data.mode === 'timed' && data.rem > 0) {
                const expireAt = Date.now() + (data.rem * 1000);
                localStorage.setItem(`expire-${id}`, expireAt);
            } else {
                localStorage.removeItem(`expire-${id}`);
            }
            updateRelayUI(id, data.state, data.mode, data.rem);
        }
    } catch (error) {
        console.error('Error:', error);
        alert('Failed to control relay. Please try again.');
    } finally {
        buttons.forEach(btn => {
            btn.disabled = false;
            btn.classList.remove('loading');
        });
        closeModal(id);
    }
}

function updateRelayUI(id, state, mode, remainingSeconds) {
    const status = document.getElementById('status-' + id);
    if (!status) return;

    const isOn = state === 'on';
    let iconHtml = '';
    let remStr = '';

    if (isOn) {
        if (mode === 'timed') {
            iconHtml = ICONS.timer;
            // Use remainingSeconds if provided, otherwise try localStorage
            let rem = remainingSeconds;
            if (rem === undefined || rem === null) {
                const expireAt = localStorage.getItem(`expire-${id}`);
                if (expireAt) {
                    rem = Math.max(0, Math.floor((expireAt - Date.now()) / 1000));
                }
            }
            
            if (rem > 0) {
                const mins = Math.floor(rem / 60);
                const secs = rem % 60;
                remStr = ` (${mins}:${secs.toString().padStart(2, '0')})`;
            } else if (rem === 0 && mode === 'timed') {
                localStorage.removeItem(`expire-${id}`);
            }
        } else {
            iconHtml = ICONS.manual;
            localStorage.removeItem(`expire-${id}`);
        }
    } else {
        localStorage.removeItem(`expire-${id}`);
    }
    
    status.innerHTML = (isOn ? 'ON' : 'OFF') + iconHtml + remStr;
    status.className = isOn ? 'status on' : 'status off';
    
    // Update modal remaining time if open
    const modalRem = document.getElementById('modal-rem-' + id);
    if (modalRem) {
        modalRem.textContent = remStr ? `Remaining: ${remStr.trim()}` : '';
    }
}

function showModal(id) {
    document.getElementById('modal-' + id).style.display = 'flex';
}

function closeModal(id) {
    document.getElementById('modal-' + id).style.display = 'none';
}

function timedRelay(id) {
    const input = document.getElementById('duration-' + id);
    const duration = input.value;
    if (duration > 0) {
        toggleRelay(id, 'timed', duration);
    } else {
        alert('Please enter a valid duration in minutes.');
    }
}

async function updateStatus() {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();

        data.relays.forEach(relay => {
            if (relay.mode === 'timed' && relay.rem > 0) {
                const expireAt = Date.now() + (relay.rem * 1000);
                localStorage.setItem(`expire-${relay.id}`, expireAt);
            }
            updateRelayUI(relay.id, relay.state, relay.mode, relay.rem);
        });
    } catch (error) {
        console.error('Status update error:', error);
    }
}

function createRelayCards() {
    const container = document.getElementById('relays');
    const lastDuration = localStorage.getItem('lastDurationMinutes') || 10;

    for (let i = 0; i < 4; i++) {
        const card = document.createElement('div');
        card.className = 'card';
        card.id = 'relay-' + i;

        card.innerHTML = `
            <div class="relay-header">
                <span class="relay-name">${RELAY_NAMES[i]}</span>
                <div class="status-group">
                    <button class="btn-timer-trigger" onclick="showModal(${i})" title="Timed ON">
                        <svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2">
                            <circle cx="12" cy="12" r="10"></circle>
                            <polyline points="12 6 12 12 16 14"></polyline>
                        </svg>
                    </button>
                    <span class="status off" id="status-${i}">OFF</span>
                </div>
            </div>
            <div class="btn-group">
                <button class="btn-on" onclick="toggleRelay(${i}, 'on')">Turn On</button>
                <button class="btn-off" onclick="toggleRelay(${i}, 'off')">Turn Off</button>
                
            </div>
            <div id="modal-${i}" class="modal-overlay" onclick="if(event.target===this)closeModal(${i})">
                <div class="modal">
                    <h3>Set Timer (min)</h3>
                    <div id="modal-rem-${i}" class="modal-remaining"></div>
                    <input type="number" id="duration-${i}" value="${lastDuration}" min="1" max="20">
                    <div class="modal-btns">
                        <button class="btn-cancel" onclick="closeModal(${i})">Cancel</button>
                        <button class="btn-timed" onclick="timedRelay(${i})">Start</button>
                    </div>
                </div>
            </div>
        `;

        container.appendChild(card);
    }
}

// Initialize on load
document.addEventListener('DOMContentLoaded', () => {
    createRelayCards();
    updateStatus();
    setInterval(updateStatus, 10000); // Check ESP every 10s
    
    // Smooth countdown local update
    setInterval(() => {
        for(let i=0; i<4; i++) {
            const status = document.getElementById('status-' + i);
            if (status && status.classList.contains('on')) {
                // Check if we have an expiration time for this relay
                const expireAt = localStorage.getItem(`expire-${i}`);
                if (expireAt) {
                    updateRelayUI(i, 'on', 'timed');
                }
            }
        }
    }, 1000);
});
