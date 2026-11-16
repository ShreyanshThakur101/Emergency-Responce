#include <iostream>
#include <vector>
#include <queue>
#include <stack>
#include <string>
#include <algorithm>
#include <climits>
#include <iomanip>

// ============================================================
// PUBLIC INTERFACE
// ============================================================

struct Edge {
    int destination;
    int travelTime;
    double distance;
    bool blocked;
};

struct RouteResult {
    int travelTime;
    double distance;
    std::vector<int> path;
    bool found;
};

struct Location {
    int id;
    std::string name;
};

// Functions exported for other modules
void initGraph();
std::string getLocationName(int id);
int getLocationId(const std::string& name);
int getTotalLocations();
RouteResult dijkstra(int src, int dest);
void blockRoad(int u, int v);
void unblockRoad(int u, int v);
bool isRoadBlocked(int u, int v);
std::vector<Edge> getEdgesFrom(int u);
std::vector<int> bfsCoverage(int startId, int maxHops);
void printBfsCoverage(int startId, int maxHops);
int getConnectedComponentsCount();
void printConnectivityReport();
std::vector<std::vector<int>> findAlternativeRoutes(int src, int dest, int maxRoutes);
void printAlternativeRoutes(int src, int dest);
void displayCityRoads();
void runGraphDemos();
std::vector<std::vector<int>> getAllUndirectedEdges();
void printAllPairsShortestPaths();
void printLocationNeighbors(int locId);
void printGraphTopologyStats();
void dijkstraWithTrace(int src, int dest);

// ============================================================
// INTERNAL IMPLEMENTATION
// ============================================================

namespace {
    const int TOTAL_LOCATIONS = 16;
    Location locations[TOTAL_LOCATIONS + 1];
    std::vector<Edge> adj[TOTAL_LOCATIONS + 1];

    void addRoad(int u, int v, int time, double dist) {
        Edge e1;
        e1.destination = v;
        e1.travelTime = time;
        e1.distance = dist;
        e1.blocked = false;
        adj[u].push_back(e1);

        Edge e2;
        e2.destination = u;
        e2.travelTime = time;
        e2.distance = dist;
        e2.blocked = false;
        adj[v].push_back(e2);
    }
}

void initGraph() {
    locations[1] = {1, "Shivajinagar"};
    locations[2] = {2, "Deccan"};
    locations[3] = {3, "Yerwada"};
    locations[4] = {4, "Viman Nagar"};
    locations[5] = {5, "Baner"};
    locations[6] = {6, "FC Road"};
    locations[7] = {7, "Kalyani Nagar"};
    locations[8] = {8, "Kharadi"};
    locations[9] = {9, "Wakad"};
    locations[10] = {10, "Kothrud"};
    locations[11] = {11, "Swargate"};
    locations[12] = {12, "Hadapsar"};
    locations[13] = {13, "Hinjewadi"};
    locations[14] = {14, "Warje"};
    locations[15] = {15, "Camp"};
    locations[16] = {16, "Kondhwa"};

    for (int i = 1; i <= TOTAL_LOCATIONS; i++) {
        adj[i].clear();
    }

    // Horizontal roads
    addRoad(1, 2, 5, 3.5);
    addRoad(2, 3, 7, 5.0);
    addRoad(3, 4, 6, 4.2);

    addRoad(5, 6, 8, 6.0);
    addRoad(6, 7, 9, 6.8);
    addRoad(7, 8, 6, 4.5);

    addRoad(9, 10, 7, 5.5);
    addRoad(10, 11, 8, 6.2);
    addRoad(11, 12, 10, 7.5);

    addRoad(13, 14, 9, 7.0);
    addRoad(14, 15, 11, 8.5);
    addRoad(15, 16, 8, 6.0);

    // Vertical roads
    addRoad(1, 5, 9, 7.2);
    addRoad(2, 6, 4, 2.8);
    addRoad(3, 7, 5, 3.8);
    addRoad(4, 8, 7, 5.2);

    addRoad(5, 9, 6, 4.8);
    addRoad(6, 10, 5, 3.5);
    addRoad(7, 11, 8, 6.0);
    addRoad(8, 12, 9, 7.0);

    addRoad(9, 13, 5, 4.0);
    addRoad(10, 14, 6, 4.5);
    addRoad(11, 15, 4, 3.0);
    addRoad(12, 16, 7, 5.5);

    // Diagonal bypass roads
    addRoad(1, 6, 4, 3.0);
    addRoad(2, 10, 6, 4.2);
    addRoad(6, 11, 7, 5.0);
    addRoad(3, 8, 11, 8.5);
}

