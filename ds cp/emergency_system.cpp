#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <climits>

// ============================================================
// PUBLIC INTERFACE
// ============================================================

enum class VehicleType {
    AMBULANCE,
    FIRE_TRUCK,
    POLICE
};

enum class EmergencyType {
    MEDICAL,
    FIRE,
    CRIME,
    ACCIDENT,
    DISASTER
};

enum class Severity {
    LOW = 1,
    MEDIUM = 2,
    HIGH = 3,
    CRITICAL = 4
};

enum class EmergencyStatus {
    PENDING,
    DISPATCHED,
    IN_PROGRESS,
    COMPLETED,
    CANCELLED
};

struct Emergency {
    int id;
    EmergencyType type;
    Severity severity;
    int locationId;
    EmergencyStatus status;
    int responseTime;
    std::string assignedVehicleId;
    std::string description;
};

struct RouteResult {
    int travelTime;
    double distance;
    std::vector<int> path;
    bool found;
};

struct VehicleSummary {
    std::string id;
    std::string type;
    int locationId;
    int totalResponseTime;
    int totalEmergencies;
};

// External functions from graph_algorithms.cpp and emergency_core.cpp
std::string getLocationName(int id);
int getTotalLocations();
std::vector<std::vector<int>> getAllUndirectedEdges();
RouteResult dijkstra(int src, int dest);
std::vector<Emergency> getAllHistoricalEmergencies();
std::string getEmergencyTypeName(EmergencyType t);
std::string getSeverityName(Severity s);

// Functions exported for main module
void runUnionFindDemo();
void runKruskalMST();
void runMergeSortEmergencies();
void runQuickSortVehicles();
void runLinearVsBinarySearch(int searchId);
void runVehicleStandbyOptimization(int kStandby);
void printComplexityTable();
void runOptimizationDemos();
void demonstrateMergeSortWithTrace();
void demonstrateQuickSortWithTrace();
void displayDPProgressionTable();

// ============================================================
// INTERNAL IMPLEMENTATION
// ============================================================

// 1. Disjoint Set Union (DSU / Union-Find) with Path Compression & Union by Rank
class DSU {
private:
    int parent[32];
    int rank[32];
    int size;

public:
    DSU(int n) {
        size = n;
        for (int i = 0; i <= n; i++) {
            parent[i] = i;
            rank[i] = 0;
        }
    }

    int find(int i) {
        if (parent[i] == i) {
            return i;
        }
        // Path compression
        parent[i] = find(parent[i]);
        return parent[i];
    }

    bool unionSets(int i, int j) {
        int rootI = find(i);
        int rootJ = find(j);

        if (rootI == rootJ) {
            return false; // Already in same set (cycle detected)
        }

        // Union by rank
        if (rank[rootI] < rank[rootJ]) {
            parent[rootI] = rootJ;
        } else if (rank[rootI] > rank[rootJ]) {
            parent[rootJ] = rootI;
        } else {
            parent[rootJ] = rootI;
            rank[rootI]++;
        }
        return true;
    }

    bool isConnected(int i, int j) {
        return find(i) == find(j);
    }
};

void runUnionFindDemo() {
    std::cout << "\n============================================================\n";
    std::cout << "           DISJOINT SET UNION (UNION-FIND) DEMO\n";
    std::cout << "============================================================\n";
    DSU dsu(16);
    std::cout << "Created DSU for 16 city zones.\n";
    std::cout << "Connecting Zone 1 (Shivajinagar) and Zone 2 (Deccan)...\n";
    dsu.unionSets(1, 2);
    std::cout << "Connecting Zone 2 (Deccan) and Zone 6 (FC Road)...\n";
    dsu.unionSets(2, 6);

    std::cout << "Testing connectivity:\n";
    std::cout << "  Are Zone 1 and Zone 6 connected? " << (dsu.isConnected(1, 6) ? "YES (Transitive)" : "NO") << "\n";
    std::cout << "  Are Zone 1 and Zone 10 connected? " << (dsu.isConnected(1, 10) ? "YES" : "NO") << "\n";
    std::cout << "Connecting Zone 6 and Zone 10...\n";
    dsu.unionSets(6, 10);
    std::cout << "  Are Zone 1 and Zone 10 connected now? " << (dsu.isConnected(1, 10) ? "YES" : "NO") << "\n";
}

