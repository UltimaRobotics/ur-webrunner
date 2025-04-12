// Global variables
let cpuChart, memoryChart, storageChart, bandwidthChart;
let chartUpdateInterval;

// Initialize the dashboard on page load
document.addEventListener('DOMContentLoaded', function() {
    // Show initial tab
    showTab('dashboard');
    
    // Initialize charts if we're on the dashboard
    if (document.getElementById('dashboard')?.classList.contains('active')) {
        initializeCharts();
    }
    
    // Set up event listeners
    setupEventListeners();
    
    // Set up popup event listeners
    setupPopupEventListeners();
});

// Setup navigation and interactive elements
function setupEventListeners() {
    // Tab navigation
    document.querySelectorAll('.sidebar-menu li').forEach(item => {
        item.addEventListener('click', function() {
            const tabId = this.getAttribute('data-tab');
            if (tabId) {
                showTab(tabId);
                
                // Initialize charts if switching to dashboard
                if (tabId === 'dashboard' && !cpuChart) {
                    initializeCharts();
                }
            }
        });
    });
    
    // Developer button event
    document.getElementById('developer-btn')?.addEventListener('click', function() {
        showDeveloperPopup();
    });
    
    // Firmware action buttons
    document.getElementById('factory-reset-btn')?.addEventListener('click', showFactoryResetPopup);
    document.getElementById('backup-settings-btn')?.addEventListener('click', showBackupSettingsPopup);
    document.getElementById('online-update-btn')?.addEventListener('click', showOnlineUpdatePopup);
}

// Set up popup event listeners
function setupPopupEventListeners() {
    // Factory Reset Popup
    document.getElementById('factoryResetClose')?.addEventListener('click', hideFactoryResetPopup);
    document.getElementById('factoryResetOverlay')?.addEventListener('click', hideFactoryResetPopup);
    document.getElementById('cancelResetBtn')?.addEventListener('click', hideFactoryResetPopup);
    
    // Setup checkbox and password validation for Factory Reset
    const confirmResetCheckbox = document.getElementById('confirmResetCheckbox');
    const adminPasswordReset = document.getElementById('adminPasswordReset');
    const confirmResetBtn = document.getElementById('confirmResetBtn');
    
    if (confirmResetCheckbox && adminPasswordReset && confirmResetBtn) {
        // Enable confirm button only when checkbox is checked and password is entered
        function validateResetForm() {
            confirmResetBtn.disabled = !(confirmResetCheckbox.checked && adminPasswordReset.value.length > 0);
        }
        
        confirmResetCheckbox.addEventListener('change', validateResetForm);
        adminPasswordReset.addEventListener('input', validateResetForm);
        
        // Factory Reset confirmation button
        confirmResetBtn.addEventListener('click', performFactoryReset);
    }
    
    // Backup Settings Popup
    document.getElementById('backupSettingsClose')?.addEventListener('click', hideBackupSettingsPopup);
    document.getElementById('backupSettingsOverlay')?.addEventListener('click', hideBackupSettingsPopup);
    document.getElementById('startBackupBtn')?.addEventListener('click', performBackupSettings);
    
    // Online Update Popup
    document.getElementById('onlineUpdateClose')?.addEventListener('click', hideOnlineUpdatePopup);
    document.getElementById('onlineUpdateOverlay')?.addEventListener('click', hideOnlineUpdatePopup);
    document.getElementById('checkUpdatesBtn')?.addEventListener('click', checkForUpdates);
    document.getElementById('startUpdateBtn')?.addEventListener('click', performOnlineUpdate);
    
    // GitHub source option toggle
    const sourceGithub = document.getElementById('sourceGithub');
    const sourceOfficial = document.getElementById('sourceOfficial');
    const githubSourceOptions = document.getElementById('githubSourceOptions');
    
    if (sourceGithub && sourceOfficial && githubSourceOptions) {
        sourceGithub.addEventListener('change', function() {
            if (this.checked) {
                githubSourceOptions.style.display = 'block';
            }
        });
        
        sourceOfficial.addEventListener('change', function() {
            if (this.checked) {
                githubSourceOptions.style.display = 'none';
            }
        });
    }
    
    // Developer Popup
    document.getElementById('developerClose')?.addEventListener('click', hideDeveloperPopup);
    document.getElementById('developerOverlay')?.addEventListener('click', hideDeveloperPopup);
    
    // Developer tabs
    document.querySelectorAll('.dev-tab').forEach(tab => {
        tab.addEventListener('click', function() {
            const tabId = this.getAttribute('data-tab');
            
            // Hide all tab panes
            document.querySelectorAll('.tab-pane').forEach(pane => {
                pane.classList.remove('active');
            });
            
            // Remove active class from all tabs
            document.querySelectorAll('.dev-tab').forEach(t => {
                t.classList.remove('active');
            });
            
            // Show selected tab pane
            document.getElementById(tabId + 'Tab').classList.add('active');
            
            // Add active class to clicked tab
            this.classList.add('active');
        });
    });
}