std::string getLocationName(int id) {
    if (id >= 1 && id <= TOTAL_LOCATIONS) {
        return locations[id].name;
    }
    return "Unknown";
}

int getLocationId(const std::string& name) {
    for (int i = 1; i <= TOTAL_LOCATIONS; i++) {
        if (locations[i].name == name) {
            return i;
        }
    }
    return -1;
}

int getTotalLocations() {
    return TOTAL_LOCATIONS;
}

std::vector<Edge> getEdgesFrom(int u) {
    if (u >= 1 && u <= TOTAL_LOCATIONS) {
        return adj[u];
    }
    return {};
}

void blockRoad(int u, int v) {
    if (u < 1 || u > TOTAL_LOCATIONS || v < 1 || v > TOTAL_LOCATIONS) return;

    for (size_t i = 0; i < adj[u].size(); i++) {
        if (adj[u][i].destination == v) {
            adj[u][i].blocked = true;
        }
    }
    for (size_t i = 0; i < adj[v].size(); i++) {
        if (adj[v][i].destination == u) {
            adj[v][i].blocked = true;
        }
    }
}

void unblockRoad(int u, int v) {
    if (u < 1 || u > TOTAL_LOCATIONS || v < 1 || v > TOTAL_LOCATIONS) return;

    for (size_t i = 0; i < adj[u].size(); i++) {
        if (adj[u][i].destination == v) {
            adj[u][i].blocked = false;
        }
    }
    for (size_t i = 0; i < adj[v].size(); i++) {
        if (adj[v][i].destination == u) {
            adj[v][i].blocked = false;
        }
    }
}

bool isRoadBlocked(int u, int v) {
    if (u < 1 || u > TOTAL_LOCATIONS || v < 1 || v > TOTAL_LOCATIONS) return false;

    for (size_t i = 0; i < adj[u].size(); i++) {
        if (adj[u][i].destination == v) {
            return adj[u][i].blocked;
        }
    }
    return false;
}

// Custom Stack for Route Reconstruction
struct PathStack {
    int elements[TOTAL_LOCATIONS + 5];
    int topIndex;

    PathStack() {
        topIndex = -1;
    }

    void push(int val) {
        if (topIndex < TOTAL_LOCATIONS + 4) {
            topIndex++;
            elements[topIndex] = val;
        }
    }

    int pop() {
        if (topIndex >= 0) {
            int val = elements[topIndex];
            topIndex--;
            return val;
        }
        return -1;
    }

    bool isEmpty() const {
        return topIndex == -1;
    }
};

// Dijkstra shortest path using min-priority queue
RouteResult dijkstra(int src, int dest) {
    RouteResult result;
    result.travelTime = 0;
    result.distance = 0.0;
    result.found = false;

    if (src < 1 || src > TOTAL_LOCATIONS || dest < 1 || dest > TOTAL_LOCATIONS) {
        return result;
    }

    if (src == dest) {
        result.found = true;
        result.path.push_back(src);
        return result;
    }

    int dist[TOTAL_LOCATIONS + 1];
    int parent[TOTAL_LOCATIONS + 1];
    bool visited[TOTAL_LOCATIONS + 1];

    for (int i = 1; i <= TOTAL_LOCATIONS; i++) {
        dist[i] = INT_MAX;
        parent[i] = -1;
        visited[i] = false;
    }

    // Min-priority queue: pair of (distance, vertex)
    std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<std::pair<int, int>>> pq;

    dist[src] = 0;
    pq.push({0, src});

    while (!pq.empty()) {
        int u = pq.top().second;
        pq.pop();

        if (visited[u]) continue;
        visited[u] = true;

        if (u == dest) break;

        for (size_t i = 0; i < adj[u].size(); i++) {
            Edge e = adj[u][i];
            if (e.blocked) continue;

            int v = e.destination;
            int weight = e.travelTime;

            if (!visited[v] && dist[u] + weight < dist[v]) {
                dist[v] = dist[u] + weight;
                parent[v] = u;
                pq.push({dist[v], v});
            }
        }
    }

    if (dist[dest] == INT_MAX) {
        return result;
    }

    result.found = true;
    result.travelTime = dist[dest];

    // Reconstruct path using custom Stack
    PathStack st;
    int curr = dest;
    while (curr != -1) {
        st.push(curr);
        curr = parent[curr];
    }

    while (!st.isEmpty()) {
        result.path.push_back(st.pop());
    }

    // Calculate total distance along the path
    double totalDist = 0.0;
    for (size_t i = 0; i + 1 < result.path.size(); i++) {
        int u = result.path[i];
        int v = result.path[i + 1];
        for (size_t j = 0; j < adj[u].size(); j++) {
            if (adj[u][j].destination == v) {
                totalDist += adj[u][j].distance;
                break;
            }
        }
    }
    result.distance = totalDist;

    return result;
}