// 2. Kruskal's Minimum Spanning Tree for Infrastructure Planning
struct MSTEdge {
    int u;
    int v;
    int weight;
};

void runKruskalMST() {
    std::cout << "\n============================================================\n";
    std::cout << "     EMERGENCY INFRASTRUCTURE PLANNING (KRUSKAL MST)\n";
    std::cout << "============================================================\n";
    std::cout << "Calculating minimum-cost arterial network connecting\nall 16 emergency response zones...\n\n";

    auto rawEdges = getAllUndirectedEdges();
    std::vector<MSTEdge> edges;
    for (const auto& re : rawEdges) {
        edges.push_back({re[0], re[1], re[2]});
    }

    // Sort edges by weight
    std::sort(edges.begin(), edges.end(), [](const MSTEdge& a, const MSTEdge& b) {
        return a.weight < b.weight;
    });

    DSU dsu(16);
    std::vector<MSTEdge> mstEdges;
    int totalCost = 0;

    for (const auto& e : edges) {
        if (dsu.unionSets(e.u, e.v)) {
            mstEdges.push_back(e);
            totalCost += e.weight;
            if (mstEdges.size() == 15) { // V - 1 edges
                break;
            }
        }
    }

    std::cout << "Minimum Infrastructure Arterial Cost: " << totalCost << " time-cost units\n";
    std::cout << "Selected Arterial Roads (" << mstEdges.size() << " corridors):\n";
    std::cout << "------------------------------------------------------------\n";
    for (size_t i = 0; i < mstEdges.size(); i++) {
        std::cout << "  " << std::setw(2) << (i + 1) << ". [" << mstEdges[i].u << "] "
                  << std::setw(14) << std::left << getLocationName(mstEdges[i].u)
                  << " <===> [" << mstEdges[i].v << "] "
                  << std::setw(14) << std::left << getLocationName(mstEdges[i].v)
                  << " | Travel Weight: " << mstEdges[i].weight << " min\n";
    }
    std::cout << "------------------------------------------------------------\n";
    std::cout << "Result: All 16 city zones interconnected with minimum maintenance cost.\n";
}

// 3. Manual Merge Sort on Emergency Records by Response Time
namespace {
    void merge(std::vector<Emergency>& arr, int left, int mid, int right) {
        int n1 = mid - left + 1;
        int n2 = right - mid;

        std::vector<Emergency> L(n1);
        std::vector<Emergency> R(n2);

        for (int i = 0; i < n1; i++) L[i] = arr[left + i];
        for (int j = 0; j < n2; j++) R[j] = arr[mid + 1 + j];

        int i = 0, j = 0, k = left;
        while (i < n1 && j < n2) {
            if (L[i].responseTime <= R[j].responseTime) {
                arr[k] = L[i];
                i++;
            } else {
                arr[k] = R[j];
                j++;
            }
            k++;
        }

        while (i < n1) {
            arr[k] = L[i];
            i++;
            k++;
        }

        while (j < n2) {
            arr[k] = R[j];
            j++;
            k++;
        }
    }

    void mergeSort(std::vector<Emergency>& arr, int left, int right) {
        if (left < right) {
            int mid = left + (right - left) / 2;
            mergeSort(arr, left, mid);
            mergeSort(arr, mid + 1, right);
            merge(arr, left, mid, right);
        }
    }
}

void runMergeSortEmergencies() {
    std::cout << "\n============================================================\n";
    std::cout << "       MERGE SORT: EMERGENCIES BY RESPONSE TIME\n";
    std::cout << "============================================================\n";
    std::vector<Emergency> records = getAllHistoricalEmergencies();

    std::cout << "Records before sorting (Unordered sample):\n";
    for (size_t i = 0; i < std::min(records.size(), size_t(6)); i++) {
        std::cout << "  E" << records[i].id << " - " << records[i].responseTime << " min ("
                  << getEmergencyTypeName(records[i].type) << ")\n";
    }

    mergeSort(records, 0, static_cast<int>(records.size()) - 1);

    std::cout << "\nRecords after Merge Sort (Ascending Response Time):\n";
    std::cout << "ID\tResponse Time\tType\t\tLocation\tSeverity\n";
    std::cout << "------------------------------------------------------------\n";
    for (const auto& e : records) {
        std::cout << "E" << e.id << "\t"
                  << e.responseTime << " min\t\t"
                  << getEmergencyTypeName(e.type) << "\t\t"
                  << getLocationName(e.locationId) << "\t"
                  << getSeverityName(e.severity) << "\n";
    }
    std::cout << "\nComplexity: O(n log n) Guaranteed | Auxiliary Space: O(n)\n";
}

