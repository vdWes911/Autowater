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
        showToast('Failed to control relay. Please try again.');
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
    const duration = parseInt(input.value);
    if (duration > 0) {
        toggleRelay(id, 'timed', duration);
    } else {
        showToast('Please enter a valid duration in minutes.');
    }
}

function adjustTimerDuration(id, delta) {
    const input = document.getElementById('duration-' + id);
    if (input) {
        const newVal = Math.max(1, Math.min(20, (parseInt(input.value) || 0) + delta));
        input.value = newVal;
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

        // Update routine UI
        if (data.routine) {
            const wasRunning = activeRoutineId !== null;
            const isRunning = data.routine.running;
            
            // Find which local routine matches the name (best effort)
            const routineIndex = routines.findIndex(r => r.name === data.routine.name);
            activeRoutineId = isRunning ? routineIndex : null;

            if (wasRunning && !isRunning) {
                showToast(`Routine completed!`, "success");
            }
            
            renderRoutines();
            
            if (isRunning && routineIndex !== -1) {
                const statusEl = document.getElementById(`routine-status-${routineIndex}`);
                if (statusEl) {
                    const current = data.routine.currentStep;
                    const steps = data.routine.steps;
                    const stepName = steps[current] ? steps[current].name : '?';
                    statusEl.innerHTML = `
                        <div class="routine-progress">
                            <span class="step-active">Active: ${stepName} (${current + 1}/${data.routine.numSteps})</span>
                            <div class="step-list-mini">
                                ${steps.map((s, i) => `
                                    <span class="step-dot ${i < current ? 'done' : (i === current ? 'busy' : 'todo')}" title="${s.name}"></span>
                                `).join('')}
                            </div>
                        </div>
                    `;
                }
            }
        }
    } catch (error) {
        console.error('Status update error:', error);
    }
}

let routines = [];
let activeRoutineId = null;
let stopRequested = false;
let skipRequested = false;

async function fetchRoutines() {
    try {
        const response = await fetch('/api/routines');
        if (response.ok) {
            routines = await response.json();
            renderRoutines();
        }
    } catch (e) {
        console.error("Failed to fetch routines", e);
    }
}

function renderRoutines() {
    const container = document.getElementById('routines');
    if (!container) return;
    container.innerHTML = '';
    
    if (routines.length > 0) {
        const title = document.createElement('h2');
        title.textContent = 'Routines';
        title.style.color = '#eceff1';
        title.style.fontSize = '20px';
        title.style.marginBottom = '15px';
        container.appendChild(title);
    }

    routines.forEach((routine, index) => {
        const pill = document.createElement('div');
        pill.className = 'routine-pill';
        if (activeRoutineId === index) pill.classList.add('active');
        
        const isActive = activeRoutineId === index;
        
        pill.innerHTML = `
            <div class="routine-pill-content" onclick="${isActive ? '' : `runRoutine(${index})`}">
                <span class="routine-pill-name">${routine.name}</span>
                <span class="routine-pill-status" id="routine-status-${index}">
                    ${isActive ? 'Running...' : 'Start'}
                </span>
            </div>
            ${isActive ? `
                <div class="routine-pill-actions">
                    <button class="btn-skip-routine" onclick="skipStep(event)">Skip</button>
                    <button class="btn-stop-routine" onclick="stopActiveRoutine(event)">Stop</button>
                </div>
            ` : ''}
        `;
        container.appendChild(pill);
    });
}

async function stopActiveRoutine(event) {
    if (event) event.stopPropagation();
    try {
        await fetch('/api/routine/control?action=stop');
        await updateStatus();
    } catch (e) {
        console.error("Failed to stop routine", e);
    }
}

async function skipStep(event) {
    if (event) event.stopPropagation();
    try {
        await fetch('/api/routine/control?action=skip');
        await updateStatus();
    } catch (e) {
        console.error("Failed to skip step", e);
    }
}

async function runRoutine(index) {
    try {
        const response = await fetch(`/api/routine/control?action=start&index=${index}`);
        const data = await response.json();
        if (data.success) {
            await updateStatus();
        }
    } catch (e) {
        console.error("Failed to start routine", e);
        showToast("Failed to start routine");
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
                    <div class="step-name" style="margin-bottom: 10px;">${RELAY_NAMES[i]}</div>
                    <div id="modal-rem-${i}" class="modal-remaining"></div>
                    <div class="step-controls" style="justify-content: center; margin-bottom: 20px;">
                        <button class="btn-step-adjust" onclick="adjustTimerDuration(${i}, -1)">-</button>
                        <input type="number" id="duration-${i}" value="${lastDuration}" min="1" max="20" style="margin-bottom: 0; width: 60px;">
                        <button class="btn-step-adjust" onclick="adjustTimerDuration(${i}, 1)">+</button>
                    </div>
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
    fetchRoutines();
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
