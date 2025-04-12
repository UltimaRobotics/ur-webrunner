// MQTT Integration Module
let mqttConnected = false;
let publishedCount = 0;
let receivedCount = 0;
let clientCount = 0;

document.addEventListener('DOMContentLoaded', function() {
    // Set up MQTT UI elements
    setupMqttUI();
    
    // Check if MQTT broker is running
    checkMqttStatus();
    
    // Set up MQTT API docs popup event handlers
    document.getElementById('api-docs-btn')?.addEventListener('click', showApiDocs);
    document.getElementById('apiDocsClose')?.addEventListener('click', hideApiDocs);
    document.getElementById('apiDocsOverlay')?.addEventListener('click', hideApiDocs);
    
    // Set up MQTT status popup event handlers
    document.getElementById('mqtt-toggle-btn')?.addEventListener('click', showMqttStatus);
    document.getElementById('mqttStatusClose')?.addEventListener('click', hideMqttStatus);
    document.getElementById('mqttStatusOverlay')?.addEventListener('click', hideMqttStatus);
    
    // Set up MQTT control buttons
    document.getElementById('startMqttButton')?.addEventListener('click', startMqttBroker);
    document.getElementById('stopMqttButton')?.addEventListener('click', stopMqttBroker);
});

// Initialize MQTT UI elements
function setupMqttUI() {
    // This function would typically set up MQTT-related UI elements
    console.log('Setting up MQTT UI elements');
}

// Check MQTT broker status
function checkMqttStatus() {
    fetch('/api/mqtt/status')
        .then(response => response.json())
        .catch(error => {
            console.error('Error fetching MQTT status:', error);
            updateMqttStatusUI(false);
        })
        .then(data => {
            if (data && data.running) {
                updateMqttStatusUI(true);
                clientCount = data.clients || 0;
                publishedCount = data.published || 0;
                receivedCount = data.received || 0;
                updateMqttStats();
            } else {
                updateMqttStatusUI(false);
            }
        });
}

// Update MQTT status in the UI
function updateMqttStatusUI(isConnected) {
    mqttConnected = isConnected;
    
    const statusIndicator = document.getElementById('mqttConnectionStatus');
    if (statusIndicator) {
        if (isConnected) {
            statusIndicator.innerHTML = '<i class="fas fa-check-circle"></i><span>Connected</span>';
            statusIndicator.classList.add('connected');
        } else {
            statusIndicator.innerHTML = '<i class="fas fa-times-circle"></i><span>Disconnected</span>';
            statusIndicator.classList.remove('connected');
        }
    }
    
    // Update buttons
    const startButton = document.getElementById('startMqttButton');
    const stopButton = document.getElementById('stopMqttButton');
    
    if (startButton && stopButton) {
        if (isConnected) {
            startButton.disabled = true;
            stopButton.disabled = false;
        } else {
            startButton.disabled = false;
            stopButton.disabled = true;
        }
    }
}

// Update MQTT statistics
function updateMqttStats() {
    document.getElementById('mqttClientCount').textContent = clientCount;
    document.getElementById('mqttPublishedCount').textContent = publishedCount;
    document.getElementById('mqttReceivedCount').textContent = receivedCount;
    document.getElementById('mqttBrokerAddress').textContent = document.getElementById('clientIP').textContent + ':1883';
}

// Show API documentation popup
function showApiDocs() {
    document.getElementById('apiDocsOverlay').style.display = 'block';
    document.getElementById('apiDocsPopup').style.display = 'block';
}

// Hide API documentation popup
function hideApiDocs() {
    document.getElementById('apiDocsOverlay').style.display = 'none';
    document.getElementById('apiDocsPopup').style.display = 'none';
}

// Show MQTT status popup
function showMqttStatus() {
    document.getElementById('mqttStatusOverlay').style.display = 'block';
    document.getElementById('mqttStatusPopup').style.display = 'block';
    
    // Refresh MQTT status
    checkMqttStatus();
}

// Hide MQTT status popup
function hideMqttStatus() {
    document.getElementById('mqttStatusOverlay').style.display = 'none';
    document.getElementById('mqttStatusPopup').style.display = 'none';
}

// Start MQTT broker
function startMqttBroker() {
    fetch('/api/mqtt/start', { 
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        }
    })
    .then(response => response.json())
    .catch(error => {
        console.error('Error starting MQTT broker:', error);
    })
    .then(data => {
        if (data && data.success) {
            updateMqttStatusUI(true);
            clientCount = 0;
            publishedCount = 0;
            receivedCount = 0;
            updateMqttStats();
            
            // Update service status in services list
            const mqttServiceStatus = document.querySelector('.service-item:nth-child(5) .service-status');
            if (mqttServiceStatus) {
                mqttServiceStatus.textContent = 'Running';
                mqttServiceStatus.classList.remove('stopped');
                mqttServiceStatus.classList.add('running');
            }
            
            // Update service action buttons
            const startButton = document.querySelector('.service-item:nth-child(5) .start-btn');
            const restartButton = document.querySelector('.service-item:nth-child(5) .restart-btn');
            if (startButton && restartButton) {
                startButton.textContent = 'Stop';
                startButton.classList.remove('start-btn');
                startButton.classList.add('stop-btn');
                restartButton.classList.remove('disabled');
            }
        } else {
            console.error('Failed to start MQTT broker');
        }
    });
}

// Stop MQTT broker
function stopMqttBroker() {
    fetch('/api/mqtt/stop', { 
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        }
    })
    .then(response => response.json())
    .catch(error => {
        console.error('Error stopping MQTT broker:', error);
    })
    .then(data => {
        if (data && data.success) {
            updateMqttStatusUI(false);
            
            // Update service status in services list
            const mqttServiceStatus = document.querySelector('.service-item:nth-child(5) .service-status');
            if (mqttServiceStatus) {
                mqttServiceStatus.textContent = 'Stopped';
                mqttServiceStatus.classList.remove('running');
                mqttServiceStatus.classList.add('stopped');
            }
            
            // Update service action buttons
            const stopButton = document.querySelector('.service-item:nth-child(5) .stop-btn');
            const restartButton = document.querySelector('.service-item:nth-child(5) .restart-btn');
            if (stopButton && restartButton) {
                stopButton.textContent = 'Start';
                stopButton.classList.remove('stop-btn');
                stopButton.classList.add('start-btn');
                restartButton.classList.add('disabled');
            }
        } else {
            console.error('Failed to stop MQTT broker');
        }
    });
}