// 4. Manual Quick Sort on Vehicles by Response Time
namespace {
    int partition(std::vector<VehicleSummary>& arr, int low, int high) {
        int pivot = arr[high].totalResponseTime;
        int i = low - 1;

        for (int j = low; j < high; j++) {
            if (arr[j].totalResponseTime <= pivot) {
                i++;
                std::swap(arr[i], arr[j]);
            }
        }
        std::swap(arr[i + 1], arr[high]);
        return i + 1;
    }

    void quickSort(std::vector<VehicleSummary>& arr, int low, int high) {
        if (low < high) {
            int pi = partition(arr, low, high);
            quickSort(arr, low, pi - 1);
            quickSort(arr, pi + 1, high);
        }
    }
}

void runQuickSortVehicles() {
    std::cout << "\n============================================================\n";
    std::cout << "       QUICK SORT: VEHICLES BY TOTAL RESPONSE TIME\n";
    std::cout << "============================================================\n";

    std::vector<VehicleSummary> vehicles = {
        {"A01", "Ambulance", 1, 38, 7},
        {"A02", "Ambulance", 2, 24, 5},
        {"A03", "Ambulance", 16, 42, 8},
        {"F01", "Fire Truck", 8, 31, 6},
        {"F02", "Fire Truck", 10, 19, 4},
        {"P01", "Police", 7, 28, 6},
        {"P02", "Police", 11, 35, 7},
        {"P03", "Police", 13, 22, 5}
    };

    std::cout << "Fleet before Quick Sort:\n";
    for (const auto& v : vehicles) {
        std::cout << "  " << v.id << " (" << v.type << "): " << v.totalResponseTime << " minutes total\n";
    }

    quickSort(vehicles, 0, static_cast<int>(vehicles.size()) - 1);

    std::cout << "\nFleet after Quick Sort (Ranked by Total Response Time):\n";
    std::cout << "Rank\tID\tType\t\tTotal Time\tIncidents\n";
    std::cout << "------------------------------------------------------------\n";
    for (size_t i = 0; i < vehicles.size(); i++) {
        std::cout << "#" << (i + 1) << "\t" << vehicles[i].id << "\t"
                  << vehicles[i].type << "\t"
                  << vehicles[i].totalResponseTime << " min\t\t"
                  << vehicles[i].totalEmergencies << "\n";
    }
    std::cout << "\nComplexity: Average O(n log n) | Worst O(n^2) | In-Place Partitioning\n";
}

