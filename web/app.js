const RELAY_NAMES = ['Plants', 'Front Lawn', 'Grass', 'Patio Grass'];

async function toggleRelay(id, action) {
    const card = document.getElementById('relay-' + id);
    const status = document.getElementById('status-' + id);
    const buttons = card.querySelectorAll('button');

    buttons.forEach(btn => {
        btn.disabled = true;
        btn.classList.add('loading');
    });

    try {
        const response = await fetch(`/api/relay?id=${id}&action=${action}`);
        const data = await response.json();

        if (data.success) {
            const isOn = data.state === 'on';
            status.textContent = isOn ? 'ON' : 'OFF';
            status.className = isOn ? 'status on' : 'status off';
        }
    } catch (error) {
        console.error('Error:', error);
        alert('Failed to control relay. Please try again.');
    } finally {
        buttons.forEach(btn => {
            btn.disabled = false;
            btn.classList.remove('loading');
        });
    }
}

async function updateStatus() {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();

        data.relays.forEach(relay => {
            const status = document.getElementById('status-' + relay.id);
            if (status) {
                const isOn = relay.state === 'on';
                status.textContent = isOn ? 'ON' : 'OFF';
                status.className = isOn ? 'status on' : 'status off';
            }
        });
    } catch (error) {
        console.error('Status update error:', error);
    }
}

function createRelayCards() {
    const container = document.getElementById('relays');

    for (let i = 0; i < 4; i++) {
        const card = document.createElement('div');
        card.className = 'card';
        card.id = 'relay-' + i;

        card.innerHTML = `
            <div class="relay-header">
                <span class="relay-name">${RELAY_NAMES[i]}</span>
                <span class="status off" id="status-${i}">OFF</span>
            </div>
            <div class="btn-group">
                <button class="btn-on" onclick="toggleRelay(${i}, 'on')">Turn On</button>
                <button class="btn-off" onclick="toggleRelay(${i}, 'off')">Turn Off</button>
            </div>
        `;

        container.appendChild(card);
    }
}

// Initialize on load
document.addEventListener('DOMContentLoaded', () => {
    createRelayCards();
    updateStatus();
    setInterval(updateStatus, 5000);
});
