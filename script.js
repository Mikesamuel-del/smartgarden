document.addEventListener('DOMContentLoaded', function() {
    // Initialize health gauge
    updateHealthGauge(75);

    // Generate trend chart
    generateTrendChart();

    // Set last updated time
    document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();

    // Navigation buttons
    const navButtons = document.querySelectorAll('.nav-btn');
    const views = document.querySelectorAll('main > div');

    function showView(viewId) {
        // Hide all views
        views.forEach(view => view.classList.add('hidden'));

        // Show target view
        document.getElementById(viewId).classList.remove('hidden');

        // Update navigation button states
        navButtons.forEach(btn => {
            btn.classList.remove('text-primary');
            btn.classList.add('text-gray-500');
        });

        const activeBtn = [...navButtons].find(btn => btn.dataset.view === viewId);
        if (activeBtn) {
            activeBtn.classList.add('text-primary');
            activeBtn.classList.remove('text-gray-500');
        }
    }

    // Default to dashboard view
    showView('dashboardView');

    navButtons.forEach(button => {
        button.addEventListener('click', function() {
            const targetView = this.getAttribute('data-view');
            showView(targetView);
        });
    });

    // Header buttons → Alerts and User
    document.getElementById('userBtn').addEventListener('click', function() {
        showView('userView');
    });

    const alertsBtn = document.getElementById('alertsBtn');
    if (alertsBtn) {
        alertsBtn.addEventListener('click', function() {
            showView('alertsView');
        });
    }

    // 🔄 Fetch sensor data every 5 seconds
    setInterval(fetchSensorData, 5000);
    fetchSensorData(); // Initial call

    // 💾 Settings Save button
    const saveButton = document.querySelector('#settingsView button');
    if (saveButton) {
        saveButton.addEventListener('click', () => {
            alert('✅ Settings saved successfully!');
        });
    }
});


// ----------------------------------------
// ✅ Function: Fetch real sensor data
// ----------------------------------------
//SENSORS TO BE ADDED HERE
async function fetchSensorData() {
    const API_URL = "http://192.168.1.100:5000/sensor"; 
    // Replace with your actual backend endpoint

    try {
        const response = await fetch(API_URL);
        const data = await response.json();

        // ✅ Update sensor cards
        document.getElementById('moistureValue').textContent = data.moisture + '%';
        document.querySelector('[id="moistureValue"] ~ div > div').style.width = data.moisture + '%';

        document.getElementById('humidityValue').textContent = data.humidity + '%';
        document.querySelector('[id="humidityValue"] ~ div > div').style.width = data.humidity + '%';

        document.getElementById('temperatureValue').textContent = data.temperature + '°C';
        document.querySelector('[id="temperatureValue"] ~ div > div').style.width =
            ((data.temperature - 15) / 15 * 100) + '%';

        // ✅ Compute simple health score (customize for your plant type)
        const healthScore =
            (data.moisture / 100) * 25 +
            (data.humidity / 100) * 25 +
            ((data.temperature - 18) / 8) * 25;

        updateHealthGauge(healthScore);

        // ✅ Update last updated time
        document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();

    } catch (error) {
        console.error("⚠️ Error fetching sensor data:", error);
        document.getElementById('healthStatus').textContent = "Sensor Offline";
        document.getElementById('healthStatus').className = 'text-lg font-medium text-danger';
    }
}


// ----------------------------------------
// ✅ Function: Update health gauge needle & label
// ----------------------------------------
function updateHealthGauge(value) {
    const needle = document.getElementById('healthNeedle');
    const healthStatus = document.getElementById('healthStatus');

    const rotation = (value / 100) * 180 - 90;
    needle.style.transform = `translateX(-50%) rotate(${rotation}deg)`;

    if (value < 25) {
        healthStatus.textContent = 'Critical Health';
        healthStatus.className = 'text-lg font-medium text-danger';
    } else if (value < 50) {
        healthStatus.textContent = 'Poor Health';
        healthStatus.className = 'text-lg font-medium text-warning';
    } else if (value < 75) {
        healthStatus.textContent = 'Fair Health';
        healthStatus.className = 'text-lg font-medium text-accent';
    } else {
        healthStatus.textContent = 'Good Health';
        healthStatus.className = 'text-lg font-medium text-success';
    }
}


// ----------------------------------------
// ✅ Function: Generate static trend chart
// ----------------------------------------
function generateTrendChart() {
    const chartContainer = document.getElementById('trendChart');
    if (!chartContainer) return;
    chartContainer.innerHTML = '';

    const days = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
    const values = [65, 59, 80, 72, 56, 55, 75];

    days.forEach((day, index) => {
        const barContainer = document.createElement('div');
        barContainer.className = 'flex flex-col items-center';

        const valueLabel = document.createElement('span');
        valueLabel.className = 'text-xs mb-1';
        valueLabel.textContent = values[index] + '%';

        const bar = document.createElement('div');
        bar.className = 'chart-bar';
        bar.style.height = (values[index] / 100 * 200) + 'px';

        const dayLabel = document.createElement('span');
        dayLabel.className = 'text-xs mt-1';
        dayLabel.textContent = day;

        barContainer.appendChild(valueLabel);
        barContainer.appendChild(bar);
        barContainer.appendChild(dayLabel);

        chartContainer.appendChild(barContainer);
    });
}
//MODIFICATIONS
document.getElementById("chatForumBtn").addEventListener("click", () => {
    showView("chatForumView");
});





