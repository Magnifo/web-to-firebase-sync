const axios = require('axios');
const moment = require('moment-timezone');
const fs = require('fs');
const https = require('https');
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

// Configuration
const CITIES = ['Islamabad', 'Karachi', 'Lahore', 'Peshawar', 'Quetta', 'Multan', 'Faisalabad', 'Sialkot'];
const TYPES = ['Arrival', 'Departure'];
const BASE_URL = 'https://paaconnectapi.paa.gov.pk/api/flights';

const agent = new https.Agent({
    rejectUnauthorized: false
});

async function fetchFlightData() {
    const dates = [
        moment().subtract(1, 'days'),
        moment(),
        moment().add(1, 'days')
    ];

    const allFlights = [];

    for (const dateObj of dates) {
        const dateStr = dateObj.tz('Asia/Karachi').format('YYYY-MM-DD');
        console.log(`Fetching flight data for: ${dateStr}`);

        for (const city of CITIES) {
            for (const type of TYPES) {
                try {
                    const url = `${BASE_URL}/${dateStr}/${type}/${city}`;
                    // console.log(`Fetching ${type} for ${city} on ${dateStr}...`); // Reduce log spam
                    const response = await axios.get(url, { httpsAgent: agent });

                    if (response.data && Array.isArray(response.data)) {
                        const flights = response.data.map(flight => ({
                            ...flight,
                            _sourceCity: city,
                            _fetchedType: type,
                            _fetchedDate: dateStr
                        }));
                        allFlights.push(...flights);
                    }
                } catch (error) {
                    // console.error(`Error fetching ${type} for ${city}:`, error.message);
                }
            }
        }
    }

    return allFlights;
}

