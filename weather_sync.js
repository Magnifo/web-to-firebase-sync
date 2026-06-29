const axios = require('axios');
const moment = require('moment-timezone');

// Same five cities hardcoded in the Flutter app (home_screen.dart).
const APP_WEATHER_CITIES = [
    { name: 'Islamabad', lat: 33.6844, lng: 73.0479 },
    { name: 'Karachi', lat: 24.8607, lng: 67.0011 },
    { name: 'Lahore', lat: 31.5204, lng: 74.3587 },
    { name: 'Faisalabad', lat: 31.4504, lng: 73.1350 },
    { name: 'Peshawar', lat: 34.0151, lng: 71.5249 },
];

const BASE_URL = 'https://api.open-meteo.com/v1/forecast';

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
        96: 'Thunderstorm with light hail', 99: 'Thunderstorm with heavy hail',
    };
    return codes[code] || 'Unknown';
}

function getWeatherIcon(code) {
    if (code === 0) return '01d';
    if (code >= 1 && code <= 3) return '03d';
    if (code >= 45 && code <= 48) return '50d';
    if (code >= 95) return '11d';
    if (code >= 71 && code <= 86) return '13d';
    return '09d';
}

async function fetchWeatherData(cityObj) {
    try {
        const params = 'current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m,visibility';
        const url = `${BASE_URL}?latitude=${cityObj.lat}&longitude=${cityObj.lng}&${params}&wind_speed_unit=ms`;
        const response = await axios.get(url);
        return response.data;
    } catch (error) {
        console.error(`Error fetching weather for ${cityObj.name}:`, error.message);
        return null;
    }
}

/**
 * Push current weather for each city to /weather/{cityName} in Firebase RTDB.
 * @param {admin.database.Database} db
 * @param {Array<{name:string,lat:number,lng:number}>} cities
 */
async function syncWeatherToFirebase(db, cities = APP_WEATHER_CITIES) {
    console.log('Starting Weather Sync (Open-Meteo)...');
    const updates = {};

    for (const city of cities) {
        const data = await fetchWeatherData(city);

        if (data && data.current) {
            const cw = data.current;
            const weatherObj = {
                temp: cw.temperature_2m,
                feels_like: cw.apparent_temperature,
                condition: getWeatherDescription(cw.weather_code),
                weathercode: cw.weather_code,
                icon: getWeatherIcon(cw.weather_code),
                humidity: cw.relative_humidity_2m,
                wind_speed: cw.wind_speed_10m,
                visibility: cw.visibility,
                lastUpdated: moment().tz('Asia/Karachi').format('YYYY-MM-DD HH:mm:ss'),
            };

            updates[`/weather/${city.name}`] = weatherObj;
            console.log(`Fetched ${city.name}: ${weatherObj.temp}°C, ${weatherObj.condition}`);
        }
    }

    if (Object.keys(updates).length === 0) {
        console.log('No weather data collected to update.');
        return { synced: 0 };
    }

    await db.ref().update(updates);
    console.log(`Successfully synced weather for ${Object.keys(updates).length} cities.`);

    const lastUpdateStr = moment().tz('Asia/Karachi').format('D MMM YYYY hh:mm A');
    await db.ref('/lastWeatherUpdate').set(lastUpdateStr);
    console.log(`Weather last update set to: ${lastUpdateStr}`);

    return { synced: Object.keys(updates).length };
}

async function main() {
    const admin = require('firebase-admin');
    const serviceAccount = require('./serviceAccountKey.json');

    try {
        if (!admin.apps.length) {
            admin.initializeApp({
                credential: admin.credential.cert(serviceAccount),
                databaseURL: 'https://aseengrs-7f20c-default-rtdb.asia-southeast1.firebasedatabase.app',
            });
            console.log('Firebase Admin initialized.');
        }
    } catch (error) {
        console.warn('Firebase initialization warning:', error.message);
    }

    const db = admin.database();
    await syncWeatherToFirebase(db);
    process.exit(0);
}

if (require.main === module) {
    main().catch((error) => {
        console.error('Weather sync failed:', error);
        process.exit(1);
    });
}

module.exports = {
    APP_WEATHER_CITIES,
    syncWeatherToFirebase,
};
