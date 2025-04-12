// Bandwidth Test JavaScript Module
let speedGauge;
let popupSpeedGauge;
let testInProgress = false;
let testInterval;
let historyChart;

document.addEventListener('DOMContentLoaded', function() {
    initializeBandwidthUI();
    
    // Bandwidth Test button event listeners
    const bandwidthTestButtons = document.querySelectorAll('.test-button');
    bandwidthTestButtons.forEach(button => {
        button.addEventListener('click', openBandwidthTest);
    });
    
    document.getElementById('startBandwidthTest')?.addEventListener('click', startBandwidthTest);
    document.getElementById('startPopupBandwidthTest')?.addEventListener('click', startBandwidthTest);
    
    document.getElementById('bandwidthTestClose')?.addEventListener('click', closeBandwidthTest);
    document.getElementById('bandwidthTestOverlay')?.addEventListener('click', closeBandwidthTest);
    
    // Initialize bandwidth history chart if we're on the bandwidth tab
    const bandwidthHistoryCanvas = document.getElementById('bandwidthHistoryChart');
    if (bandwidthHistoryCanvas) {
        initializeBandwidthHistoryChart();
    }
});

// Initialize gauge charts for bandwidth speedometers
function initializeBandwidthUI() {
    // Initialize the main speedometer if exists
    const speedGaugeElement = document.getElementById('speedGauge');
    if (speedGaugeElement) {
        speedGauge = GaugeChart.gaugeChart(speedGaugeElement, {
            hasNeedle: true,
            needleColor: 'rgba(255, 255, 255, 0.8)',
            needleUpdateSpeed: 1000,
            arcColors: ['#00b8fe', '#00ff6b'],
            arcDelimiters: [40],
            arcPadding: 10,
            arcPaddingColor: '#121212',
            arcLabels: ['0', '50', '100'],
            arcLabelFontSize: 16,
            rangeLabel: ['0', '100 Mbps'],
            centralLabel: '',
            rangeLabelFontSize: 14
        });
    }
    
    // Initialize the popup speedometer if exists
    const popupSpeedGaugeElement = document.getElementById('popupSpeedGauge');
    if (popupSpeedGaugeElement) {
        popupSpeedGauge = GaugeChart.gaugeChart(popupSpeedGaugeElement, {
            hasNeedle: true,
            needleColor: 'rgba(255, 255, 255, 0.8)',
            needleUpdateSpeed: 500,
            arcColors: ['#00b8fe', '#00ff6b'],
            arcDelimiters: [40],
            arcPadding: 10,
            arcPaddingColor: 'white',
            arcLabels: ['0', '500', '1000'],
            arcLabelFontSize: 16,
            rangeLabel: ['0', '1000 Mbps'],
            centralLabel: '',
            rangeLabelFontSize: 14
        });
    }
}

// Initialize bandwidth history chart
function initializeBandwidthHistoryChart() {
    const ctx = document.getElementById('bandwidthHistoryChart').getContext('2d');
    
    historyChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: getTimeLabels(24), // Last 24 hours with hourly labels
            datasets: [
                {
                    label: 'Download (Mbps)',
                    data: generateRandomData(24, 500, 950), // Simulated download data 
                    borderColor: '#4CAF50',
                    backgroundColor: 'rgba(76, 175, 80, 0.1)',
                    borderWidth: 2,
                    fill: true,
                    tension: 0.4
                },
                {
                    label: 'Upload (Mbps)',
                    data: generateRandomData(24, 50, 120), // Simulated upload data
                    borderColor: '#2196F3',
                    backgroundColor: 'rgba(33, 150, 243, 0.1)',
                    borderWidth: 2,
                    fill: true,
                    tension: 0.4
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                mode: 'index',
                intersect: false,
            },
            plugins: {
                tooltip: {
                    callbacks: {
                        label: function(context) {
                            return context.dataset.label + ': ' + context.raw.toFixed(1) + ' Mbps';
                        }
                    }
                },
                legend: {
                    position: 'top',
                }
            },
            scales: {
                y: {
                    beginAtZero: true,
                    title: {
                        display: true,
                        text: 'Bandwidth (Mbps)'
                    }
                },
                x: {
                    title: {
                        display: true,
                        text: 'Time'
                    }
                }
            }
        }
    });
}

// Helper function to generate time labels for the bandwidth history chart
function getTimeLabels(count) {
    const labels = [];
    const now = new Date();
    
    for (let i = count - 1; i >= 0; i--) {
        const date = new Date(now);
        date.setHours(now.getHours() - i);
        labels.push(date.getHours() + ':00');
    }
    
    return labels;
}

// Helper function to generate random data for the bandwidth history chart
function generateRandomData(count, min, max) {
    return Array.from({ length: count }, () => Math.random() * (max - min) + min);
}

// Open the bandwidth test popup
function openBandwidthTest() {
    document.getElementById('bandwidthTestOverlay').style.display = 'block';
    document.getElementById('bandwidthTestPopup').style.display = 'block';
    
    resetTestUI();
}

// Close the bandwidth test popup
function closeBandwidthTest() {
    document.getElementById('bandwidthTestOverlay').style.display = 'none';
    document.getElementById('bandwidthTestPopup').style.display = 'none';
    
    if (testInProgress) {
        clearInterval(testInterval);
        testInProgress = false;
    }
}