async function main() {
    try {
        const data = await fetchFlightData();
        console.log(`Total flights fetched: ${data.length}`);

        // Generate and log report BEFORE modifying data
        generateReport(data);

        // Restructure data: City -> Arrival/Departure -> { Key: FlightObject }
        const structuredData = {};

        data.forEach(flight => {
            const city = flight._sourceCity;
            const type = flight._fetchedType;

            // Generate Key: YYMMDDHHMM_FlightNumber
            // Ensure we capture this BEFORE deleting _fetchedDate
            const dateKey = flight._fetchedDate ? moment(flight._fetchedDate, 'YYYY-MM-DD').format('YYMMDD') : '000000';
            const timeKey = flight.ST ? flight.ST.replace(/:/g, '') : '0000';
            const flightNumOriginal = flight.FlightNumber || 'UNKNOWN';
            const flightNumKey = flightNumOriginal.replace(/[^a-zA-Z0-9]/g, '');
            const uniqueKey = `${dateKey}${timeKey}_${flightNumKey}`;

            if (!structuredData[city]) {
                structuredData[city] = {
                    Arrival: {},
                    Departure: {}
                };
            }

            // Cleanup & Renaming
            // 1. Rename FlightNumber -> flnr and extract airline prefix
            flight.flnr = flight.FlightNumber.replace(/-/g, ' ');

            // Extract airline code:
            // 1. If has space, get part before space (e.g., "PK 342" → "PK")
            // 2. If has hyphen, get part before hyphen (e.g., "PK-302" → "PK")
            // 3. Otherwise, get first 2 characters
            if (flight.flnr && flight.flnr.includes(' ')) {
                flight.airline = flight.flnr.split(' ')[0];
            } else if (flight.flnr && flight.flnr.includes('-')) {
                flight.airline = flight.flnr.split('-')[0];
            } else if (flight.flnr && flight.flnr.length >= 2) {
                flight.airline = flight.flnr.substring(0, 2);
            } else {
                flight.airline = flight.flnr || '';
            }
            delete flight.FlightNumber;

            // 2. Transform Nature -> SubCat (INT / DOM)
            if (flight.Nature) {
                const upperNature = flight.Nature.toUpperCase();
                if (upperNature.includes('INTERNATIONAL')) flight.SubCat = 'INT';
                else if (upperNature.includes('DOMESTIC')) flight.SubCat = 'DOM';
                else flight.SubCat = upperNature; // Fallback
            }
            //delete flight.Nature;

            // 3. Clean up metadata fields
            delete flight._sourceCity;
            delete flight._fetchedType;
            delete flight._fetchedDate;

            // 4. Remove unwanted fields per user request
            delete flight.UrduFromCity;
            delete flight.UrduToCity;
            delete flight.Logo;
            delete flight.UrduRemarks;

            delete flight.IsNonStop;
            delete flight.IsDelta;
            // delete flight.IsHidden; // User commented out? No, previous blocks had deletes.
            // Let's stick to the requested change area.

            delete flight.IsHidden;
            delete flight.Type;
            delete flight.Day;
            //delete flight.EnglishRemarks;

            // 4b. Replace EnglishFrom/To with city_lu (Remote City)
            if (type === 'Arrival') {
                flight.city_lu = flight.EnglishFromCity;
            } else {
                flight.city_lu = flight.EnglishToCity;
            }
            delete flight.EnglishFromCity;
            delete flight.EnglishToCity;
            // 4c. Replace EnglishRemarks with prem_lu
            let remarksVal = flight.EnglishRemarks || '';
            if (remarksVal.trim().toUpperCase() === 'ARRIVED') {
                remarksVal = 'Landed';
            }
            flight.prem_lu = remarksVal;
            delete flight.EnglishRemarks;

            delete flight.DateUpdated;

            // 5. Add stm field (YYYYMMDDHHMM)
            const stmString = `${flight.Date} ${flight.ST}`;
            const stmMoment = moment.tz(stmString, 'M/D/YYYY h:mm:ss A HH:mm', 'Asia/Karachi'); // Adjust format to match input "1/7/2026 12:00:00 AM 00:05" logic
            // Actually, based on previous JSON, flight.Date is "1/7/2026 12:00:00 AM" and flight.ST is "00:05"
            // Let's parse strictly.
            // Simplified: We have dateKey "260107" and timeKey "0005".
            const stmIso = moment.tz(`${dateKey} ${timeKey}`, 'YYMMDD HHmm', 'Asia/Karachi');
            flight.stm = stmIso.format('YYMMDDHHmm');

            // 6. Add att field if ET exists (YYYYMMDDHHMM)
            // Handle date rollover (Next Day / Previous Day)
            if (flight.ET && flight.ET.trim() !== '') {
                const etTimeKey = flight.ET.replace(/:/g, '');

                // Create candidates
                // UPDATED to match new dateKey format (YYMMDD)
                const attSameDay = moment.tz(`${dateKey} ${etTimeKey}`, 'YYMMDD HHmm', 'Asia/Karachi');
                const attPrevDay = attSameDay.clone().subtract(1, 'days');
                const attNextDay = attSameDay.clone().add(1, 'days');

                // Calculate differences in minutes
                const diffSame = Math.abs(attSameDay.diff(stmIso, 'minutes'));
                const diffPrev = Math.abs(attPrevDay.diff(stmIso, 'minutes'));
                const diffNext = Math.abs(attNextDay.diff(stmIso, 'minutes'));

                // Find minimum difference
                let correctAtt = attSameDay;
                if (diffPrev < diffSame && diffPrev < diffNext) {
                    correctAtt = attPrevDay;
                } else if (diffNext < diffSame && diffNext < diffPrev) {
                    correctAtt = attNextDay;
                }

                // Determine if this is Actual (att) or Estimated (est)
                const remarks = (flight.prem_lu || '').toUpperCase();
                // Check if status indicates actual time (Departed or Landed)
                if (remarks.includes('DEPARTED') || remarks.includes('LANDED') || remarks.includes('ARRIVED')) {
                    flight.att = correctAtt.format('YYMMDDHHmm');
                } else {
                    flight.est = correctAtt.format('YYMMDDHHmm');
                }
            }

            // Final Cleanup: Remove Date and ST/ET after using them
            delete flight.Date;
            delete flight.ST;
            delete flight.ET;
            //delete flight.DateUpdated;
            delete flight.Nature; // Should already be handled but ensuring cleanup.

            if (structuredData[city][type]) {
                structuredData[city][type][uniqueKey] = flight;
            }
        });

        // Load previous data for comparison
        let previousData = {};
        if (fs.existsSync('flights_data.json')) {
            try {
                previousData = JSON.parse(fs.readFileSync('flights_data.json', 'utf8'));
                console.log('Loaded previous local data for Delta Sync comparison.');
            } catch (e) {
                console.log('Previous data corrupted or mismatch, starting fresh diff.');
            }
        }

        // Save new data to JSON file (overwriting old one for next run)
        fs.writeFileSync('flights_data.json', JSON.stringify(structuredData, null, 2));
        console.log('Data saved to flights_data.json (Restructured)');

        if (db) {
            console.log('Calculating Delta updates (Zero Firebase reads)...');
            const updates = {};
            let changedCount = 0;
            let newCount = 0;

            // Define the fields that this script manages
            const managedFields = ['flnr', 'airline', 'SubCat', 'city_lu', 'prem_lu', 'stm', 'att', 'est'];

            // Process all flights using LOCAL comparison only
            for (const city in structuredData) {
                for (const type in structuredData[city]) {
                    for (const key in structuredData[city][type]) {
                        const newFlight = structuredData[city][type][key];
                        const oldFlight = previousData[city] && previousData[city][type]
                            ? previousData[city][type][key]
                            : null;

                        if (!oldFlight) {
                            // Brand new flight - use field-level updates to preserve any existing custom fields
                            for (const field of managedFields) {
                                if (newFlight[field] !== undefined) {
                                    updates[`${city}/${type}/${key}/${field}`] = newFlight[field];
                                }
                            }
                            newCount++;
                        } else if (JSON.stringify(newFlight) !== JSON.stringify(oldFlight)) {
                            // Flight changed - update only the managed fields that changed
                            for (const field of managedFields) {
                                const newValue = newFlight[field];
                                const oldValue = oldFlight[field];

                                if (newValue !== oldValue) {
                                    // Field changed or added
                                    if (newValue !== undefined) {
                                        updates[`${city}/${type}/${key}/${field}`] = newValue;
                                    } else {
                                        // Field removed (was in old, not in new)
                                        updates[`${city}/${type}/${key}/${field}`] = null;
                                    }
                                }
                            }
                            changedCount++;
                        }
                        // else: unchanged, skip entirely
                    }
                }
            }

            if (Object.keys(updates).length > 0) {
                console.log(`Delta Sync: ${newCount} new flights, ${changedCount} changed flights (0 Firebase reads).`);
                console.log(`Uploading ${Object.keys(updates).length} field-level updates to Firebase...`);
                await db.ref('/flights').update(updates);
                console.log('Successfully updated flight data in Firebase.');
            } else {
                console.log('Delta Sync: No changes detected. Skipping Firebase update.');
            }

            // Always update the 'lastUpdate' timestamp so the client knows when we last checked
            const lastUpdateStr = moment().tz('Asia/Karachi').format('D MMM YYYY hh:mm A');
            await db.ref('/lastUpdate').set(lastUpdateStr);
            console.log(`Global Last Update time set to: ${lastUpdateStr}`);

            process.exit(0);
        }

    } catch (error) {
        console.error('Fatal Error:', error);
    }
}

function generateReport(flights) {
    console.log('\n--- Flight Data Report ---');
    const report = {};

    flights.forEach(flight => {
        const date = flight._fetchedDate;
        const city = flight._sourceCity;
        const type = flight._fetchedType;
        const key = `${date} - ${city}`;

        if (!report[key]) {
            report[key] = {
                Date: date,
                City: city,
                Total: 0,
                Arrivals: 0,
                Departures: 0
            };
        }

        report[key].Total++;
        if (type === 'Arrival') report[key].Arrivals++;
        if (type === 'Departure') report[key].Departures++;
    });

    console.table(Object.values(report).sort((a, b) => a.Date.localeCompare(b.Date) || a.City.localeCompare(b.City)));
}

main();