// Show the selected tab
function showTab(tabId) {
    // Hide all tab contents
    document.querySelectorAll('.tab-content').forEach(tab => {
        tab.classList.remove('active');
    });
    
    // Show selected tab content
    const selectedTab = document.getElementById(tabId);
    if (selectedTab) {
        selectedTab.classList.add('active');
    }
    
    // Update active menu item
    document.querySelectorAll('.sidebar-menu li').forEach(item => {
        item.classList.remove('active');
        if (item.getAttribute('data-tab') === tabId) {
            item.classList.add('active');
        }
    });
    
    // Start or stop metrics updates based on active tab
    if (tabId === 'dashboard') {
        startMetricsUpdates();
    } else {
        stopMetricsUpdates();
    }
}

// Initialize charts for real-time metrics
function initializeCharts() {
    // CPU Usage Chart
    const cpuCtx = document.getElementById('cpuChart')?.getContext('2d');
    if (cpuCtx) {
        cpuChart = new Chart(cpuCtx, {
            type: 'line',
            data: {
                labels: Array(30).fill(''),
                datasets: [{
                    label: 'CPU Usage %',
                    data: Array(30).fill(0),
                    borderColor: '#0066cc',
                    backgroundColor: 'rgba(0, 102, 204, 0.1)',
                    borderWidth: 2,
                    fill: true,
                    tension: 0.4
                }]
            },
            options: getChartOptions('CPU Usage %')
        });
    }
    
    // Memory Usage Chart
    const memCtx = document.getElementById('memoryChart')?.getContext('2d');
    if (memCtx) {
        memoryChart = new Chart(memCtx, {
            type: 'line',
            data: {
                labels: Array(30).fill(''),
                datasets: [{
                    label: 'Memory Usage %',
                    data: Array(30).fill(0),
                    borderColor: '#e63946',
                    backgroundColor: 'rgba(230, 57, 70, 0.1)',
                    borderWidth: 2,
                    fill: true,
                    tension: 0.4
                }]
            },
            options: getChartOptions('Memory Usage %')
        });
    }
    
    // Storage Usage Chart
    const storageCtx = document.getElementById('storageChart')?.getContext('2d');
    if (storageCtx) {
        storageChart = new Chart(storageCtx, {
            type: 'doughnut',
            data: {
                labels: ['Used', 'Free'],
                datasets: [{
                    data: [0, 100],
                    backgroundColor: ['#ff9800', '#e0e0e0'],
                    borderWidth: 0
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                cutout: '70%',
                plugins: {
                    legend: {
                        position: 'bottom'
                    }
                }
            }
        });
    }
    
    // Network Bandwidth Chart
    const bandwidthCtx = document.getElementById('bandwidthChart')?.getContext('2d');
    if (bandwidthCtx) {
        bandwidthChart = new Chart(bandwidthCtx, {
            type: 'bar',
            data: {
                labels: ['Download', 'Upload'],
                datasets: [{
                    label: 'Bandwidth (KB/s)',
                    data: [0, 0],
                    backgroundColor: ['#4CAF50', '#2196F3'],
                    borderWidth: 0
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    y: {
                        beginAtZero: true
                    }
                }
            }
        });
    }
    
    // Start the metrics updates
    startMetricsUpdates();
}

// Common chart options
function getChartOptions(title) {
    return {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
            title: {
                display: true,
                text: title
            }
        },
        scales: {
            y: {
                beginAtZero: true,
                max: 100,
                ticks: {
                    callback: function(value) {
                        return value + '%';
                    }
                }
            },
            x: {
                display: false
            }
        },
        animation: {
            duration: 300
        }
    };
}

// Start regular updates of metrics
function startMetricsUpdates() {
    if (!chartUpdateInterval) {
        // Initial update
        updateMetrics();
        
        // Set interval for updates (every 2 seconds)
        chartUpdateInterval = setInterval(updateMetrics, 2000);
    }
}

// Stop updating metrics
function stopMetricsUpdates() {
    if (chartUpdateInterval) {
        clearInterval(chartUpdateInterval);
        chartUpdateInterval = null;
    }
}

// Update all metrics
function updateMetrics() {
    // Fetch metrics data from the server
    fetch('/api/metrics')
        .then(response => response.json())
        .then(data => {
            updateCPUMetrics(data.cpu);
            updateMemoryMetrics(data.memory);
            updateStorageMetrics(data.storage);
            updateBandwidthMetrics(data.bandwidth);
            updateConnectionStatusIcons(data.internet, data.ultima_server);
        })
        .catch(error => {
            console.error('Error fetching metrics:', error);
        });
}

// Update CPU metrics
function updateCPUMetrics(cpuData) {
    if (!cpuChart) return;
    
    // Update chart
    const data = cpuChart.data.datasets[0].data;
    data.shift();
    data.push(cpuData.usage);
    cpuChart.update();
    
    // Update numeric display
    const cpuValueElement = document.getElementById('cpuValue');
    if (cpuValueElement) {
        cpuValueElement.textContent = cpuData.usage + '%';
    }
    
    // Update progress bar
    const progressBar = document.getElementById('cpuProgressBar');
    if (progressBar) {
        progressBar.style.width = cpuData.usage + '%';
    }
}

// Update Memory metrics
function updateMemoryMetrics(memData) {
    if (!memoryChart) return;
    
    // Update chart
    const data = memoryChart.data.datasets[0].data;
    data.shift();
    data.push(memData.usage);
    memoryChart.update();
    
    // Update numeric display
    const memoryValueElement = document.getElementById('memoryValue');
    const memoryDetailsElement = document.getElementById('memoryDetails');
    
    if (memoryValueElement) {
        memoryValueElement.textContent = memData.usage + '%';
    }
    
    if (memoryDetailsElement) {
        memoryDetailsElement.textContent = `${memData.used}MB / ${memData.total}MB`;
    }
    
    // Update progress bar
    const progressBar = document.getElementById('memoryProgressBar');
    if (progressBar) {
        progressBar.style.width = memData.usage + '%';
    }
}

// Update Storage metrics
function updateStorageMetrics(storageData) {
    if (!storageChart) return;
    
    // Update chart
    storageChart.data.datasets[0].data = [storageData.used, storageData.free];
    storageChart.update();
    
    // Update numeric display
    const storageValueElement = document.getElementById('storageValue');
    const storageDetailsElement = document.getElementById('storageDetails');
    
    if (storageValueElement) {
        storageValueElement.textContent = storageData.usage + '%';
    }
    
    if (storageDetailsElement) {
        storageDetailsElement.textContent = 
            `${storageData.used_formatted} / ${storageData.total_formatted}`;
    }
    
    // Update progress bar
    const progressBar = document.getElementById('storageProgressBar');
    if (progressBar) {
        progressBar.style.width = storageData.usage + '%';
    }
}

// Update Bandwidth metrics
function updateBandwidthMetrics(bandwidthData) {
    if (!bandwidthChart) return;
    
    // Update chart
    bandwidthChart.data.datasets[0].data = [
        bandwidthData.download, 
        bandwidthData.upload
    ];
    bandwidthChart.update();
    
    // Update numeric displays
    const downloadValueElement = document.getElementById('downloadValue');
    const uploadValueElement = document.getElementById('uploadValue');
    
    if (downloadValueElement) {
        downloadValueElement.textContent = bandwidthData.download + ' KB/s';
    }
    
    if (uploadValueElement) {
        uploadValueElement.textContent = bandwidthData.upload + ' KB/s';
    }
}

// Update connection status icons
function updateConnectionStatusIcons(internet, ultimaServer) {
    // Internet connection
    const internetIcon = document.getElementById('internetStatusIcon');
    const internetText = document.getElementById('internetStatusText');
    
    if (internetIcon && internetText) {
        if (internet.connected) {
            internetIcon.classList.add('connected');
            internetText.textContent = 'Connected';
            internetText.classList.add('connected');
        } else {
            internetIcon.classList.remove('connected');
            internetText.textContent = 'Disconnected';
            internetText.classList.remove('connected');
        }
    }
    
    // Ultima server connection
    const ultimaIcon = document.getElementById('ultimaStatusIcon');
    const ultimaText = document.getElementById('ultimaStatusText');
    
    if (ultimaIcon && ultimaText) {
        if (ultimaServer.connected) {
            ultimaIcon.classList.add('connected');
            ultimaText.textContent = 'Connected';
            ultimaText.classList.add('connected');
        } else {
            ultimaIcon.classList.remove('connected');
            ultimaText.textContent = 'Disconnected';
            ultimaText.classList.remove('connected');
        }
    }
}

// Factory Reset Popup Functions
function showFactoryResetPopup() {
    document.getElementById('factoryResetOverlay').style.display = 'block';
    document.getElementById('factoryResetPopup').style.display = 'block';
    document.getElementById('adminPasswordReset').value = '';
    document.getElementById('confirmResetCheckbox').checked = false;
    document.getElementById('confirmResetBtn').disabled = true;
}

function hideFactoryResetPopup() {
    document.getElementById('factoryResetOverlay').style.display = 'none';
    document.getElementById('factoryResetPopup').style.display = 'none';
}

function performFactoryReset() {
    const password = document.getElementById('adminPasswordReset').value;
    
    // Show confirmation message
    const confirmMessage = "Are you absolutely sure you want to reset this device to factory settings? This action cannot be undone!";
    if (!confirm(confirmMessage)) {
        return;
    }
    
    // Here we would normally make an API call to perform the factory reset
    // For now, we'll just simulate it
    
    // Disable the button to prevent multiple clicks
    document.getElementById('confirmResetBtn').disabled = true;
    document.getElementById('confirmResetBtn').textContent = 'Resetting...';
    
    // Simulate API call
    setTimeout(() => {
        alert("Factory reset initiated. The device will restart and be unavailable for several minutes.");
        hideFactoryResetPopup();
    }, 2000);
    
    // In a real implementation, we would send a POST request to the server
    /*
    fetch('/api/system/factory-reset', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ password: password })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            alert("Factory reset initiated. The device will restart and be unavailable for several minutes.");
            hideFactoryResetPopup();
        } else {
            alert("Error: " + data.error);
            document.getElementById('confirmResetBtn').disabled = false;
            document.getElementById('confirmResetBtn').textContent = 'Reset Device';
        }
    })
    .catch(error => {
        console.error('Error:', error);
        alert("An error occurred while attempting to reset the device.");
        document.getElementById('confirmResetBtn').disabled = false;
        document.getElementById('confirmResetBtn').textContent = 'Reset Device';
    });
    */
}

// Backup Settings Popup Functions
function showBackupSettingsPopup() {
    document.getElementById('backupSettingsOverlay').style.display = 'block';
    document.getElementById('backupSettingsPopup').style.display = 'block';
    
    // Default all checkboxes to checked
    document.querySelectorAll('#backupSystem, #backupNetwork, #backupFirewall, #backupServices').forEach(checkbox => {
        checkbox.checked = true;
    });
    
    // Reset password fields
    document.getElementById('backupPassword').value = '';
    document.getElementById('confirmBackupPassword').value = '';
    
    // Set default backup filename with current date and time
    const now = new Date();
    const dateStr = now.toISOString().slice(0, 10).replace(/-/g, '');
    const timeStr = now.toTimeString().slice(0, 8).replace(/:/g, '');
    document.getElementById('backupFileName').value = `openwrt-backup-${dateStr}-${timeStr}`;
}

function hideBackupSettingsPopup() {
    document.getElementById('backupSettingsOverlay').style.display = 'none';
    document.getElementById('backupSettingsPopup').style.display = 'none';
}

function performBackupSettings() {
    // Get selected options
    const backupSystem = document.getElementById('backupSystem').checked;
    const backupNetwork = document.getElementById('backupNetwork').checked;
    const backupFirewall = document.getElementById('backupFirewall').checked;
    const backupServices = document.getElementById('backupServices').checked;
    const backupPackages = document.getElementById('backupPackages').checked;
    const backupData = document.getElementById('backupData').checked;
    
    // Get password if encryption is enabled
    const password = document.getElementById('backupPassword').value;
    const confirmPassword = document.getElementById('confirmBackupPassword').value;
    
    // Check password match if provided
    if (password && password !== confirmPassword) {
        alert("Passwords do not match");
        return;
    }
    
    // Get filename
    const fileName = document.getElementById('backupFileName').value;
    
    // Disable the button to prevent multiple clicks
    const startBackupBtn = document.getElementById('startBackupBtn');
    startBackupBtn.disabled = true;
    startBackupBtn.textContent = 'Creating Backup...';
    
    // Update progress bar
    const progressBar = document.getElementById('backupProgressBar');
    let progress = 0;
    
    const progressInterval = setInterval(() => {
        progress += 5;
        progressBar.style.width = `${progress}%`;
        
        if (progress >= 100) {
            clearInterval(progressInterval);
            
            // After simulated backup is complete
            setTimeout(() => {
                alert(`Backup created successfully: ${fileName}.tar.gz`);
                
                // In a real implementation, we would initiate a download of the backup file
                // window.location.href = `/api/system/backup/download/${fileName}.tar.gz`;
                
                // Reset UI
                startBackupBtn.disabled = false;
                startBackupBtn.textContent = 'Create Backup';
                progressBar.style.width = '0%';
                
                // Close popup
                hideBackupSettingsPopup();
            }, 500);
        }
    }, 200);
    
    // In a real implementation, we would send a POST request to the server
    /*
    fetch('/api/system/backup', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            fileName: fileName,
            options: {
                system: backupSystem,
                network: backupNetwork,
                firewall: backupFirewall,
                services: backupServices,
                packages: backupPackages,
                data: backupData
            },
            encryption: password ? { enabled: true, password: password } : { enabled: false }
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            alert(`Backup created successfully: ${data.fileName}`);
            window.location.href = `/api/system/backup/download/${data.fileName}`;
            hideBackupSettingsPopup();
        } else {
            alert("Error: " + data.error);
        }
        
        startBackupBtn.disabled = false;
        startBackupBtn.textContent = 'Create Backup';
        progressBar.style.width = '0%';
    })
    .catch(error => {
        console.error('Error:', error);
        alert("An error occurred while creating the backup.");
        startBackupBtn.disabled = false;
        startBackupBtn.textContent = 'Create Backup';
        progressBar.style.width = '0%';
    });
    */
}

// Online Update Popup Functions
function showOnlineUpdatePopup() {
    document.getElementById('onlineUpdateOverlay').style.display = 'block';
    document.getElementById('onlineUpdatePopup').style.display = 'block';
    
    // Reset form
    document.getElementById('sourceOfficial').checked = true;
    document.getElementById('githubSourceOptions').style.display = 'none';
    document.getElementById('preserveSettings').checked = true;
    document.getElementById('backupBeforeUpdate').checked = true;
    document.getElementById('updatePackages').checked = false;
    document.getElementById('startUpdateBtn').disabled = true;
    
    // Reset progress indicators
    document.querySelectorAll('.step').forEach(step => {
        step.classList.remove('active', 'complete', 'error');
        step.querySelector('.step-status').textContent = 'Pending';
    });
    
    document.getElementById('updateProgressBar').style.width = '0%';
    document.getElementById('updateProgressStatus').textContent = 'Ready to start';
    
    // Hide any previous update results
    document.getElementById('availableUpdates').innerHTML = `
        <h4>Available Updates</h4>
        <div class="loading-indicator">
            <div class="spinner"></div>
            <span>Click "Check for Updates" to begin</span>
        </div>
    `;
}

function hideOnlineUpdatePopup() {
    document.getElementById('onlineUpdateOverlay').style.display = 'none';
    document.getElementById('onlineUpdatePopup').style.display = 'none';
}

function checkForUpdates() {
    // Get update source
    const isGithubSource = document.getElementById('sourceGithub').checked;
    
    // Update UI to show checking status
    document.getElementById('availableUpdates').innerHTML = `
        <h4>Available Updates</h4>
        <div class="loading-indicator">
            <div class="spinner"></div>
            <span>Checking for updates...</span>
        </div>
    `;
    
    // Update steps UI
    const step1 = document.querySelector('.step[data-step="1"]');
    step1.classList.add('active');
    step1.querySelector('.step-status').textContent = 'In Progress';
    
    // Simulate API call to check for updates
    setTimeout(() => {
        // Mark step 1 as complete
        step1.classList.remove('active');
        step1.classList.add('complete');
        step1.querySelector('.step-status').textContent = 'Complete';
        
        // Update available updates section
        let updatesHtml;
        
        if (isGithubSource) {
            // GitHub updates
            const repoUrl = document.getElementById('githubRepoUrl').value || 'https://github.com/example/repo';
            const branch = document.getElementById('githubBranch').value || 'main';
            
            updatesHtml = `
                <h4>Available Updates from GitHub</h4>
                <div class="update-list">
                    <div class="update-item">
                        <div class="update-info">
                            <div class="update-title">Latest commit: a1b2c3d</div>
                            <div class="update-description">feat: Updated firmware with latest improvements</div>
                            <div class="update-details">Repository: ${repoUrl}</div>
                            <div class="update-details">Branch: ${branch}</div>
                            <div class="update-details">Last Updated: April 12, 2025</div>
                        </div>
                        <div class="update-actions">
                            <button class="select-update-btn">Select</button>
                        </div>
                    </div>
                </div>
            `;
        } else {
            // Official repository updates
            updatesHtml = `
                <h4>Available Updates</h4>
                <div class="update-list">
                    <div class="update-item">
                        <div class="update-info">
                            <div class="update-title">OpenWRT 22.03.0-rc1</div>
                            <div class="update-description">This is a release candidate for the upcoming stable version.</div>
                            <div class="update-details">Version: 22.03.0-rc1</div>
                            <div class="update-details">Release Date: April 10, 2025</div>
                            <div class="update-details">Size: 12.5 MB</div>
                        </div>
                        <div class="update-actions">
                            <button class="select-update-btn">Select</button>
                        </div>
                    </div>
                </div>
            `;
        }
        
        document.getElementById('availableUpdates').innerHTML = updatesHtml;
        
        // Add event listener to the select button
        document.querySelector('.select-update-btn').addEventListener('click', function() {
            this.textContent = 'Selected ✓';
            this.disabled = true;
            document.getElementById('startUpdateBtn').disabled = false;
        });
    }, 2000);
}

function performOnlineUpdate() {
    // Get update options
    const isGithubSource = document.getElementById('sourceGithub').checked;
    const preserveSettings = document.getElementById('preserveSettings').checked;
    const backupBeforeUpdate = document.getElementById('backupBeforeUpdate').checked;
    const updatePackages = document.getElementById('updatePackages').checked;
    
    // Disable start button to prevent multiple clicks
    const startUpdateBtn = document.getElementById('startUpdateBtn');
    startUpdateBtn.disabled = true;
    startUpdateBtn.textContent = 'Updating...';
    
    // Update progress status
    document.getElementById('updateProgressStatus').textContent = 'Update in progress...';
    
    // Update step 1 (already completed from check)
    const step1 = document.querySelector('.step[data-step="1"]');
    step1.classList.add('complete');
    step1.querySelector('.step-status').textContent = 'Complete';
    
    // Update progress bar to 25%
    document.getElementById('updateProgressBar').style.width = '25%';
    
    // Simulate download (step 2)
    const step2 = document.querySelector('.step[data-step="2"]');
    step2.classList.add('active');
    step2.querySelector('.step-status').textContent = 'Downloading...';
    
    setTimeout(() => {
        // Complete step 2
        step2.classList.remove('active');
        step2.classList.add('complete');
        step2.querySelector('.step-status').textContent = 'Complete';
        
        // Update progress bar to 50%
        document.getElementById('updateProgressBar').style.width = '50%';
        
        // Start step 3 (verification)
        const step3 = document.querySelector('.step[data-step="3"]');
        step3.classList.add('active');
        step3.querySelector('.step-status').textContent = 'Verifying...';
        
        setTimeout(() => {
            // Complete step 3
            step3.classList.remove('active');
            step3.classList.add('complete');
            step3.querySelector('.step-status').textContent = 'Complete';
            
            // Update progress bar to 75%
            document.getElementById('updateProgressBar').style.width = '75%';
            
            // Start step 4 (installation)
            const step4 = document.querySelector('.step[data-step="4"]');
            step4.classList.add('active');
            step4.querySelector('.step-status').textContent = 'Installing...';
            
            setTimeout(() => {
                // Complete step 4
                step4.classList.remove('active');
                step4.classList.add('complete');
                step4.querySelector('.step-status').textContent = 'Complete';
                
                // Update progress bar to 100%
                document.getElementById('updateProgressBar').style.width = '100%';
                document.getElementById('updateProgressStatus').textContent = 'Update completed successfully!';
                
                // Show completion message
                alert("Update completed successfully! The device will now restart to apply the changes.");
                
                // Reset the start button
                startUpdateBtn.disabled = false;
                startUpdateBtn.textContent = 'Start Update';
                
                // Close the popup
                hideOnlineUpdatePopup();
                
                // In a real implementation, the device would restart here
            }, 3000);
        }, 2000);
    }, 3000);
    
    // In a real implementation, we would send a POST request to the server
    /*
    fetch('/api/firmware/update', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            source: isGithubSource ? 'github' : 'official',
            github: isGithubSource ? {
                repo: document.getElementById('githubRepoUrl').value,
                branch: document.getElementById('githubBranch').value,
                token: document.getElementById('githubToken').value
            } : null,
            options: {
                preserveSettings: preserveSettings,
                backupBeforeUpdate: backupBeforeUpdate,
                updatePackages: updatePackages
            }
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            alert("Update completed successfully! The device will now restart to apply the changes.");
            hideOnlineUpdatePopup();
        } else {
            alert("Error: " + data.error);
            
            // Mark the current step as error
            const currentStep = document.querySelector('.step.active');
            if (currentStep) {
                currentStep.classList.remove('active');
                currentStep.classList.add('error');
                currentStep.querySelector('.step-status').textContent = 'Failed: ' + data.error;
            }
            
            document.getElementById('updateProgressStatus').textContent = 'Update failed: ' + data.error;
        }
        
        startUpdateBtn.disabled = false;
        startUpdateBtn.textContent = 'Start Update';
    })
    .catch(error => {
        console.error('Error:', error);
        alert("An error occurred during the update process.");
        
        // Mark the current step as error
        const currentStep = document.querySelector('.step.active');
        if (currentStep) {
            currentStep.classList.remove('active');
            currentStep.classList.add('error');
            currentStep.querySelector('.step-status').textContent = 'Failed: Network error';
        }
        
        document.getElementById('updateProgressStatus').textContent = 'Update failed: Network error';
        startUpdateBtn.disabled = false;
        startUpdateBtn.textContent = 'Start Update';
    });
    */
}

// Developer Popup Functions
function showDeveloperPopup() {
    document.getElementById('developerOverlay').style.display = 'block';
    document.getElementById('developerPopup').style.display = 'block';
    
    // Reset active tab
    document.querySelectorAll('.dev-tab').forEach(tab => {
        tab.classList.remove('active');
    });
    document.querySelectorAll('.tab-pane').forEach(pane => {
        pane.classList.remove('active');
    });
    
    // Set first tab as active
    document.querySelector('.dev-tab[data-tab="mqtt"]').classList.add('active');
    document.getElementById('mqttTab').classList.add('active');
    
    // Load system debug info
    const debugSystemInfo = document.getElementById('debugSystemInfo');
    if (debugSystemInfo) {
        debugSystemInfo.textContent = 'Loading system information...';
        
        // In a real implementation, we would fetch this data from the server
        setTimeout(() => {
            debugSystemInfo.textContent = `
OpenWRT: OpenWrt 22.03.0
Architecture: arm_cortex-a53_neon-vfpv4
Kernel: 5.10.120
Uptime: 3 days, 7 hours, 12 minutes
CPU: Quad-core ARM Cortex-A53
Memory: 512 MB
Flash: 128 MB
IP: 192.168.1.1
MAC: AA:BB:CC:DD:EE:FF
            `.trim();
        }, 1000);
    }
}

function hideDeveloperPopup() {
    document.getElementById('developerOverlay').style.display = 'none';
    document.getElementById('developerPopup').style.display = 'none';
}