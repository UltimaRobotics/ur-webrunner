// Global variables
let cpuChart, memoryChart, storageChart, bandwidthChart;
let chartUpdateInterval;

// Initialize the dashboard on page load
document.addEventListener('DOMContentLoaded', function() {
    // Show initial tab
    showTab('dashboard');
    
    // Initialize charts if we're on the dashboard
    if (document.getElementById('dashboard').classList.contains('active')) {
        initializeCharts();
    }
    
    // Check for terminal command in URL
    checkCommandInURL();
    
    // Set up event listeners
    setupEventListeners();
});

// Setup navigation and interactive elements
function setupEventListeners() {
    // Tab navigation
    document.querySelectorAll('.sidebar-menu li').forEach(item => {
        item.addEventListener('click', function() {
            const tabId = this.getAttribute('data-tab');
            showTab(tabId);
            
            // Initialize charts if switching to dashboard
            if (tabId === 'dashboard' && !cpuChart) {
                initializeCharts();
            }
        });
    });
    
    // Terminal controls
    document.getElementById('terminal-btn').addEventListener('click', openTerminal);
    document.getElementById('overlay').addEventListener('click', closeTerminal);
    document.getElementById('terminal-close').addEventListener('click', closeTerminal);
    document.getElementById('terminal-minimize').addEventListener('click', minimizeTerminal);
    document.getElementById('terminal-maximize').addEventListener('click', maximizeTerminal);
    
    // Prevent terminal from closing when clicking inside
    document.getElementById('terminal').addEventListener('click', function(e) {
        e.stopPropagation();
    });
    
    // Terminal form submission
    document.getElementById('terminal-form').addEventListener('submit', function(e) {
        const commandInput = document.getElementById('commandInput');
        if (!commandInput.value.trim()) {
            e.preventDefault();
        }
    });
}

// Show the selected tab
function showTab(tabId) {
    // Hide all tab contents
    document.querySelectorAll('.tab-content').forEach(tab => {
        tab.classList.remove('active');
    });
    
    // Show selected tab content
    document.getElementById(tabId).classList.add('active');
    
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
    const cpuCtx = document.getElementById('cpuChart').getContext('2d');
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
    
    // Memory Usage Chart
    const memCtx = document.getElementById('memoryChart').getContext('2d');
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
    
    // Storage Usage Chart
    const storageCtx = document.getElementById('storageChart').getContext('2d');
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
    
    // Network Bandwidth Chart
    const bandwidthCtx = document.getElementById('bandwidthChart').getContext('2d');
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
            updateConnectionStatus(data.internet, data.ultima_server);
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
    document.getElementById('cpuValue').textContent = cpuData.usage + '%';
    
    // Update progress bar
    const progressBar = document.getElementById('cpuProgressBar');
    progressBar.style.width = cpuData.usage + '%';
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
    document.getElementById('memoryValue').textContent = memData.usage + '%';
    document.getElementById('memoryDetails').textContent = 
        `${memData.used}MB / ${memData.total}MB`;
    
    // Update progress bar
    const progressBar = document.getElementById('memoryProgressBar');
    progressBar.style.width = memData.usage + '%';
}

// Update Storage metrics
function updateStorageMetrics(storageData) {
    if (!storageChart) return;
    
    // Update chart
    storageChart.data.datasets[0].data = [storageData.used, storageData.free];
    storageChart.update();
    
    // Update numeric display
    document.getElementById('storageValue').textContent = storageData.usage + '%';
    document.getElementById('storageDetails').textContent = 
        `${storageData.used_formatted} / ${storageData.total_formatted}`;
    
    // Update progress bar
    const progressBar = document.getElementById('storageProgressBar');
    progressBar.style.width = storageData.usage + '%';
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
    document.getElementById('downloadValue').textContent = 
        bandwidthData.download + ' KB/s';
    document.getElementById('uploadValue').textContent = 
        bandwidthData.upload + ' KB/s';
}

// Update connection status indicators
function updateConnectionStatus(internet, ultimaServer) {
    // Internet connection
    const internetIndicator = document.getElementById('internetStatus');
    const internetText = document.getElementById('internetStatusText');
    
    if (internet.connected) {
        internetIndicator.className = 'status-indicator status-online';
        internetText.textContent = 'Connected';
        internetText.style.color = '#4CAF50';
    } else {
        internetIndicator.className = 'status-indicator status-offline';
        internetText.textContent = 'Disconnected';
        internetText.style.color = '#f44336';
    }
    
    // Ultima server connection
    const ultimaIndicator = document.getElementById('ultimaStatus');
    const ultimaText = document.getElementById('ultimaStatusText');
    
    if (ultimaServer.connected) {
        ultimaIndicator.className = 'status-indicator status-online';
        ultimaText.textContent = 'Connected';
        ultimaText.style.color = '#4CAF50';
    } else {
        ultimaIndicator.className = 'status-indicator status-offline';
        ultimaText.textContent = 'Disconnected';
        ultimaText.style.color = '#f44336';
    }
}

// Check for terminal command in URL
function checkCommandInURL() {
    const urlParams = new URLSearchParams(window.location.search);
    if (urlParams.has('command')) {
        openTerminal();
    }
}

// Terminal functions
function openTerminal() {
    document.getElementById('terminal').style.display = 'flex';
    document.getElementById('overlay').style.display = 'block';
    document.getElementById('commandInput').focus();
    
    const terminalBody = document.getElementById('terminal-body');
    terminalBody.scrollTop = terminalBody.scrollHeight;
}

function closeTerminal() {
    document.getElementById('terminal').style.display = 'none';
    document.getElementById('overlay').style.display = 'none';
}

function minimizeTerminal() {
    closeTerminal();
}

function maximizeTerminal() {
    const terminal = document.getElementById('terminal');
    if (terminal.style.width === '95%' && terminal.style.height === '95%') {
        terminal.style.width = '80%';
        terminal.style.height = '80%';
    } else {
        terminal.style.width = '95%';
        terminal.style.height = '95%';
    }
}