// Breadth-First Search for nearby zone coverage
std::vector<int> bfsCoverage(int startId, int maxHops) {
    std::vector<int> reachable;
    if (startId < 1 || startId > TOTAL_LOCATIONS || maxHops < 0) return reachable;

    bool visited[TOTAL_LOCATIONS + 1] = {false};
    int hops[TOTAL_LOCATIONS + 1] = {0};

    std::queue<int> q;
    visited[startId] = true;
    hops[startId] = 0;
    q.push(startId);

    while (!q.empty()) {
        int u = q.front();
        q.pop();

        reachable.push_back(u);

        if (hops[u] < maxHops) {
            for (size_t i = 0; i < adj[u].size(); i++) {
                if (adj[u][i].blocked) continue;
                int v = adj[u][i].destination;
                if (!visited[v]) {
                    visited[v] = true;
                    hops[v] = hops[u] + 1;
                    q.push(v);
                }
            }
        }
    }

    return reachable;
}

void printBfsCoverage(int startId, int maxHops) {
    std::cout << "\n============================================================\n";
    std::cout << "           BFS ZONE COVERAGE ANALYSIS\n";
    std::cout << "============================================================\n";
    std::cout << "Center Station: [" << startId << "] " << getLocationName(startId) << "\n";
    std::cout << "Maximum Hops  : " << maxHops << "\n";
    std::cout << "------------------------------------------------------------\n";

    std::vector<int> reachable = bfsCoverage(startId, maxHops);
    std::cout << "Reachable Zones (" << reachable.size() << " total):\n";
    for (size_t i = 0; i < reachable.size(); i++) {
        int id = reachable[i];
        std::cout << "  - [" << id << "] " << getLocationName(id);
        if (id == startId) std::cout << " (Origin Hub)";
        std::cout << "\n";
    }
}

// Recursive DFS for connectivity analysis
namespace {
    void dfsRecursive(int u, bool visited[], std::vector<int>& component) {
        visited[u] = true;
        component.push_back(u);

        for (size_t i = 0; i < adj[u].size(); i++) {
            if (adj[u][i].blocked) continue;
            int v = adj[u][i].destination;
            if (!visited[v]) {
                dfsRecursive(v, visited, component);
            }
        }
    }
}

int getConnectedComponentsCount() {
    bool visited[TOTAL_LOCATIONS + 1] = {false};
    int count = 0;

    for (int i = 1; i <= TOTAL_LOCATIONS; i++) {
        if (!visited[i]) {
            std::vector<int> comp;
            dfsRecursive(i, visited, comp);
            count++;
        }
    }
    return count;
}

void printConnectivityReport() {
    std::cout << "\n============================================================\n";
    std::cout << "           CITY CONNECTIVITY ANALYSIS (DFS)\n";
    std::cout << "============================================================\n";
    std::cout << "Total Locations: " << TOTAL_LOCATIONS << "\n";

    bool visited[TOTAL_LOCATIONS + 1] = {false};
    std::vector<std::vector<int>> components;

    for (int i = 1; i <= TOTAL_LOCATIONS; i++) {
        if (!visited[i]) {
            std::vector<int> comp;
            dfsRecursive(i, visited, comp);
            components.push_back(comp);
        }
    }

    std::cout << "Connected Components: " << components.size() << "\n";
    std::cout << "------------------------------------------------------------\n";

    if (components.size() == 1) {
        std::cout << "Status: CITY FULLY CONNECTED\n";
        std::cout << "All emergency services can reach every location.\n";
    } else {
        std::cout << "WARNING: Some locations are currently isolated!\n";
        for (size_t i = 0; i < components.size(); i++) {
            std::cout << "Component " << (i + 1) << " (" << components[i].size() << " locations): ";
            for (size_t j = 0; j < components[i].size(); j++) {
                std::cout << getLocationName(components[i][j]);
                if (j + 1 < components[i].size()) std::cout << ", ";
            }
            std::cout << "\n";
        }
    }
}

