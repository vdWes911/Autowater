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

async function startOTA() {
    const fileInput = document.getElementById('ota-file');
    const typeInput = document.querySelector('input[name="update-type"]:checked');
    const btn = document.getElementById('ota-btn');
    const progressBar = document.getElementById('ota-progress-bar');
    const percentText = document.getElementById('ota-percent');
    const progressContainer = document.getElementById('ota-progress-container');
    
    if (fileInput.files.length === 0) {
        showToast('Please select a file first.');
        return;
    }
    
    const file = fileInput.files[0];
    const type = typeInput.value;
    
    if (!confirm(`Are you sure you want to flash this ${type} update? The device will reboot.`)) {
        return;
    }
    
    btn.disabled = true;
    btn.classList.add('loading');
    progressContainer.style.display = 'block';
    
    const xhr = new XMLHttpRequest();
    xhr.open('POST', `/api/ota?type=${type === 'spiffs' ? 'spiffs' : 'app'}`, true);
    
    xhr.upload.onprogress = (e) => {
        if (e.lengthComputable) {
            const percent = Math.round((e.loaded / e.total) * 100);
            progressBar.style.width = percent + '%';
            percentText.textContent = percent + '%';
        }
    };
    
    xhr.onload = () => {
        btn.classList.remove('loading');
        if (xhr.status === 200) {
            showToast('Update successful! Rebooting...', 'success');
            // Show 100% just in case
            progressBar.style.width = '100%';
            percentText.textContent = '100%';
            
            setTimeout(() => {
                window.location.href = '/';
            }, 5000);
        } else {
            showToast('Update failed: ' + xhr.responseText);
            btn.disabled = false;
        }
    };
    
    xhr.onerror = () => {
        showToast('Connection error during update.');
        btn.disabled = false;
        btn.classList.remove('loading');
    };
    
    xhr.send(file);
}
