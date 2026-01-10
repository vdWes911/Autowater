let routines = [];
let currentRoutineIndex = -1;

const ICONS = {
    up: `<svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"><polyline points="18 15 12 9 6 15"></polyline></svg>`,
    down: `<svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"><polyline points="6 9 12 15 18 9"></polyline></svg>`,
    trash: `<svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="3 6 5 6 21 6"></polyline><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path><line x1="10" y1="11" x2="10" y2="17"></line><line x1="14" y1="11" x2="14" y2="17"></line></svg>`
};

function showToast(message, type = 'error') {
    let toastContainer = document.getElementById('toast-container');
    if (!toastContainer) {
        toastContainer = document.createElement('div');
        toastContainer.id = 'toast-container';
        document.body.appendChild(toastContainer);
    }

    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = message;
    
    toastContainer.appendChild(toast);

    setTimeout(() => {
        toast.classList.add('fade-out');
        setTimeout(() => toast.remove(), 500);
    }, 3000);
}

async function fetchRoutines() {
    try {
        const response = await fetch('/api/routines');
        if (response.ok) {
            routines = await response.json();
            if (!Array.isArray(routines)) routines = [];
            updateRoutineList();
        }
    } catch (e) {
        console.error("Failed to fetch routines", e);
        routines = [];
        updateRoutineList();
    }
}

function updateRoutineList() {
    const list = document.getElementById('routine-list');
    const val = list.value;
    list.innerHTML = '<option value="">-- Select or Create Routine --</option>';
    routines.forEach((r, i) => {
        const opt = document.createElement('option');
        opt.value = i;
        opt.textContent = r.name;
        list.appendChild(opt);
    });
    list.value = val;
}

function createNewRoutine() {
    const name = `Routine ${routines.length + 1}`;
    routines.push({ name, steps: [] });
    updateRoutineList();
    document.getElementById('routine-list').value = routines.length - 1;
    loadSelectedRoutine();
    
    // Focus the name input for immediate editing
    setTimeout(() => {
        const nameInput = document.getElementById('routine-name');
        if (nameInput) {
            nameInput.focus();
            nameInput.select();
        }
    }, 10);
}

function deleteRoutine() {
    if (currentRoutineIndex === -1) return;
    if (!confirm("Delete this routine?")) return;
    
    routines.splice(currentRoutineIndex, 1);
    currentRoutineIndex = -1;
    updateRoutineList();
    document.getElementById('routine-editor').style.display = 'none';
}

function loadSelectedRoutine() {
    const index = document.getElementById('routine-list').value;
    if (index === "") {
        document.getElementById('routine-editor').style.display = 'none';
        currentRoutineIndex = -1;
        return;
    }
    
    currentRoutineIndex = parseInt(index);
    const routine = routines[currentRoutineIndex];
    document.getElementById('routine-name').value = routine.name;
    document.getElementById('routine-editor').style.display = 'block';
    renderSteps();
}

function renderSteps() {
    const container = document.getElementById('routine-steps');
    container.innerHTML = '';
    
    const routine = routines[currentRoutineIndex];
    // Sort steps by order
    const sortedSteps = [...routine.steps].sort((a, b) => a.order - b.order);
    
    sortedSteps.forEach((step, idx) => {
        const stepEl = document.createElement('div');
        stepEl.className = 'routine-step-card';
        stepEl.dataset.order = step.order;
        
        stepEl.innerHTML = `
            <div class="step-info">
                <span class="step-name">${step.name}</span>
                <div class="step-controls">
                    <button class="btn-step-adjust" onclick="adjustDuration(${step.order}, -1)">-</button>
                    <input type="number" value="${step.duration}" min="1" max="20" onchange="updateStepDuration(${step.order}, this.value)">
                    <button class="btn-step-adjust" onclick="adjustDuration(${step.order}, 1)">+</button>
                    <span>minutes</span>
                </div>
            </div>
            <div class="step-actions">
                <button class="btn-icon" onclick="moveStep(${step.order}, -1)" ${idx === 0 ? 'disabled' : ''}>${ICONS.up}</button>
                <button class="btn-icon" onclick="moveStep(${step.order}, 1)" ${idx === sortedSteps.length - 1 ? 'disabled' : ''}>${ICONS.down}</button>
                <button class="btn-remove-step" onclick="removeStep(${step.order})" title="Remove Step">${ICONS.trash}</button>
            </div>
        `;
        container.appendChild(stepEl);
    });
}

function updateStepDuration(order, value) {
    const routine = routines[currentRoutineIndex];
    const step = routine.steps.find(s => s.order === order);
    if (step) {
        const newVal = Math.max(1, Math.min(20, parseInt(value) || 1));
        step.duration = newVal;
        
        // Update DOM directly instead of renderSteps()
        const container = document.getElementById('routine-steps');
        const card = Array.from(container.querySelectorAll('.routine-step-card'))
            .find(c => parseInt(c.dataset.order) === order);
        if (card) {
            const input = card.querySelector('input');
            if (input && input.value != newVal) {
                input.value = newVal;
            }
        }
    }
}