// Reset the bandwidth test UI
function resetTestUI() {
    // Reset gauges
    if (popupSpeedGauge) {
        popupSpeedGauge.updateNeedle(0);
    }
    if (speedGauge) {
        speedGauge.updateNeedle(0);
    }
    
    // Reset progress bar
    const progressBar = document.getElementById('testProgressBar');
    if (progressBar) {
        progressBar.style.width = '0%';
    }
    
    // Reset value displays
    document.getElementById('popupSpeedValue').textContent = '0 Mbps';
    document.getElementById('popupDownloadValue').textContent = '-- Mbps';
    document.getElementById('popupUploadValue').textContent = '-- Mbps';
    document.getElementById('popupPingValue').textContent = '-- ms';
    
    // Enable the start button
    document.getElementById('startPopupBandwidthTest').disabled = false;
    document.getElementById('startPopupBandwidthTest').textContent = 'Start Test';
}

// Start the bandwidth test
function startBandwidthTest() {
    if (testInProgress) {
        return;
    }
    
    testInProgress = true;
    
    // Disable start button during test
    const startButton = document.getElementById('startPopupBandwidthTest');
    if (startButton) {
        startButton.disabled = true;
        startButton.textContent = 'Testing...';
    }
    
    // Reset UI
    resetTestUI();
    
    // Set initial values for ping
    setTimeout(() => {
        document.getElementById('popupPingValue').textContent = '3 ms';
    }, 500);
    
    // Simulate bandwidth test stages
    simulateDownloadTest()
        .then(() => simulateUploadTest())
        .then(() => {
            testInProgress = false;
            
            // Update statistics on the main page
            updateBandwidthStats();
            
            // Re-enable start button
            if (startButton) {
                startButton.disabled = false;
                startButton.textContent = 'Run Again';
            }
        });
}

// Simulate a download speed test
function simulateDownloadTest() {
    return new Promise((resolve) => {
        let progress = 0;
        let targetSpeed = Math.random() * 300 + 700; // Random speed between 700-1000 Mbps
        let currentSpeed = 0;
        
        updateProgressBar(progress);
        
        testInterval = setInterval(() => {
            progress += 2;
            updateProgressBar(progress);
            
            if (progress >= 50) {
                clearInterval(testInterval);
                
                // Set final download speed
                document.getElementById('popupDownloadValue').textContent = targetSpeed.toFixed(1) + ' Mbps';
                document.getElementById('popupSpeedValue').textContent = targetSpeed.toFixed(1) + ' Mbps';
                
                if (popupSpeedGauge) {
                    popupSpeedGauge.updateNeedle(targetSpeed / 10); // Scale to gauge max
                }
                
                // Update speed value on main page
                document.getElementById('downloadSpeedValue').textContent = targetSpeed.toFixed(1);
                
                if (speedGauge) {
                    speedGauge.updateNeedle(targetSpeed / 10); // Scale to gauge max
                }
                
                // Update speed value display
                document.getElementById('speedValue').textContent = targetSpeed.toFixed(1);
                
                resolve();
            } else {
                // Gradually increase to target speed
                currentSpeed = (progress / 50) * targetSpeed;
                document.getElementById('popupSpeedValue').textContent = currentSpeed.toFixed(1) + ' Mbps';
                
                if (popupSpeedGauge) {
                    popupSpeedGauge.updateNeedle(currentSpeed / 10); // Scale to gauge max
                }
            }
        }, 100);
    });
}

// Simulate an upload speed test
function simulateUploadTest() {
    return new Promise((resolve) => {
        let progress = 50;
        let targetSpeed = Math.random() * 50 + 70; // Random speed between 70-120 Mbps
        let currentSpeed = 0;
        
        testInterval = setInterval(() => {
            progress += 2;
            updateProgressBar(progress);
            
            if (progress >= 100) {
                clearInterval(testInterval);
                
                // Set final upload speed
                document.getElementById('popupUploadValue').textContent = targetSpeed.toFixed(1) + ' Mbps';
                
                // Update speed value on main page
                document.getElementById('uploadSpeedValue').textContent = targetSpeed.toFixed(1);
                
                resolve();
            } else {
                // Gradually increase to target speed
                currentSpeed = ((progress - 50) / 50) * targetSpeed;
                document.getElementById('popupSpeedValue').textContent = currentSpeed.toFixed(1) + ' Mbps';
                
                if (popupSpeedGauge) {
                    popupSpeedGauge.updateNeedle(currentSpeed / 10); // Scale to gauge max
                }
            }
        }, 100);
    });
}

// Update the progress bar
function updateProgressBar(percentage) {
    const progressBar = document.getElementById('testProgressBar');
    if (progressBar) {
        progressBar.style.width = percentage + '%';
    }
}

// Update bandwidth statistics on the main dashboard
function updateBandwidthStats() {
    // This would typically update the main dashboard with the latest test results
    // Since we're simulating, we'll just update the UI
    const downloadSpeed = document.getElementById('popupDownloadValue').textContent;
    const uploadSpeed = document.getElementById('popupUploadValue').textContent;
    
    if (historyChart) {
        // Add latest values to the chart (scaled down to fit in with historical data)
        const downloadVal = parseFloat(downloadSpeed) / 10;
        const uploadVal = parseFloat(uploadSpeed) / 10;
        
        // Add latest test result to history chart
        historyChart.data.datasets[0].data.push(downloadVal);
        historyChart.data.datasets[0].data.shift();
        
        historyChart.data.datasets[1].data.push(uploadVal);
        historyChart.data.datasets[1].data.shift();
        
        historyChart.update();
    }
}