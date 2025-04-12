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
    
    // Set up event listeners for terminal menu item (removed from UI)
    document.getElementById('terminal-menu-item')?.addEventListener('click', function() {
        this.style.display = 'none';
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