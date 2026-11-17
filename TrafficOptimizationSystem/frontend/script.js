// ===============================
// TRAFFIC OPTIMIZATION SYSTEM
// ===============================

const map = L.map("map").setView([30.3165, 78.0322], 13);

L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
    attribution: "&copy; OpenStreetMap contributors"
}).addTo(map);

// Coordinates for the project locations.
// Leaflet uses [latitude, longitude].
const locations = {
    "Clock Tower": [30.3256, 78.0437],
    "ISBT": [30.2900, 78.0130],
    "Rajpur Road": [30.3450, 78.0600],
    "Ballupur": [30.3310, 78.0050],
    "Prem Nagar": [30.3520, 77.9500],
    "Pacific Mall": [30.3700, 78.0800]
};

let markers = [];

async function loadSensors() {
    try {
        const response = await fetch("/api/sensors");
        const data = await response.json();
        displaySensors(data);
    } catch (error) {
        console.error("Error loading sensors:", error);
    }
}

function displaySensors(data) {
    const tableBody = document.getElementById("sensorTableBody");
    if (!tableBody) return;

    tableBody.innerHTML = "";

    data.forEach((sensor, index) => {
        const row = document.createElement("tr");
        row.innerHTML = `
            <td>${index + 1}</td>
            <td>${sensor.road}</td>
            <td>${sensor.vehicles}</td>
            <td>${Number(sensor.speed).toFixed(1)} km/h</td>
            <td>${sensor.waiting} sec</td>
            <td><span class="traffic-level">${sensor.level}</span></td>
            <td>${sensor.priority}</td>
            <td>${sensor.emergency ? "🚑 Emergency" : "Normal"}</td>
        `;
        tableBody.appendChild(row);
    });

    updateStatistics(data);
}

function updateStatistics(data) {
    let totalVehicles = 0;
    let totalWaiting = 0;
    let emergencyCount = 0;

    data.forEach(sensor => {
        totalVehicles += Number(sensor.vehicles);
        totalWaiting += Number(sensor.waiting);
        if (sensor.emergency) emergencyCount++;
    });

    const totalVehiclesElement = document.getElementById("totalVehicles");
    const avgWaitingElement = document.getElementById("avgWaiting");
    const emergencyElement = document.getElementById("emergencyCount");

    if (totalVehiclesElement) {
        totalVehiclesElement.textContent = totalVehicles;
    }

    if (avgWaitingElement) {
        avgWaitingElement.textContent =
            (data.length ? totalWaiting / data.length : 0).toFixed(1) + " sec";
    }

    if (emergencyElement) {
        emergencyElement.textContent = emergencyCount;
    }
}

async function loadPriority() {
    try {
        const response = await fetch("/api/priority");
        const data = await response.json();
        displayPriority(data);
    } catch (error) {
        console.error("Error loading priority:", error);
    }
}

function displayPriority(data) {
    const container = document.getElementById("priorityList");
    if (!container) return;

    container.innerHTML = "";

    data.forEach((item, index) => {
        const div = document.createElement("div");
        div.className = "priority-item";
        div.innerHTML = `
            <div><strong>#${index + 1}</strong> ${item.road}</div>
            <div>Priority: <strong>${item.priority}</strong></div>
            <div>${item.emergency ? "🚑 EMERGENCY" : "Normal"}</div>
        `;
        container.appendChild(div);
    });
}

async function updateSensors() {
    try {
        const response = await fetch("/api/update", { method: "POST" });
        const result = await response.json();

        if (result.success) {
            await loadSensors();
            await loadPriority();
            showMessage("Traffic sensor data updated successfully.");
        }
    } catch (error) {
        console.error(error);
        showMessage("Unable to update sensor data.");
    }
}

async function findRoute() {
    const source = document.getElementById("source").value;
    const destination = document.getElementById("destination").value;

    if (source === "" || destination === "") {
        alert("Please select source and destination.");
        return;
    }

    if (source === destination) {
        alert("Source and destination cannot be the same.");
        return;
    }

    try {
        // First, C++ Dijkstra chooses the traffic-optimized sequence
        // of project nodes.
        const response = await fetch(
            `/api/route?source=${source}&destination=${destination}`
        );

        const data = await response.json();

        if (!data.success) {
            alert(data.error || "No route found.");
            return;
        }

        // Then OSRM converts those project nodes into a road-following
        // route using the OpenStreetMap road network.
        await displayRoadRoute(data);
    } catch (error) {
        console.error("Route error:", error);
        alert("Unable to find route.");
    }
}

