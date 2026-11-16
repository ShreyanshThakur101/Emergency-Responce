# Pulse Response — C++ Emergency Dispatch Web App

This adds a web interface and HTTP API around the existing C++ emergency-dispatch project. The original command-line program and its source files remain available.

## What is connected to the existing C++ implementation?

- `emergency_core.cpp` remains the source of truth for the priority queue, responder compatibility checks, greedy responder selection, Dijkstra-based simulated ETA, active emergency records, and fleet availability.
- `graph_algorithms.cpp` provides the existing 16-zone Pune simulation graph and Dijkstra route calculations.
- `emergency_system.cpp` contains the demand-weighted standby optimization. The web workflow calls the same reusable `getOptimizedStandbyLocations(3)` function used by the existing standby demonstration.
- `web_api.cpp` is a small C++ HTTP server and adapter. It validates web requests, preserves map/address metadata, exposes API endpoints, and enforces responder status transitions. It is compiled with the existing core files, not as a disconnected mock backend.

The original `main.cpp` CLI is not replaced. `run_web.sh` builds a separate web-server entry point without the CLI's `main()`.

## Requirements

- Linux (tested in the current Linux environment; Bazzite/Fedora should work with a C++ toolchain)
- `g++` with C++17 support
- A modern browser
- Optional: Google Maps JavaScript API key, with the Maps JavaScript API enabled and billing configured in Google Cloud

No Node.js, npm, third-party C++ library, or Google Maps key is required to start the app.

## Run it

From the extracted project root (the folder containing `run_web.sh`, `web_api.cpp`, and `ds cp/`):

```bash
chmod +x run_web.sh
./run_web.sh
```

The script compiles `emergency_web` and starts the HTTP server at:

**http://127.0.0.1:8765**

Keep that terminal open while using the application. Press `Ctrl+C` to stop it. The default bind address is localhost, so the server is not exposed to your LAN by default.

To choose another port:

```bash
WEB_PORT=8766 ./run_web.sh
```

To use the app from another device on a trusted private LAN, set `WEB_HOST=0.0.0.0`, then open `http://<your-computer-LAN-IP>:8765` on that device. Only do this on a trusted network: the simulation has no user authentication, and the API can change simulated emergency records. Also note that mobile browsers generally require HTTPS for Geolocation outside `localhost`; over plain HTTP on a LAN, use the manual location fallback or put the app behind a trusted HTTPS development setup.

## Google Maps configuration

1. Copy the template:
   ```bash
   cp .env.example .env
   ```
2. In Google Cloud Console, create a browser key and enable **Maps JavaScript API** and **Geocoding API**. The browser uses the Maps JavaScript Geocoder; no server-side Google Maps API call is made.
3. Set the exact designated identifier in `.env`:
   ```dotenv
   googleMapApi=YOUR_RESTRICTED_BROWSER_KEY
   ```
4. Restrict the key by **HTTP referrer** and by the **Maps JavaScript API**. For local testing, add the relevant local origin/referrer patterns allowed by Google Cloud. Never commit the real `.env` file.
5. Restart `./run_web.sh` and reload the page.

The browser key is necessarily visible to the browser. It is not a server secret. Restrict it in Google Cloud Console. The app also reads `googleMapApi` from the process environment; `GOOGLE_MAPS_API_KEY` is accepted as a compatibility fallback, while `googleMapApi` remains the documented configuration name.

If the key is missing or invalid, the app displays a configuration message and leaves emergency reporting and simulated dispatch usable through GPS coordinates or the manual Pune-zone selector. Standard Google Maps directions links work without an API key.

## End-to-end test

With `g++` available, build the web server first, then run:

```bash
python3 test_web_api.py
```

The test starts a fresh isolated server on port 8766 and exercises request creation, automatic assignment, responder status transitions, standby assignment/arrival, duplicate-submit protection, and the no-available-ambulance case. It does not call real emergency services.

## Web API summary

| Method | Endpoint | Purpose |
|---|---|---|
| `GET` | `/api/health` | Backend health and simulation flag |
| `GET` | `/api/config` | Browser map configuration state |
| `GET` | `/api/locations` | 16 simulated dispatch zones with approximate real-world coordinates |
| `GET` | `/api/responders` | Fleet status from the C++ fleet |
| `GET` | `/api/emergencies` | Requests submitted through the web app in this session |
| `GET` | `/api/emergencies/:id` | One web-submitted emergency |
| `POST` | `/api/emergencies` | Validate and record a request, then trigger C++ automatic dispatch |
| `POST` | `/api/responders/:id/actions` | Validate and update the assigned responder's operational state |

## Workflow details and honest limitations

- The city graph and its travel-time estimates are **simulated**. Real-world Google Maps directions use the reported coordinates, but Google road routes do not replace or silently rewrite the existing Dijkstra graph.
- The 16 zone coordinates are approximate reference points for the existing simulated Pune zones. A citizen can report a location elsewhere; the backend maps its coordinates to the nearest graph zone for the existing C++ dispatch algorithm while preserving the exact coordinates/address for Google Maps navigation.
- Automatic priority is a simple project-level heuristic based on the emergency category, affected-person count, and a few high-risk phrases. It is not clinical triage and should not be used for real dispatch.
- Responder location is **not live-tracked**. Location on the fleet panel changes only when backend operations record a move or standby arrival. Responders confirm arrival manually in this simulation.
- When an emergency is resolved, the C++ core completes it. The web adapter chooses a hub from the existing demand-weighted standby optimizer's recommended three locations, then scores those hubs by Dijkstra travel time and current hub occupancy. The unit remains unavailable while returning and becomes available only after `Confirm arrival at standby`.
- Request metadata in this web adapter is held in memory and resets when the server restarts, as does the existing C++ core's state. Persistent storage is not part of the current C++ project. The app clearly reports failed requests and does not show success when the backend rejects an operation.
- This is a **simulation only**. It does not contact 112, police, ambulances, fire services, hospitals, or any real responder.

## Original CLI build remains available

The original command-line application can still be built independently:

```bash
g++ -std=c++17 -O2 "ds cp/main.cpp" "ds cp/emergency_core.cpp" "ds cp/graph_algorithms.cpp" "ds cp/emergency_system.cpp" -o emergency_system
./emergency_system
```
