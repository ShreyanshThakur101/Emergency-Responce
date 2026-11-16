# Project Pulse Response: System Architecture & Technical Context

> **City-Wide Emergency Response & Resource Dispatch System (Pune, Maharashtra)**  
> High-performance C++ backend utilizing core Data Structures & Algorithms (DSA), coupled with a real-time map-first web frontend powered by Google Maps.

---

## 1. Executive Overview

**Pulse Response** is an end-to-end simulation system designed to solve city-wide emergency dispatch and fleet repositioning challenges in Pune, India. It models real-time citizen emergency reporting, automated greedy triage, optimal shortest-path routing, 60-second responder acceptance deadlines, automatic rejection/timeout cycling, and demand-weighted standby hub relocation.

The project is structured into two cooperating layers:
1. **Authoritative C++ Core (`ds cp/` & `web_api.cpp`)**: Contains the algorithms (Dijkstra, Kruskal's MST, Priority Queues, BST, Trie, DSU) and hosts a custom, lightweight POSIX socket HTTP server.
2. **Web Frontend (`web/`)**: A responsive single-page application featuring a Citizen Request Portal and a Map-First Responder Operations Desk with interactive Google Maps integration.

---

## 2. Directory & Repository Structure

```
City-Wide-Emergency-Response-Web-App/
├── .env                              # Environment configuration (googleMapApi key, ports)
├── .env.example                      # Configuration template
├── run_web.sh                        # One-command build and run script
├── web_api.cpp                       # POSIX HTTP Web Server & REST API layer
├── test_dispatch_and_accuracy.py     # Automated integration & regression test suite
├── test_rejection_cycle.py           # Automated test suite for multi-unit rejection cycling
├── test_web_api.py                   # Black-box API integration tests
├── ds cp/                            # Core C++ DSA Implementation
│   ├── emergency_core.cpp            # Vehicle fleet, queues, BST, Linked Lists, dispatch core
│   ├── graph_algorithms.cpp          # 16-zone road network, Dijkstra, BFS, DFS, backtracking
│   ├── emergency_system.cpp          # Standby optimization, Kruskal MST, sorting & searching
│   └── main.cpp                      # Interactive CLI terminal interface & viva demos
└── web/                              # Single-Page Web Application
    ├── index.html                    # Citizen & Responder portal DOM structure
    ├── styles.css                    # Responsive layout, map-first 3-column grid, badges
    └── app.js                        # Maps loader, GPS accuracy handler, polling loop & desk
```

---

## 3. C++ Backend & Core Data Structures

The system deliberately avoids heavy external dependencies, implementing classical data structures and algorithms from scratch in modern C++17:

### A. Graph Representation & Routing (`graph_algorithms.cpp`)
- **16 Pune Zones (Vertices)**:
  1. Shivajinagar (`18.5308, 73.8475`)
  2. Deccan (`18.5164, 73.8417`)
  3. Yerwada (`18.5515, 73.8796`)
  4. Viman Nagar (`18.5679, 73.9143`)
  5. Baner (`18.5590, 73.7868`)
  6. FC Road (`18.5236, 73.8417`)
  7. Kalyani Nagar (`18.5480, 73.9032`)
  8. Kharadi (`18.5515, 73.9348`)
  9. Wakad (`18.5975, 73.7639`)
  10. Kothrud (`18.5074, 73.8077`)
  11. Swargate (`18.5018, 73.8636`)
  12. Hadapsar (`18.5089, 73.9260`)
  13. Hinjewadi (`18.5913, 73.7389`)
  14. Warje (`18.4825, 73.8021`)
  15. Camp (`18.5134, 73.8796`)
  16. Kondhwa (`18.4635, 73.8710`)
- **24 Undirected Corridors (Edges)**: Stored in an adjacency list `std::vector<Edge> adj[17]`, each with travel time in minutes and distance in kilometers.
- **Dijkstra's Shortest Path Algorithm (`dijkstra`)**:
  - Uses `std::priority_queue` min-heap on `(travelTime, vertex)`.
  - Reconstructs optimal paths using a custom LIFO stack (`PathStack`) to reverse predecessor pointers.
- **Network Analysis**:
  - **BFS (`bfsCoverage`)**: Calculates reachable zones within $k$-hops from any station.
  - **DFS (`dfsRecursive`)**: Analyzes connected components to detect network isolation.
  - **Backtracking (`findAlternativeRoutes`)**: Discovers distinct alternative paths between locations.
  - **Road Blockage (`blockRoad` / `unblockRoad`)**: Simulates real-time corridor closures and rerouting.

### B. Fleet & Emergency Dispatch Core (`emergency_core.cpp`)
- **Vehicles (`Vehicle`)**: Preset fleet of 8 units distributed across the city:
  - Ambulances: `A01` (Shivajinagar), `A02` (Deccan), `A03` (Kondhwa)
  - Fire Trucks: `F01` (Kharadi), `F02` (Kothrud)
  - Police Units: `P01` (Kalyani Nagar), `P02` (Swargate), `P03` (Hinjewadi)
- **Queues**:
  - **FIFO Intake Queue (`EmergencyQueue`)**: Custom singly-linked list queue preserving chronological arrival order.
  - **Min-Heap Priority Queue (`EmergencyPriorityQueue`)**: Binary heap ordering emergencies by triage priority (`CRITICAL` > `HIGH` > `MEDIUM` > `LOW`), using older emergency IDs as deterministic tie-breakers.
- **Audit & History Structures**:
  - **Linked List (`HistoryLinkedList`)**: Per-vehicle custom linked list storing completed incidents.
  - **Circular Buffer (`CircularEventBuffer`)**: Fixed-capacity ring buffer tracking the last 8 operational events.
  - **Undo Stack (`DispatchUndoStack`)**: LIFO stack to roll back erroneous dispatches.
  - **Binary Search Tree (`EmergencyBST`)**: Indexes historical and active emergencies by ID, supporting search, deletion, and inorder traversal.
  - **Trie (`LocationTrie`)**: 26-ary prefix tree for fast location autocomplete.

### C. Advanced Optimization & Algorithms (`emergency_system.cpp`)
- **Kruskal's Algorithm (`runKruskalMST`)**: Computes the Minimum Spanning Tree across city corridors using Disjoint Set Union (DSU with union by rank and path compression).
- **Sorting Algorithms**:
  - Merge Sort (`runMergeSortEmergencies`) for divide-and-conquer sorting of incident histories.
  - Quick Sort (`runQuickSortVehicles`) for partition-based fleet ranking.
- **Demand-Weighted Standby Repositioning (`getOptimizedStandbyLocations`)**:
  - Evaluates historical emergency demand distribution across all 16 zones.
  - Runs combinatorial optimization over candidate subsets to select $k$ optimal coverage hubs that minimize city-wide expected response latency.
  - When a responder resolves an emergency, the backend evaluates the top 3 hubs based on graph travel time plus hub congestion penalty ($5\text{ min} \times \text{occupied units}$) to choose the optimal standby target.

---

## 4. Web API & Real-Time Operational Layer (`web_api.cpp`)

Built with raw POSIX sockets (`AF_INET`, `SOCK_STREAM`), handling concurrent HTTP/1.1 requests without external frameworks.

### Data Structures:
```cpp
struct ApiEmergency {
    int id;
    std::string type;              // "medical", "fire", "road_accident", "police", "other"
    std::string severity;          // "CRITICAL", "HIGH", "MEDIUM", "LOW"
    std::string status;            // "RECEIVED", "FINDING_RESPONDER", "ASSIGNED", "EN_ROUTE", "AT_SCENE", "IN_PROGRESS", "RESOLVED"
    int locationId;
    double lat, lng;
    std::string address, description, contact;
    std::string assignedVehicleId;
    std::string firstAssignedVehicleId;
    std::string assignedAt;        // ISO 8601 creation timestamp
    std::string deadlineAt;        // assignedAt + 60s
    std::set<std::string> rejectedBy;
    int rejectionCount;
    int standbyLocationId;
};
```

### Endpoints:
- `GET /api/health`: Health status and service validation.
- `GET /api/config`: Returns `googleMapApi`, `mapsConfigured: bool`, and `acceptanceDeadlineSeconds: 60`.
- `GET /api/locations`: Array of 16 Pune zones with latitude/longitude coordinates.
- `GET /api/responders`: Fleet status, unit assignments, metrics, and zone coordinates.
- `GET /api/emergencies`: All emergencies; automatically triggers `checkAndExpireDeadlines()`.
- `POST /api/emergencies`: Citizen submission; runs auto-triage, Dijkstra greedy selection, and initial dispatch.
- `GET /api/emergencies/:id`: Details of a specific emergency.
- `POST /api/emergencies/:id/reject`: Explicit rejection by assigned unit (`{"responderId": "A01", "reason": "..."}`).
- `POST /api/responders/:id/actions`: Responder state transitions:
  - `accept` $\rightarrow$ transitions to `EN_ROUTE`, cancels 60s timer.
  - `arrived` $\rightarrow$ transitions to `AT_SCENE`.
  - `start_work` $\rightarrow$ transitions to `IN_PROGRESS`.
  - `resolve` $\rightarrow$ marks `RESOLVED`, computes optimal standby hub via `getOptimizedStandbyLocations`.
  - `standby_arrived` $\rightarrow$ restores unit availability to `AVAILABLE`.

---

## 5. Automated Dispatch, Rejection & Cyclic Reassignment

The system provides fully automated dispatch with strict deadline and rejection handling:

```
[Citizen Reports Emergency]
              │
              ▼
[Greedy Dijkstra Selection]
(Evaluates compatible, available units)
              │
              ▼
[Assigned to Best Responder (e.g., A03)]
Status: ASSIGNED | Deadline: Now + 60s
              │
      ┌───────┴────────────────────────┐
      ▼                                ▼
[Accept & Start Nav]       [Explicit Rejection OR 60s Timeout]
      │                                │
• Status: EN_ROUTE           • Unit added to e.rejectedBy
• Timer cancelled            • e.rejectionCount++
• Follows operational flow   • webReleaseVehicle frees current unit
                             • reassignEmergency searches next candidate
                                       │
                                       ▼
                         Are all compatible units in e.rejectedBy?
                                ├── NO  ──► Assign next best unit (e.g., A01)
                                └── YES ──► Clear e.rejectedBy & CYCLE BACK
                                            to firstAssignedVehicleId (A03)!
```

### Key Safety Guarantees:
1. **No Lost Requests**: If all compatible responders reject or time out, `e.rejectedBy` resets and the request cycles back to the first responder with a fresh 60-second deadline.
2. **Race-Condition Protection**: Late acceptance attempts on expired or reassigned incidents return HTTP `409 Conflict`.
3. **No Overrides**: Citizens cannot choose responders; dispatch remains 100% algorithm-governed.

---

## 6. Location Accuracy & Geolocation Pipeline

To eliminate the observed discrepancy where users in Kondhwa were misidentified as being on FC Road:

1. **Fresh Hardware Geolocation**:
   `navigator.geolocation.getCurrentPosition` runs with `maximumAge: 0`, forcing hardware sensor reads and bypassing stale city-center cached locks.
2. **Honest Accuracy Classification**:
   - **$\le 100\text{ m}$**: Green pill `GPS ±Xm (High precision)`.
   - **$100\text{ m} < \text{acc} \le 1000\text{ m}$**: Orange pill `Est ±Xm (Wi-Fi/Cellular)`.
   - **$> 1000\text{ m}$**: Red pill `Low Acc ±X km` and a prominent warning box explaining that network/IP geolocation defaulted to Pune city center.
3. **Separation of State Variables**:
   Device GPS coordinates, user-confirmed incident coordinates, reverse-geocoded address, and graph zone IDs are maintained as distinct variables with zero silent overwriting.
4. **Interactive Diagnostics**:
   An expandable `<details>` panel displays raw coordinates, horizontal accuracy, timestamp, confirmed coordinates, and active source.

---

## 7. Map-First Operations Dashboard (`web/`)

The Responder Portal is built as a 3-column operational cockpit:
- **Left Column (Fleet Sidebar)**:
  - Searchable, filterable list of all 8 responders (`All`, `Available`, `Assigned`, `En Route`, `Standby`).
  - Active assignment badges (e.g. `E201`) and status pills.
  - Selecting any card focuses the responder's map pin and loads their desk.
- **Center Column (Interactive Google Map)**:
  - Custom SVG markers distinguish unit roles and states:
    - 🟢 Green: Available unit
    - 🟠 Orange: Awaiting acceptance / assigned unit
    - 🔵 Blue: En route / at scene / in progress unit
    - 🔴 Red: Emergency incident
    - 🟣 Purple: Standby coverage hub
  - Layer toggles (Units, Incidents, Standby Hubs).
  - `⤢ Overview` button dynamically fits map bounds (`google.maps.LatLngBounds`).
  - Clickable markers synchronize with sidebar and detail panels.
- **Right Column (Operations Desk)**:
  - Dynamic display adapting to the responder's state.
  - Active 60-second countdown bar with live progress fill.
  - Prominent `Accept & Start Navigation ↗` and `Reject Assignment ✕` buttons.
  - One-click Google Maps turn-by-turn driving links.

---

## 8. Verification & Test Suites

The codebase includes automated test suites covering all operational requirements:

```bash
# 1. Dispatch, accuracy, and full workflow test suite
python3 test_dispatch_and_accuracy.py

# 2. Multi-unit rejection cycle test suite (verifies cyclic reassignment)
python3 test_rejection_cycle.py

# 3. Web API integration test suite
python3 test_web_api.py
```

### Verification Checklist:
- [x] Config endpoint returns `googleMapApi` and `mapsConfigured: true`.
- [x] All 16 zones and 8 responders return valid coordinates.
- [x] Citizen submission correctly maps Kondhwa incidents without FC Road fallback.
- [x] 60-second acceptance timer enforces deadline server-side.
- [x] Multi-unit rejections cycle back to the initial responder indefinitely.
- [x] Completed emergencies route responders to optimized standby hubs.
- [x] Standard C++ graph algorithms remain 100% operational.

---

## 9. Rebuild & Execution Reference

```bash
# Switch to project directory
cd "/home/thakur/Documents/da cp/City-Wide-Emergency-Response-Web-App"

# Compile executable
g++ -std=c++17 -O2 -Wall -Wextra -pedantic \
  web_api.cpp "ds cp/emergency_core.cpp" "ds cp/graph_algorithms.cpp" "ds cp/emergency_system.cpp" \
  -o emergency_web

# Run server (default binding: http://127.0.0.1:8765)
./emergency_web
# Or use the convenience script
./run_web.sh
```
