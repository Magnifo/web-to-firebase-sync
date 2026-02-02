const axios = require('axios');
const moment = require('moment-timezone');
const admin = require('firebase-admin');
const serviceAccount = require('./serviceAccountKey.json');

// Initialize Firebase Admin SDK
try {
    admin.initializeApp({
        credential: admin.credential.cert(serviceAccount),
        databaseURL: "https://aseengrs-7f20c-default-rtdb.asia-southeast1.firebasedatabase.app"
    });
    console.log('Firebase Admin initialized.');
} catch (error) {
    console.warn('Firebase initialization warning:', error.message);
}

const db = admin.database();

// Configuration: City Names with Coordinates (Lat, Lng)
const CITIES = [
    { name: 'Islamabad', lat: 33.6844, lng: 73.0479 },
    { name: 'Karachi', lat: 24.8607, lng: 67.0011 },
    { name: 'Lahore', lat: 31.5204, lng: 74.3587 },
    { name: 'Peshawar', lat: 34.0151, lng: 71.5249 },
    { name: 'Quetta', lat: 30.1798, lng: 66.9750 },
    { name: 'Multan', lat: 30.1575, lng: 71.5249 },
    { name: 'Faisalabad', lat: 31.4504, lng: 73.1350 },
    { name: 'Sialkot', lat: 32.4945, lng: 74.5229 }
];

const BASE_URL = 'https://api.open-meteo.com/v1/forecast';

// Helper: Convert WMO Weather Code to Description
function getWeatherDescription(code) {
    const codes = {
        0: 'Clear sky',
        1: 'Mainly clear', 2: 'Partly cloudy', 3: 'Overcast',
        45: 'Fog', 48: 'Depositing rime fog',
        51: 'Light drizzle', 53: 'Moderate drizzle', 55: 'Dense drizzle',
        56: 'Light freezing drizzle', 57: 'Dense freezing drizzle',
        61: 'Slight rain', 63: 'Moderate rain', 65: 'Heavy rain',
        66: 'Light freezing rain', 67: 'Heavy freezing rain',
        71: 'Slight snow', 73: 'Moderate snow', 75: 'Heavy snow',
        77: 'Snow grains',
        80: 'Slight rain showers', 81: 'Moderate rain showers', 82: 'Violent rain showers',
        85: 'Slight snow showers', 86: 'Heavy snow showers',
        95: 'Thunderstorm',
        96: 'Thunderstorm with light hail', 99: 'Thunderstorm with heavy hail'
    };
    return codes[code] || 'Unknown';
}

// Helper: Map WMO Code to Icon Name (Simplified for generic use)
function getWeatherIcon(code) {
    // 0: Clear, 1-3: Clouds, 45-48: Fog, 51+ Rain/Snow/Thunder
    if (code === 0) return '01d'; // clear
    if (code >= 1 && code <= 3) return '03d'; // cloudy
    if (code >= 45 && code <= 48) return '50d'; // mist/fog
    if (code >= 95) return '11d'; // thunderstorm
    if (code >= 71 && code <= 86) return '13d'; // snow
    return '09d'; // rain (default for others)
}

async function fetchWeatherData(cityObj) {
    try {
        // Request specific current weather variables:
        // temperature_2m, relative_humidity_2m, apparent_temperature, weather_code, wind_speed_10m, visibility
        const params = 'current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m,visibility';
        const url = `${BASE_URL}?latitude=${cityObj.lat}&longitude=${cityObj.lng}&${params}&wind_speed_unit=ms`;

        const response = await axios.get(url);
        return response.data;
    } catch (error) {
        console.error(`Error fetching weather for ${cityObj.name}:`, error.message);
        return null;
    }
}

async function main() {
    console.log('Starting Weather Sync (Open-Meteo)...');
    const updates = {};

    for (const city of CITIES) {
        const data = await fetchWeatherData(city);

        if (data && data.current) {
            const cw = data.current;
            const weatherObj = {
                temp: cw.temperature_2m,
                feels_like: cw.apparent_temperature,
                condition: getWeatherDescription(cw.weather_code),
                icon: getWeatherIcon(cw.weather_code),

                humidity: cw.relative_humidity_2m,
                wind_speed: cw.wind_speed_10m,
                visibility: cw.visibility, // in meters

                lastUpdated: moment().tz('Asia/Karachi').format('YYYY-MM-DD HH:mm:ss')
            };

            // Prepare update path: /weather/Islamabad
            updates[`/weather/${city.name}`] = weatherObj;
            console.log(`Fetched ${city.name}: ${weatherObj.temp}°C, ${weatherObj.condition}, Vis: ${weatherObj.visibility}m`);
        }
    }

    if (Object.keys(updates).length > 0) {
        try {
            // Atomic update to Firebase
            await db.ref().update(updates);
            console.log(`Successfully synced weather for ${Object.keys(updates).length} cities.`);

            // Update global timestamp
            const lastUpdateStr = moment().tz('Asia/Karachi').format('D MMM YYYY hh:mm A');
            await db.ref('/lastWeatherUpdate').set(lastUpdateStr);
            console.log(`Global Last Update time set to: ${lastUpdateStr}`);

        } catch (error) {
            console.error('Firebase update failed:', error);
        }
    } else {
        console.log('No weather data collected to update.');
    }

    process.exit(0);
}

main();