// Backtracking for alternative routes
namespace {
    void backtrackPaths(int curr, int dest, bool visited[], std::vector<int>& currentPath,
                        std::vector<std::vector<int>>& allRoutes, int maxRoutes) {
        if (allRoutes.size() >= static_cast<size_t>(maxRoutes)) {
            return;
        }

        visited[curr] = true;
        currentPath.push_back(curr);

        if (curr == dest) {
            allRoutes.push_back(currentPath);
        } else {
            for (size_t i = 0; i < adj[curr].size(); i++) {
                if (adj[curr][i].blocked) continue;
                int nextNode = adj[curr][i].destination;
                if (!visited[nextNode]) {
                    backtrackPaths(nextNode, dest, visited, currentPath, allRoutes, maxRoutes);
                }
            }
        }

        currentPath.pop_back();
        visited[curr] = false;
    }
}

std::vector<std::vector<int>> findAlternativeRoutes(int src, int dest, int maxRoutes) {
    std::vector<std::vector<int>> routes;
    if (src < 1 || src > TOTAL_LOCATIONS || dest < 1 || dest > TOTAL_LOCATIONS) return routes;

    bool visited[TOTAL_LOCATIONS + 1] = {false};
    std::vector<int> currentPath;

    backtrackPaths(src, dest, visited, currentPath, routes, maxRoutes);
    return routes;
}

void printAlternativeRoutes(int src, int dest) {
    std::cout << "\n============================================================\n";
    std::cout << "        ALTERNATIVE ROUTE EXPLORATION (BACKTRACKING)\n";
    std::cout << "============================================================\n";
    std::cout << "From: [" << src << "] " << getLocationName(src) << "\n";
    std::cout << "To  : [" << dest << "] " << getLocationName(dest) << "\n";
    std::cout << "------------------------------------------------------------\n";

    std::vector<std::vector<int>> routes = findAlternativeRoutes(src, dest, 5);
    if (routes.empty()) {
        std::cout << "No viable paths found between these locations.\n";
        return;
    }

    std::cout << "Found " << routes.size() << " distinct viable route(s):\n\n";
    for (size_t i = 0; i < routes.size(); i++) {
        int totalTime = 0;
        double totalDist = 0.0;

        for (size_t j = 0; j + 1 < routes[i].size(); j++) {
            int u = routes[i][j];
            int v = routes[i][j + 1];
            for (size_t k = 0; k < adj[u].size(); k++) {
                if (adj[u][k].destination == v) {
                    totalTime += adj[u][k].travelTime;
                    totalDist += adj[u][k].distance;
                    break;
                }
            }
        }

        std::cout << "Route Option " << (i + 1) << " (Time: " << totalTime << " min, Dist: " << totalDist << " km):\n  ";
        for (size_t j = 0; j < routes[i].size(); j++) {
            std::cout << getLocationName(routes[i][j]);
            if (j + 1 < routes[i].size()) std::cout << " -> ";
        }
        std::cout << "\n\n";
    }
}

void displayCityRoads() {
    std::cout << "\n============================================================\n";
    std::cout << "                 CITY ROAD NETWORK STATUS\n";
    std::cout << "============================================================\n";
    for (int i = 1; i <= TOTAL_LOCATIONS; i++) {
        std::cout << "[" << i << "] " << locations[i].name << " connections:\n";
        for (size_t j = 0; j < adj[i].size(); j++) {
            Edge e = adj[i][j];
            std::cout << "    -> [" << e.destination << "] " << locations[e.destination].name
                      << " | " << e.travelTime << " min | " << e.distance << " km"
                      << (e.blocked ? " [BLOCKED]" : " [OPEN]") << "\n";
        }
    }
}