function adjustDuration(order, delta) {
    const routine = routines[currentRoutineIndex];
    const step = routine.steps.find(s => s.order === order);
    if (step) {
        const newVal = Math.max(1, Math.min(20, step.duration + delta));
        if (newVal !== step.duration) {
            step.duration = newVal;
            
            // Update DOM directly instead of renderSteps()
            const container = document.getElementById('routine-steps');
            const card = Array.from(container.querySelectorAll('.routine-step-card'))
                .find(c => parseInt(c.dataset.order) === order);
            if (card) {
                const input = card.querySelector('input');
                if (input) {
                    input.value = newVal;
                }
            }
        }
    }
}

function showStationPicker() {
    const options = document.getElementById('station-options');
    options.innerHTML = '';
    RELAY_NAMES.forEach((name, id) => {
        const div = document.createElement('div');
        div.className = 'station-option';
        div.textContent = name;
        div.onclick = () => addStep(id, name);
        options.appendChild(div);
    });
    document.getElementById('station-modal').style.display = 'flex';
}

function closeStationPicker() {
    document.getElementById('station-modal').style.display = 'none';
}

function addStep(id, name) {
    const routine = routines[currentRoutineIndex];
    const newOrder = routine.steps.length > 0 ? Math.max(...routine.steps.map(s => s.order)) + 1 : 0;
    routine.steps.push({
        id,
        name,
        duration: 5,
        enabled: true,
        order: newOrder
    });
    closeStationPicker();
    renderSteps();
}

function removeStep(order) {
    const routine = routines[currentRoutineIndex];
    const index = routine.steps.findIndex(s => s.order === order);
    if (index !== -1) {
        routine.steps.splice(index, 1);
        // Normalize orders
        routine.steps.sort((a, b) => a.order - b.order).forEach((s, i) => s.order = i);
        renderSteps();
    }
}

function updateStep(order, field, value) {
    const step = routines[currentRoutineIndex].steps.find(s => s.order === order);
    if (field === 'duration') value = parseInt(value);
    step[field] = value;
}

async function moveStep(order, direction) {
    const routine = routines[currentRoutineIndex];
    const steps = routine.steps;
    const idx = steps.findIndex(s => s.order === order);
    const otherOrder = order + direction;
    const otherIdx = steps.findIndex(s => s.order === otherOrder);
    
    if (otherIdx !== -1) {
        const container = document.getElementById('routine-steps');
        const cards = Array.from(container.querySelectorAll('.routine-step-card'));
        
        // Capture all card positions for FLIP
        const cardPositions = cards.map(card => ({
            el: card,
            order: parseInt(card.dataset.order),
            rect: card.getBoundingClientRect()
        }));

        // Swap orders in data
        const tempOrder = steps[idx].order;
        steps[idx].order = steps[otherIdx].order;
        steps[otherIdx].order = tempOrder;

        // Sort steps by order
        steps.sort((a, b) => a.order - b.order);

        // Re-render
        renderSteps();

        // FLIP: Play
        const newCards = Array.from(container.querySelectorAll('.routine-step-card'));
        newCards.forEach(newCard => {
            const newOrder = parseInt(newCard.dataset.order);
            const oldPos = cardPositions.find(p => p.order === newOrder);
            
            if (oldPos) {
                const newRect = newCard.getBoundingClientRect();
                const deltaY = oldPos.rect.top - newRect.top;

                if (deltaY !== 0) {
                    newCard.style.transition = 'none';
                    newCard.style.transform = `translateY(${deltaY}px)`;
                    
                    // Trigger reflow
                    newCard.offsetHeight;

                    requestAnimationFrame(() => {
                        newCard.style.transition = 'transform 0.4s cubic-bezier(0.2, 0, 0, 1)';
                        newCard.style.transform = '';
                        newCard.classList.add('moving');
                        
                        setTimeout(() => {
                            newCard.style.transition = '';
                            newCard.classList.remove('moving');
                        }, 400);
                    });
                }
            }
        });
    }
}

async function saveRoutines() {
    const routine = routines[currentRoutineIndex];
    routine.name = document.getElementById('routine-name').value;
    updateRoutineList();

    try {
        const response = await fetch('/api/routines', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(routines)
        });
        if (response.ok) {
            showToast("Routines saved", "success");
        } else {
            showToast("Failed to save routines, error was: " + response.statusText);
        }
    } catch (e) {
        showToast("Error saving routines: " + e.message);
    }
}

document.addEventListener('DOMContentLoaded', fetchRoutines);