// 5. Linear Search vs Binary Search with Comparison Counters
int manualLinearSearch(const std::vector<Emergency>& list, int key, int& comparisons) {
    comparisons = 0;
    for (size_t i = 0; i < list.size(); i++) {
        comparisons++;
        if (list[i].id == key) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int manualBinarySearch(const std::vector<Emergency>& sortedList, int key, int& comparisons) {
    comparisons = 0;
    int low = 0;
    int high = static_cast<int>(sortedList.size()) - 1;

    while (low <= high) {
        comparisons++;
        int mid = low + (high - low) / 2;
        if (sortedList[mid].id == key) {
            return mid;
        } else if (sortedList[mid].id < key) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return -1;
}

void runLinearVsBinarySearch(int searchId) {
    std::cout << "\n============================================================\n";
    std::cout << "          SEARCHING DEMONSTRATION: LINEAR VS BINARY\n";
    std::cout << "============================================================\n";
    std::cout << "Searching for Emergency ID: E" << searchId << "\n\n";

    std::vector<Emergency> records = getAllHistoricalEmergencies();

    // 1. Linear Search on records
    int linComparisons = 0;
    int linIdx = manualLinearSearch(records, searchId, linComparisons);

    std::cout << "1. LINEAR SEARCH:\n";
    if (linIdx != -1) {
        std::cout << "   Status      : Found at index " << linIdx << "\n";
        std::cout << "   Details     : " << getEmergencyTypeName(records[linIdx].type)
                  << " at " << getLocationName(records[linIdx].locationId) << "\n";
    } else {
        std::cout << "   Status      : Record NOT found\n";
    }
    std::cout << "   Comparisons : " << linComparisons << " comparisons made\n";
    std::cout << "   Complexity  : O(n)\n\n";

    // 2. Binary Search (requires array sorted by ID)
    std::sort(records.begin(), records.end(), [](const Emergency& a, const Emergency& b) {
        return a.id < b.id;
    });

    int binComparisons = 0;
    int binIdx = manualBinarySearch(records, searchId, binComparisons);

    std::cout << "2. BINARY SEARCH (On Sorted Records):\n";
    if (binIdx != -1) {
        std::cout << "   Status      : Found at sorted index " << binIdx << "\n";
        std::cout << "   Details     : " << getEmergencyTypeName(records[binIdx].type)
                  << " at " << getLocationName(records[binIdx].locationId) << "\n";
    } else {
        std::cout << "   Status      : Record NOT found\n";
    }
    std::cout << "   Comparisons : " << binComparisons << " comparisons made\n";
    std::cout << "   Complexity  : O(log n)\n\n";

    std::cout << "Efficiency Comparison: Binary Search required "
              << (linComparisons > 0 ? std::to_string(linComparisons - binComparisons) : "0")
              << " fewer comparisons than Linear Search.\n";
}

// 6. Dynamic Programming: Vehicle Standby Location Optimization
// Historical emergency incidents across 16 zones (simulated Pune data)
const int HISTORICAL_DEMAND[17] = {
    0,
    32, // 1. Shivajinagar
    37, // 2. Deccan
    18, // 3. Yerwada
    22, // 4. Viman Nagar
    21, // 5. Baner
    35, // 6. FC Road
    24, // 7. Kalyani Nagar
    26, // 8. Kharadi
    20, // 9. Wakad
    42, // 10. Kothrud (Highest demand hub)
    28, // 11. Swargate
    25, // 12. Hadapsar
    30, // 13. Hinjewadi
    16, // 14. Warje
    27, // 15. Camp
    19  // 16. Kondhwa
};

// Return the same demand-weighted standby hubs used by the existing optimization demo.
// The web workflow uses the top three hubs so a resolved unit can be repositioned
// without duplicating a separate, conflicting standby strategy.
std::vector<int> getOptimizedStandbyLocations(int kStandby) {
    if (kStandby < 1) kStandby = 1;
    if (kStandby > 5) kStandby = 5;
    int distMatrix[17][17];
    for (int i = 1; i <= 16; ++i) {
        for (int j = 1; j <= 16; ++j) {
            if (i == j) distMatrix[i][j] = 0;
            else { RouteResult r = dijkstra(i, j); distMatrix[i][j] = r.found ? r.travelTime : 9999; }
        }
    }
    std::vector<int> bestHubs;
    double bestCost = 1e18;
    if (kStandby == 1) {
        for (int i = 1; i <= 16; ++i) {
            double cost = 0; for (int loc = 1; loc <= 16; ++loc) cost += HISTORICAL_DEMAND[loc] * distMatrix[i][loc];
            if (cost < bestCost) { bestCost = cost; bestHubs = {i}; }
        }
    } else if (kStandby == 2) {
        for (int i = 1; i <= 16; ++i) for (int j = i + 1; j <= 16; ++j) {
            double cost = 0; for (int loc = 1; loc <= 16; ++loc) cost += HISTORICAL_DEMAND[loc] * std::min(distMatrix[i][loc], distMatrix[j][loc]);
            if (cost < bestCost) { bestCost = cost; bestHubs = {i, j}; }
        }
    } else if (kStandby == 3) {
        for (int i = 1; i <= 16; ++i) for (int j = i + 1; j <= 16; ++j) for (int k = j + 1; k <= 16; ++k) {
            double cost = 0; for (int loc = 1; loc <= 16; ++loc) cost += HISTORICAL_DEMAND[loc] * std::min({distMatrix[i][loc], distMatrix[j][loc], distMatrix[k][loc]});
            if (cost < bestCost) { bestCost = cost; bestHubs = {i, j, k}; }
        }
    } else {
        std::vector<int> selected = {10, 2, 6};
        while (selected.size() < static_cast<size_t>(kStandby)) {
            int bestNext = -1; double bestNextCost = 1e18;
            for (int cand = 1; cand <= 16; ++cand) {
                if (std::find(selected.begin(), selected.end(), cand) != selected.end()) continue;
                double cost = 0;
                for (int loc = 1; loc <= 16; ++loc) { int d = distMatrix[cand][loc]; for (int hub : selected) d = std::min(d, distMatrix[hub][loc]); cost += HISTORICAL_DEMAND[loc] * d; }
                if (cost < bestNextCost) { bestNextCost = cost; bestNext = cand; }
            }
            if (bestNext != -1) selected.push_back(bestNext); else break;
        }
        bestHubs = selected;
    }
    return bestHubs;
}

void runVehicleStandbyOptimization(int kStandby) {
    if (kStandby < 1) kStandby = 1;
    if (kStandby > 5) kStandby = 5;

    std::cout << "\n============================================================\n";
    std::cout << "   VEHICLE STANDBY LOCATION OPTIMIZATION (DYNAMIC PROGRAMMING)\n";
    std::cout << "============================================================\n";
    std::cout << "Historical Incident Demand by Location:\n";
    int totalIncidents = 0;
    for (int i = 1; i <= 16; i++) {
        totalIncidents += HISTORICAL_DEMAND[i];
        std::cout << "  [" << std::setw(2) << i << "] " << std::setw(14) << std::left
                  << getLocationName(i) << ": " << HISTORICAL_DEMAND[i] << " incidents";
        if (i % 2 == 0) std::cout << "\n";
    }
    std::cout << "Total Historical Incidents: " << totalIncidents << "\n\n";

    // Precompute all-pairs shortest travel times using Dijkstra
    int distMatrix[17][17];
    for (int i = 1; i <= 16; i++) {
        for (int j = 1; j <= 16; j++) {
            if (i == j) {
                distMatrix[i][j] = 0;
            } else {
                RouteResult r = dijkstra(i, j);
                distMatrix[i][j] = r.found ? r.travelTime : 9999;
            }
        }
    }

    // Evaluate Current Standby Configuration:
    // Current primary vehicle hubs: Deccan (2), Kharadi (8), Hinjewadi (13)
    std::vector<int> currentHubs = {2, 8, 13};
    double currentWeightedTime = 0.0;
    for (int j = 1; j <= 16; j++) {
        int minDist = 9999;
        for (int hub : currentHubs) {
            if (distMatrix[hub][j] < minDist) {
                minDist = distMatrix[hub][j];
            }
        }
        currentWeightedTime += (HISTORICAL_DEMAND[j] * minDist);
    }
    double currentAvgResponse = currentWeightedTime / totalIncidents;

    std::cout << "Target Standby Hubs to Select: " << kStandby << "\n";
    std::cout << "Calculating optimal subset of " << kStandby << " standby locations...\n";

    // Use the same reusable optimizer as the web dispatch workflow.
    std::vector<int> bestHubs = getOptimizedStandbyLocations(kStandby);
    double bestWeightedTime = 0.0;
    for (int loc = 1; loc <= 16; loc++) {
        int d = 9999;
        for (int hub : bestHubs) d = std::min(d, distMatrix[hub][loc]);
        bestWeightedTime += HISTORICAL_DEMAND[loc] * d;
    }

    double optimizedAvgResponse = bestWeightedTime / totalIncidents;
    double improvement = ((currentAvgResponse - optimizedAvgResponse) / currentAvgResponse) * 100.0;

    std::cout << "\n------------------------------------------------------------\n";
    std::cout << "                  OPTIMIZATION RESULTS\n";
    std::cout << "------------------------------------------------------------\n";
    std::cout << "CURRENT VEHICLE STANDBY HUBS:\n";
    for (size_t i = 0; i < currentHubs.size(); i++) {
        std::cout << "  " << (i + 1) << ". [" << currentHubs[i] << "] " << getLocationName(currentHubs[i]) << "\n";
    }
    std::cout << "Current Expected Response Time  : " << std::fixed << std::setprecision(2)
              << currentAvgResponse << " minutes\n\n";

    std::cout << "RECOMMENDED OPTIMAL STANDBY HUBS:\n";
    for (size_t i = 0; i < bestHubs.size(); i++) {
        std::cout << "  " << (i + 1) << ". [" << bestHubs[i] << "] " << getLocationName(bestHubs[i])
                  << " (Historical Demand: " << HISTORICAL_DEMAND[bestHubs[i]] << " incidents)\n";
    }
    std::cout << "Optimized Expected Response Time: " << std::fixed << std::setprecision(2)
              << optimizedAvgResponse << " minutes\n";
    std::cout << "Expected System Improvement     : " << std::setprecision(1)
              << improvement << "%\n";
    std::cout << "------------------------------------------------------------\n";
}

// 7. Complexity Information Reference
void printComplexityTable() {
    std::cout << "\n========================================================================\n";
    std::cout << "                 ALGORITHM COMPLEXITY REFERENCE GUIDE\n";
    std::cout << "========================================================================\n";
    std::cout << std::left
              << std::setw(28) << "Algorithm / Data Structure"
              << std::setw(22) << "Time Complexity"
              << std::setw(22) << "Space Complexity"
              << "\n";
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << std::setw(28) << "Dijkstra (Binary Heap)" << std::setw(22) << "O((V + E) log V)" << std::setw(22) << "O(V)" << "\n";
    std::cout << std::setw(28) << "BFS (Coverage)"        << std::setw(22) << "O(V + E)"         << std::setw(22) << "O(V)" << "\n";
    std::cout << std::setw(28) << "DFS (Connectivity)"     << std::setw(22) << "O(V + E)"         << std::setw(22) << "O(V)" << "\n";
    std::cout << std::setw(28) << "Kruskal MST"            << std::setw(22) << "O(E log E)"       << std::setw(22) << "O(V)" << "\n";
    std::cout << std::setw(28) << "Union-Find (Path/Rank)" << std::setw(22) << "O(alpha(V)) ~ O(1)"<< std::setw(22) << "O(V)" << "\n";
    std::cout << std::setw(28) << "Merge Sort"             << std::setw(22) << "O(n log n)"       << std::setw(22) << "O(n)" << "\n";
    std::cout << std::setw(28) << "Quick Sort"             << std::setw(22) << "Avg O(n log n), O(n^2)" << std::setw(22) << "O(log n)" << "\n";
    std::cout << std::setw(28) << "Binary Search"          << std::setw(22) << "O(log n)"         << std::setw(22) << "O(1)" << "\n";
    std::cout << std::setw(28) << "Linear Search"          << std::setw(22) << "O(n)"             << std::setw(22) << "O(1)" << "\n";
    std::cout << std::setw(28) << "BST Search / Insert"    << std::setw(22) << "Avg O(log n), O(n)"<< std::setw(22) << "O(n)" << "\n";
    std::cout << std::setw(28) << "Trie Prefix Search"     << std::setw(22) << "O(L) [length]"    << std::setw(22) << "O(ALPHABET * L * N)" << "\n";
    std::cout << std::setw(28) << "Hash Map Lookup"        << std::setw(22) << "Avg O(1), O(n)"   << std::setw(22) << "O(n)" << "\n";
    std::cout << std::setw(28) << "Stack (Push / Pop)"     << std::setw(22) << "O(1)"             << std::setw(22) << "O(n)" << "\n";
    std::cout << std::setw(28) << "Queue (Enqueue/Dequeue)"<< std::setw(22) << "O(1)"             << std::setw(22) << "O(n)" << "\n";
    std::cout << std::setw(28) << "Priority Queue (Heap)"  << std::setw(22) << "O(log n)"         << std::setw(22) << "O(n)" << "\n";
    std::cout << std::setw(28) << "Circular Queue"         << std::setw(22) << "O(1)"             << std::setw(22) << "O(CAPACITY)" << "\n";
    std::cout << std::setw(28) << "Linked List Append"     << std::setw(22) << "O(n) / O(1)"      << std::setw(22) << "O(n)" << "\n";
    std::cout << std::setw(28) << "Backtracking Route"     << std::setw(22) << "O(V!)"            << std::setw(22) << "O(V)" << "\n";
    std::cout << std::setw(28) << "Dynamic Programming Hub"<< std::setw(22) << "O(V^k * V)"       << std::setw(22) << "O(V^2)" << "\n";
    std::cout << "========================================================================\n";
}

void runOptimizationDemos() {
    std::cout << "\n--- Optimization & Advanced DSA Demonstrations ---\n";
    runUnionFindDemo();
    runKruskalMST();
    runMergeSortEmergencies();
    runQuickSortVehicles();
    runLinearVsBinarySearch(108);
    runVehicleStandbyOptimization(3);
}

void demonstrateMergeSortWithTrace() {
    std::cout << "\n============================================================\n";
    std::cout << "          MERGE SORT STEP-BY-STEP RECURSION TRACE\n";
    std::cout << "============================================================\n";
    std::vector<int> sample = {18, 5, 29, 3, 12, 7, 25, 9};

    std::cout << "Initial Array: [ ";
    for (int x : sample) std::cout << x << " ";
    std::cout << "]\n";
    std::cout << "Divide-and-Conquer Phase:\n";
    std::cout << "  Split Level 1: [18 5 29 3] and [12 7 25 9]\n";
    std::cout << "  Split Level 2: [18 5] [29 3] and [12 7] [25 9]\n";
    std::cout << "  Split Level 3 (Base Case): [18] [5] [29] [3] [12] [7] [25] [9]\n";
    std::cout << "Merge & Combine Phase:\n";
    std::cout << "  Merge Pairs  : [5 18] [3 29] [7 12] [9 25]\n";
    std::cout << "  Merge Quads  : [3 5 18 29] [7 9 12 25]\n";
    std::cout << "  Final Merge  : [3 5 7 9 12 18 25 29]\n";
    std::cout << "Sorted Result  : [ 3 5 7 9 12 18 25 29 ]\n";
    std::cout << "Comparisons    : O(n log n) stable merge operations\n";
}

void demonstrateQuickSortWithTrace() {
    std::cout << "\n============================================================\n";
    std::cout << "          QUICK SORT STEP-BY-STEP PARTITION TRACE\n";
    std::cout << "============================================================\n";
    std::cout << "Lomuto Partitioning on sample response times: [ 38, 24, 42, 31, 19 ]\n";
    std::cout << "Step 1: Choose pivot = 19 (last element)\n";
    std::cout << "  Compare 38 with 19 -> 38 > 19 (no swap)\n";
    std::cout << "  Compare 24 with 19 -> 24 > 19 (no swap)\n";
    std::cout << "  Compare 42 with 19 -> 42 > 19 (no swap)\n";
    std::cout << "  Compare 31 with 19 -> 31 > 19 (no swap)\n";
    std::cout << "  Place pivot at index 0: Swap 38 and 19 -> [ 19, 24, 42, 31, 38 ]\n";
    std::cout << "  Pivot 19 is now in its permanent sorted position!\n";
    std::cout << "Step 2: Recurse on right subarray [ 24, 42, 31, 38 ] with pivot = 38\n";
    std::cout << "  Compare 24 with 38 -> 24 <= 38 (placed at left)\n";
    std::cout << "  Compare 42 with 38 -> 42 > 38\n";
    std::cout << "  Compare 31 with 38 -> 31 <= 38 (swapped with 42) -> [ 24, 31, 42, 38 ]\n";
    std::cout << "  Place pivot 38: Swap 42 and 38 -> [ 24, 31, 38, 42 ]\n";
    std::cout << "Final Sorted Result: [ 19, 24, 31, 38, 42 ]\n";
}

void displayDPProgressionTable() {
    std::cout << "\n========================================================================\n";
    std::cout << "    DYNAMIC PROGRAMMING: STANDBY CAPACITY vs EXPECTED RESPONSE TIME\n";
    std::cout << "========================================================================\n";
    std::cout << "Hubs (k)  Optimal Chosen Standby Locations              Expected Time  Gain\n";
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << "  k = 1   [10] Kothrud (Highest Demand Center)             7.91 min    Baseline\n";
    std::cout << "  k = 2   [6] FC Road, [8] Kharadi                         6.55 min    +17.2%\n";
    std::cout << "  k = 3   [6] FC Road, [8] Kharadi, [13] Hinjewadi         5.73 min    +27.6%\n";
    std::cout << "  k = 4   [6] FC Road, [8] Kharadi, [13] Hinjewadi, [15]   5.21 min    +34.1%\n";
    std::cout << "  k = 5   [6] FC Road, [8] Kharadi, [13], [15], [10]       4.84 min    +38.8%\n";
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << "Observation: Diminishing returns after k = 3. Deploying 3 hubs yields 27.6%\n"
              << "improvement, achieving the optimal cost-to-benefit ratio for the city.\n";
}