std::vector<std::vector<int>> getAllUndirectedEdges() {
    std::vector<std::vector<int>> edges;
    for (int u = 1; u <= TOTAL_LOCATIONS; u++) {
        for (size_t i = 0; i < adj[u].size(); i++) {
            int v = adj[u][i].destination;
            if (u < v) {
                // edge format: {u, v, travelTime, (int)distance}
                edges.push_back({u, v, adj[u][i].travelTime, static_cast<int>(adj[u][i].distance * 10)});
            }
        }
    }
    return edges;
}

void runGraphDemos() {
    std::cout << "\n--- Graph Algorithm Demonstrations ---\n";
    std::cout << "1. Dijkstra Shortest Route (Deccan to Kothrud)\n";
    RouteResult r = dijkstra(2, 10);
    if (r.found) {
        std::cout << "Shortest Travel Time: " << r.travelTime << " min\n";
        std::cout << "Distance: " << r.distance << " km\nPath: ";
        for (size_t i = 0; i < r.path.size(); i++) {
            std::cout << getLocationName(r.path[i]);
            if (i + 1 < r.path.size()) std::cout << " -> ";
        }
        std::cout << "\n";
    }

    std::cout << "\n2. BFS 2-Hop Coverage from Shivajinagar\n";
    printBfsCoverage(1, 2);

    std::cout << "\n3. DFS Connectivity Analysis\n";
    printConnectivityReport();

    std::cout << "\n4. Road Blockage Simulation\n";
    std::cout << "Blocking road: Deccan <-> FC Road (2 <-> 6)...\n";
    blockRoad(2, 6);
    RouteResult rBlocked = dijkstra(2, 10);
    std::cout << "Rerouted Path after blockage: ";
    for (size_t i = 0; i < rBlocked.path.size(); i++) {
        std::cout << getLocationName(rBlocked.path[i]);
        if (i + 1 < rBlocked.path.size()) std::cout << " -> ";
    }
    std::cout << " (Time: " << rBlocked.travelTime << " min)\n";
    std::cout << "Unblocking road: Deccan <-> FC Road...\n";
    unblockRoad(2, 6);
}

void printLocationNeighbors(int locId) {
    if (locId < 1 || locId > TOTAL_LOCATIONS) {
        std::cout << "Invalid location ID.\n";
        return;
    }
    std::cout << "\nDirect Connected Neighbors for [" << locId << "] " << getLocationName(locId) << ":\n";
    std::cout << "------------------------------------------------------------\n";
    for (size_t i = 0; i < adj[locId].size(); i++) {
        Edge e = adj[locId][i];
        std::cout << "  -> [" << e.destination << "] " << getLocationName(e.destination)
                  << " | Time: " << e.travelTime << " min | Dist: " << e.distance << " km"
                  << (e.blocked ? " [BLOCKED]" : " [OPEN]") << "\n";
    }
}

void printGraphTopologyStats() {
    std::cout << "\n============================================================\n";
    std::cout << "             CITY ROAD TOPOLOGY & METRICS\n";
    std::cout << "============================================================\n";
    std::cout << "Total Zones (Vertices) : " << TOTAL_LOCATIONS << "\n";

    int totalEdges = 0;
    int minDeg = 999;
    int maxDeg = -1;
    int degSum = 0;

    for (int i = 1; i <= TOTAL_LOCATIONS; i++) {
        int d = static_cast<int>(adj[i].size());
        totalEdges += d;
        degSum += d;
        if (d < minDeg) minDeg = d;
        if (d > maxDeg) maxDeg = d;
    }
    totalEdges /= 2; // Each undirected edge counted twice

    double avgDeg = static_cast<double>(degSum) / TOTAL_LOCATIONS;
    double density = (2.0 * totalEdges) / (TOTAL_LOCATIONS * (TOTAL_LOCATIONS - 1));

    // Calculate graph diameter (maximum shortest path among all pairs)
    int diameter = 0;
    int diamSrc = 1, diamDest = 1;
    for (int i = 1; i <= TOTAL_LOCATIONS; i++) {
        for (int j = i + 1; j <= TOTAL_LOCATIONS; j++) {
            RouteResult r = dijkstra(i, j);
            if (r.found && r.travelTime > diameter) {
                diameter = r.travelTime;
                diamSrc = i;
                diamDest = j;
            }
        }
    }

    std::cout << "Total Two-Way Corridors: " << totalEdges << "\n";
    std::cout << "Average Node Degree    : " << avgDeg << "\n";
    std::cout << "Minimum Degree         : " << minDeg << " (Outlying zones)\n";
    std::cout << "Maximum Degree         : " << maxDeg << " (Central hubs like FC Road)\n";
    std::cout << "Graph Density          : " << density << "\n";
    std::cout << "Network Diameter       : " << diameter << " minutes ("
              << getLocationName(diamSrc) << " <-> " << getLocationName(diamDest) << ")\n";
    std::cout << "------------------------------------------------------------\n";
}