async function displayRoadRoute(data) {
    clearRoute();

    const routeNames = data.route || [];
    if (routeNames.length < 2) {
        alert("A valid route was not found.");
        return;
    }

    // OSRM alternatives are requested between the selected endpoints.
    // OSRM expects longitude,latitude; Leaflet uses latitude,longitude.
    const sourceName = routeNames[0];
    const destinationName = routeNames[routeNames.length - 1];
    const source = locations[sourceName];
    const destination = locations[destinationName];

    if (!source || !destination) {
        alert("Source or destination coordinates are not available.");
        return;
    }

    const start = `${source[1]},${source[0]}`;
    const end = `${destination[1]},${destination[0]}`;
    const osrmUrl =
        `https://router.project-osrm.org/route/v1/driving/${start};${end}` +
        `?alternatives=3&overview=full&geometries=geojson&steps=false`;

    try {
        const response = await fetch(osrmUrl);
        if (!response.ok) {
            throw new Error(`OSRM HTTP ${response.status}`);
        }

        const roadData = await response.json();
        if (roadData.code !== "Ok" || !Array.isArray(roadData.routes) || roadData.routes.length === 0) {
            throw new Error("No road routes returned by OSRM");
        }

        const roadRoutes = roadData.routes.slice(0, 3);
        const colors = ["#1683ff", "#e58a16", "#8b5cf6"];
        const routeLabels = ["Route 1 — Primary", "Route 2 — Alternative", "Route 3 — Alternative"];
        const routeLines = [];

        roadRoutes.forEach((roadRoute, index) => {
            const roadCoordinates = roadRoute.geometry.coordinates.map(
                point => [point[1], point[0]]
            );

            const line = L.polyline(roadCoordinates, {
                color: colors[index],
                weight: index === 0 ? 7 : 5,
                opacity: index === 0 ? 1 : 0.85
            }).addTo(map);

            line.bindPopup(
                `<strong>${routeLabels[index]}</strong><br>` +
                `Distance: ${(roadRoute.distance / 1000).toFixed(2)} km<br>` +
                `Estimated time: ${(roadRoute.duration / 60).toFixed(1)} minutes`
            );
            markers.push(line);
            routeLines.push(line);
        });

        const startMarker = L.marker(source)
            .addTo(map)
            .bindPopup(`<strong>Source:</strong> ${sourceName}`);
        const endMarker = L.marker(destination)
            .addTo(map)
            .bindPopup(`<strong>Destination:</strong> ${destinationName}`);
        markers.push(startMarker, endMarker);

        const boundsGroup = L.featureGroup(routeLines);
        map.fitBounds(boundsGroup.getBounds(), { padding: [30, 30] });

        const routeInfo = document.getElementById("routeInfo");
        if (routeInfo) {
            const details = roadRoutes.map((roadRoute, index) => `
                <div style="border-left:5px solid ${colors[index]};padding:8px 12px;margin:10px 0;background:#f5f7fa;border-radius:5px;">
                    <strong>${routeLabels[index]}</strong><br>
                    Distance: ${(roadRoute.distance / 1000).toFixed(2)} km<br>
                    Estimated time: ${(roadRoute.duration / 60).toFixed(1)} minutes
                </div>
            `).join("");

            routeInfo.innerHTML = `
                <h3>Road Route Comparison</h3>
                <p><strong>Source:</strong> ${sourceName}</p>
                <p><strong>Destination:</strong> ${destinationName}</p>
                ${details}
                <p><strong>Dijkstra traffic-adjusted graph cost:</strong> ${Number(data.cost).toFixed(2)}</p>
                <small>OSRM may return fewer than three routes when fewer suitable alternatives are available. The Dijkstra graph cost is calculated separately from OSRM road distance and time.</small>
            `;
        }

        if (roadRoutes.length < 3) {
            showMessage(`OSRM returned ${roadRoutes.length} route(s); fewer than three alternatives were available.`);
        }
    } catch (error) {
        console.error("OSRM routing error:", error);
        alert("Unable to load alternative road routes. Check your internet connection and try again.");
    }
}

function clearRoute() {
    markers.forEach(marker => map.removeLayer(marker));
    markers = [];
}

function showMessage(message) {
    const element = document.getElementById("message");
    if (!element) return;

    element.textContent = message;

    setTimeout(() => {
        element.textContent = "";
    }, 3000);
}

function initializeLocations() {
    const source = document.getElementById("source");
    const destination = document.getElementById("destination");

    if (!source || !destination) return;

    Object.keys(locations).forEach((name, index) => {
        const option1 = document.createElement("option");
        option1.value = index;
        option1.textContent = name;
        source.appendChild(option1);

        const option2 = document.createElement("option");
        option2.value = index;
        option2.textContent = name;
        destination.appendChild(option2);
    });
}

document.addEventListener("DOMContentLoaded", () => {
    initializeLocations();
    loadSensors();
    loadPriority();
});

setInterval(() => {
    loadSensors();
    loadPriority();
}, 10000);