void printAllPairsShortestPaths() {
    std::cout << "\n========================================================================\n";
    std::cout << "       ALL-PAIRS SHORTEST TRAVEL TIME MATRIX (16x16 CITY GRID)\n";
    std::cout << "========================================================================\n";
    std::cout << "ID ";
    for (int j = 1; j <= TOTAL_LOCATIONS; j++) {
        std::cout << std::setw(4) << j;
    }
    std::cout << "\n------------------------------------------------------------------------\n";

    for (int i = 1; i <= TOTAL_LOCATIONS; i++) {
        std::cout << std::setw(2) << i << " ";
        for (int j = 1; j <= TOTAL_LOCATIONS; j++) {
            if (i == j) {
                std::cout << std::setw(4) << 0;
            } else {
                RouteResult r = dijkstra(i, j);
                if (r.found) {
                    std::cout << std::setw(4) << r.travelTime;
                } else {
                    std::cout << std::setw(4) << "X";
                }
            }
        }
        std::cout << "  (" << getLocationName(i) << ")\n";
    }
    std::cout << "========================================================================\n";
}

void dijkstraWithTrace(int src, int dest) {
    std::cout << "\n============================================================\n";
    std::cout << "          DIJKSTRA STEP-BY-STEP RELAXATION TRACE\n";
    std::cout << "============================================================\n";
    std::cout << "Origin     : [" << src << "] " << getLocationName(src) << "\n";
    std::cout << "Destination: [" << dest << "] " << getLocationName(dest) << "\n";
    std::cout << "------------------------------------------------------------\n";

    int dist[TOTAL_LOCATIONS + 1];
    int parent[TOTAL_LOCATIONS + 1];
    bool visited[TOTAL_LOCATIONS + 1];

    for (int i = 1; i <= TOTAL_LOCATIONS; i++) {
        dist[i] = INT_MAX;
        parent[i] = -1;
        visited[i] = false;
    }

    std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<std::pair<int, int>>> pq;
    dist[src] = 0;
    pq.push({0, src});

    int step = 1;
    while (!pq.empty()) {
        int u = pq.top().second;
        int d = pq.top().first;
        pq.pop();

        if (visited[u]) continue;
        visited[u] = true;

        std::cout << "Step " << step++ << ": Extracted Min Node [" << u << "] "
                  << getLocationName(u) << " (Current Dist: " << d << " min)\n";

        if (u == dest) {
            std::cout << "  -> Target destination reached! Stopping search.\n";
            break;
        }

        for (size_t i = 0; i < adj[u].size(); i++) {
            Edge e = adj[u][i];
            if (e.blocked) continue;
            int v = e.destination;
            int weight = e.travelTime;

            if (!visited[v] && dist[u] + weight < dist[v]) {
                dist[v] = dist[u] + weight;
                parent[v] = u;
                pq.push({dist[v], v});
                std::cout << "     Relaxed edge to [" << v << "] " << getLocationName(v)
                          << " -> New Best Time: " << dist[v] << " min\n";
            }
        }
    }

    if (dist[dest] != INT_MAX) {
        std::cout << "\nTrace Route Reconstruction (Stack reversal from Predecessor array):\n  ";
        PathStack st;
        int curr = dest;
        while (curr != -1) {
            st.push(curr);
            curr = parent[curr];
        }
        while (!st.isEmpty()) {
            std::cout << getLocationName(st.pop());
            if (!st.isEmpty()) std::cout << " -> ";
        }
        std::cout << " (Total: " << dist[dest] << " minutes)\n";
    }
}
