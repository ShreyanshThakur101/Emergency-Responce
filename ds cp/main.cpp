#include <iostream>
#include <vector>
#include <string>
#include <limits>
#include <iomanip>

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

struct RouteResult {
    int travelTime;
    double distance;
    std::vector<int> path;
    bool found;
};

// Functions from graph_algorithms.cpp
void initGraph();
std::string getLocationName(int id);
int getLocationId(const std::string& name);
int getTotalLocations();
RouteResult dijkstra(int src, int dest);
void blockRoad(int u, int v);
void unblockRoad(int u, int v);
bool isRoadBlocked(int u, int v);
void printBfsCoverage(int startId, int maxHops);
int getConnectedComponentsCount();
void printConnectivityReport();
void printAlternativeRoutes(int src, int dest);
void displayCityRoads();
void runGraphDemos();
void printAllPairsShortestPaths();
void printLocationNeighbors(int locId);
void printGraphTopologyStats();
void dijkstraWithTrace(int src, int dest);

// Functions from emergency_core.cpp
void initEmergencySystem();
bool reportEmergency(int locId, EmergencyType type, Severity sev, const std::string& desc, int& outId);
bool processNextEmergency();
bool completeActiveEmergency(int emergencyId);
bool cancelEmergency(int emergencyId);
int getAvailableVehiclesCount();
int getBusyVehiclesCount();
int getPendingEmergenciesCount();
int getCompletedEmergenciesCount();
void printDashboardStatus();
void printEmergencyQueue();
void printAllVehicles();
void printVehicleHistory(const std::string& vehicleId);
void printRecentEvents();
void searchLocationTrie(const std::string& prefix);
void searchEmergencyBST(int eId);
void deleteEmergencyBSTRecord(int eId);
void printBSTTraversals();
void runEmergencyCoreDemos();
void demonstrateQueueVsPriorityQueue();
bool undoLastDispatch();
std::string getVehicleTypeName(VehicleType t);
std::string getEmergencyTypeName(EmergencyType t);
std::string getSeverityName(Severity s);
std::string getStatusName(EmergencyStatus st);
std::vector<std::string> getVehiclesAtLocation(int locId);

// Functions from emergency_system.cpp
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

namespace {
    // Input validation helpers
    int getValidInt(int minVal, int maxVal, const std::string& promptMsg) {
        int val;
        while (true) {
            std::cout << promptMsg;
            if (std::cin >> val) {
                if (val >= minVal && val <= maxVal) {
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    return val;
                }
            }
            std::cout << "ERROR: Invalid input. Please enter an integer between "
                      << minVal << " and " << maxVal << ".\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }

    std::string getValidString(const std::string& promptMsg) {
        std::string s;
        while (true) {
            std::cout << promptMsg;
            std::getline(std::cin, s);
            if (!s.empty()) {
                return s;
            }
            std::cout << "Input cannot be empty. Please try again.\n";
        }
    }

    void displayCityMap() {
        std::cout << "\n                      CITY MAP\n";
        std::cout << "------------------------------------------------------------\n\n";
        std::cout << "[1] Shivajinagar ─── [2] Deccan ─── [3] Yerwada ─── [4] Viman Nagar\n";
        std::cout << "       │                   │              │                 │\n";
        std::cout << "       │                   │              │                 │\n";
        std::cout << "[5] Baner ─────────── [6] FC Road ── [7] Kalyani ──── [8] Kharadi\n";
        std::cout << "       │                   │              │                 │\n";
        std::cout << "       │                   │              │                 │\n";
        std::cout << "[9] Wakad ────────── [10] Kothrud ── [11] Swargate ─ [12] Hadapsar\n";
        std::cout << "       │                   │              │                 │\n";
        std::cout << "       │                   │              │                 │\n";
        std::cout << "[13] Hinjewadi ───── [14] Warje ─── [15] Camp ────── [16] Kondhwa\n\n";
        std::cout << "------------------------------------------------------------\n";
        std::cout << "Live Vehicle Station Deployment:\n";
        for (int i = 1; i <= 16; i++) {
            auto vList = getVehiclesAtLocation(i);
            if (!vList.empty()) {
                std::cout << "  [" << i << "] " << std::setw(14) << std::left << getLocationName(i) << " : ";
                for (size_t j = 0; j < vList.size(); j++) {
                    std::cout << "[" << vList[j] << "] ";
                }
                std::cout << "\n";
            }
        }
        std::cout << "------------------------------------------------------------\n";
        std::cout << "(Note: Simulated Pune emergency response network)\n";
    }

    void displayDashboard() {
        std::cout << "\n============================================================\n";
        std::cout << "         CITY-WIDE EMERGENCY RESPONSE SYSTEM\n";
        std::cout << "============================================================\n\n";
        std::cout << "                     CITY STATUS\n";
        std::cout << "------------------------------------------------------------\n";
        printDashboardStatus();
        std::cout << "------------------------------------------------------------\n\n";
        std::cout << "1. Report Emergency\n";
        std::cout << "2. View City Map\n";
        std::cout << "3. View Vehicles\n";
        std::cout << "4. View Emergency Queue\n";
        std::cout << "5. Process Next Emergency\n";
        std::cout << "6. Complete Active Emergency\n";
        std::cout << "7. Search Location\n";
        std::cout << "8. Search Emergency\n";
        std::cout << "9. View Vehicle History\n";
        std::cout << "10. Find Shortest Route\n";
        std::cout << "11. Find Alternative Route\n";
        std::cout << "12. Analyze City Connectivity\n";
        std::cout << "13. Nearby Location Analysis\n";
        std::cout << "14. Optimize Vehicle Positions\n";
        std::cout << "15. Emergency Infrastructure Planning\n";
        std::cout << "16. Sorting & Searching Demonstration\n";
        std::cout << "17. Algorithm Demonstration\n";
        std::cout << "18. Run Emergency Simulation\n";
        std::cout << "19. Exit\n\n";
    }

    void handleReportEmergency() {
        std::cout << "\n============================================================\n";
        std::cout << "                  REPORT NEW EMERGENCY\n";
        std::cout << "============================================================\n";
        std::cout << "Locations:\n";
        for (int i = 1; i <= 16; i++) {
            std::cout << "  [" << i << "] " << std::setw(14) << std::left << getLocationName(i);
            if (i % 2 == 0) std::cout << "\n";
        }
        int locId = getValidInt(1, 16, "\nEnter incident location (1-16): ");

        std::cout << "\nEmergency Types:\n";
        std::cout << "  1. Medical\n  2. Fire\n  3. Crime\n  4. Accident\n  5. Disaster\n";
        int typeChoice = getValidInt(1, 5, "Select emergency type (1-5): ");
        EmergencyType eType = static_cast<EmergencyType>(typeChoice - 1);

        std::cout << "\nSeverity Levels:\n";
        std::cout << "  1. LOW\n  2. MEDIUM\n  3. HIGH\n  4. CRITICAL\n";
        int sevChoice = getValidInt(1, 4, "Select severity level (1-4): ");
        Severity sev = static_cast<Severity>(sevChoice);

        std::string desc = getValidString("Enter brief incident description: ");

        int eId = 0;
        if (reportEmergency(locId, eType, sev, desc, eId)) {
            std::cout << "\nSUCCESS: Emergency E" << eId << " reported and enqueued!\n";
            std::cout << "Priority queue will dispatch vehicle based on severity ("
                      << getSeverityName(sev) << ").\n";
        }
    }

    void handleShortestRoute() {
        std::cout << "\n============================================================\n";
        std::cout << "            FIND SHORTEST ROUTE (DIJKSTRA)\n";
        std::cout << "============================================================\n";
        int src = getValidInt(1, 16, "Enter origin location ID (1-16): ");
        int dest = getValidInt(1, 16, "Enter destination location ID (1-16): ");

        RouteResult r = dijkstra(src, dest);
        if (r.found) {
            std::cout << "\nShortest Route\n";
            std::cout << "------------------------------------------------\n";
            std::cout << "From: " << getLocationName(src) << "\n";
            std::cout << "To  : " << getLocationName(dest) << "\n\n";
            std::cout << "Route:\n\n";
            for (size_t i = 0; i < r.path.size(); i++) {
                std::cout << getLocationName(r.path[i]) << "\n";
                if (i + 1 < r.path.size()) {
                    std::cout << "  ↓\n";
                }
            }
            std::cout << "\nEstimated Travel Time: " << r.travelTime << " minutes\n";
            std::cout << "Estimated Distance   : " << std::fixed << std::setprecision(1)
                      << r.distance << " km\n";
        } else {
            std::cout << "\nERROR: No open route exists between "
                      << getLocationName(src) << " and " << getLocationName(dest) << "!\n";
        }
    }

    void handleRoadControl() {
        std::cout << "\nRoad Block / Unblock Management:\n";
        std::cout << "  1. Block a Road\n  2. Unblock a Road\n  3. View Road Network\n";
        int choice = getValidInt(1, 3, "Select option (1-3): ");
        if (choice == 1) {
            int u = getValidInt(1, 16, "Enter first location ID (1-16): ");
            int v = getValidInt(1, 16, "Enter second location ID (1-16): ");
            blockRoad(u, v);
            std::cout << "Road between " << getLocationName(u) << " and " << getLocationName(v) << " is now BLOCKED.\n";
        } else if (choice == 2) {
            int u = getValidInt(1, 16, "Enter first location ID (1-16): ");
            int v = getValidInt(1, 16, "Enter second location ID (1-16): ");
            unblockRoad(u, v);
            std::cout << "Road between " << getLocationName(u) << " and " << getLocationName(v) << " is now OPEN.\n";
        } else {
            displayCityRoads();
        }
    }

    void handleAlternativeRoute() {
        std::cout << "\n============================================================\n";
        std::cout << "          ALTERNATIVE ROUTES & ROAD MANAGEMENT\n";
        std::cout << "============================================================\n";
        std::cout << "1. Find Alternative Route (Backtracking)\n";
        std::cout << "2. Block or Unblock a City Road\n";
        std::cout << "3. View All City Road Connections\n";
        int choice = getValidInt(1, 3, "Select option (1-3): ");

        if (choice == 1) {
            int src = getValidInt(1, 16, "Enter origin location ID (1-16): ");
            int dest = getValidInt(1, 16, "Enter destination location ID (1-16): ");
            printAlternativeRoutes(src, dest);
        } else if (choice == 2) {
            handleRoadControl();
        } else {
            displayCityRoads();
        }
    }

    void handleBfsNearby() {
        int src = getValidInt(1, 16, "Enter center location ID (1-16): ");
        int hops = getValidInt(1, 5, "Enter maximum hops (1-5): ");
        printBfsCoverage(src, hops);
    }

    void runAlgorithmDemosMenu() {
        while (true) {
            std::cout << "\n============================================================\n";
            std::cout << "              ALGORITHM DEMONSTRATION MENU\n";
            std::cout << "============================================================\n";
            std::cout << "1.  Dijkstra Algorithm\n";
            std::cout << "2.  Breadth-First Search (BFS)\n";
            std::cout << "3.  Depth-First Search (DFS)\n";
            std::cout << "4.  Merge Sort\n";
            std::cout << "5.  Quick Sort\n";
            std::cout << "6.  Linear Search\n";
            std::cout << "7.  Binary Search\n";
            std::cout << "8.  Binary Search Tree (BST)\n";
            std::cout << "9.  Trie (Location Autocomplete)\n";
            std::cout << "10. Kruskal Minimum Spanning Tree\n";
            std::cout << "11. Union-Find (Disjoint Set)\n";
            std::cout << "12. Backtracking (Alternative Routes)\n";
            std::cout << "13. Dynamic Programming (Standby Hubs)\n";
            std::cout << "14. FIFO Normal Queue\n";
            std::cout << "15. Priority Queue (Severity First)\n";
            std::cout << "16. Circular Queue (Event Log)\n";
            std::cout << "17. Route Reconstruction Stack\n";
            std::cout << "18. Singly Linked List (Vehicle History)\n";
            std::cout << "19. Display Algorithm Complexities\n";
            std::cout << "20. Return to Main Menu\n\n";

            int choice = getValidInt(1, 20, "Enter demonstration choice (1-20): ");
            if (choice == 20) break;

            switch (choice) {
                case 1: {
                    std::cout << "\n[Demo: Dijkstra Shortest Path]\n";
                    RouteResult r = dijkstra(2, 10);
                    std::cout << "Shortest path from Deccan to Kothrud:\n";
                    std::cout << "Travel Time: " << r.travelTime << " min, Distance: " << r.distance << " km\nPath: ";
                    for (size_t i = 0; i < r.path.size(); i++) {
                        std::cout << getLocationName(r.path[i]);
                        if (i + 1 < r.path.size()) std::cout << " -> ";
                    }
                    std::cout << "\n";
                    dijkstraWithTrace(2, 10);
                    break;
                }
                case 2:
                    std::cout << "\n[Demo: BFS Coverage]\n";
                    printBfsCoverage(1, 2);
                    break;
                case 3:
                    std::cout << "\n[Demo: DFS Connectivity & Topology]\n";
                    printConnectivityReport();
                    printGraphTopologyStats();
                    break;
                case 4:
                    std::cout << "\n[Demo: Merge Sort]\n";
                    runMergeSortEmergencies();
                    demonstrateMergeSortWithTrace();
                    break;
                case 5:
                    std::cout << "\n[Demo: Quick Sort]\n";
                    runQuickSortVehicles();
                    demonstrateQuickSortWithTrace();
                    break;
                case 6:
                case 7:
                    std::cout << "\n[Demo: Searching]\n";
                    runLinearVsBinarySearch(105);
                    break;
                case 8:
                    std::cout << "\n[Demo: BST Traversals & Operations]\n";
                    printBSTTraversals();
                    searchEmergencyBST(101);
                    break;
                case 9:
                    std::cout << "\n[Demo: Trie Autocomplete]\n";
                    searchLocationTrie("W");
                    searchLocationTrie("Ko");
                    break;
                case 10:
                    std::cout << "\n[Demo: Kruskal MST]\n";
                    runKruskalMST();
                    break;
                case 11:
                    std::cout << "\n[Demo: Union-Find]\n";
                    runUnionFindDemo();
                    break;
                case 12:
                    std::cout << "\n[Demo: Backtracking Alternative Routes]\n";
                    printAlternativeRoutes(1, 10);
                    break;
                case 13:
                    std::cout << "\n[Demo: Dynamic Programming]\n";
                    runVehicleStandbyOptimization(3);
                    displayDPProgressionTable();
                    break;
                case 14:
                    demonstrateQueueVsPriorityQueue();
                    break;
                case 15:
                    std::cout << "\n[Demo: Priority Queue (Current Emergency Queue)]\n";
                    printEmergencyQueue();
                    break;
                case 16:
                    std::cout << "\n[Demo: Circular Queue]\n";
                    printRecentEvents();
                    break;
                case 17: {
                    std::cout << "\n[Demo: Stack Data Structure]\n";
                    std::cout << "1. Route Reconstruction Stack (Hinjewadi to Viman Nagar):\n";
                    RouteResult r = dijkstra(13, 4);
                    for (size_t i = 0; i < r.path.size(); i++) {
                        std::cout << "  Step " << (i + 1) << ": [" << r.path[i] << "] "
                                  << getLocationName(r.path[i]) << "\n";
                    }
                    std::cout << "\n2. Dispatch History & Undo Stack:\n";
                    undoLastDispatch();
                    break;
                }
                case 18:
                    std::cout << "\n[Demo: Singly Linked List History]\n";
                    printVehicleHistory("A02");
                    break;
                case 19:
                    printComplexityTable();
                    break;
            }
        }
    }

    void runEmergencySimulation() {
        std::cout << "\n============================================================\n";
        std::cout << "                  EMERGENCY SIMULATION\n";
        std::cout << "============================================================\n\n";
        std::cout << "Generating emergency requests...\n\n";

        int id1, id2, id3, id4;
        reportEmergency(10, EmergencyType::MEDICAL,  Severity::CRITICAL, "Cardiac distress at Kothrud", id1);
        reportEmergency(8,  EmergencyType::FIRE,     Severity::HIGH,     "Factory fire at Kharadi", id2);
        reportEmergency(5,  EmergencyType::CRIME,    Severity::MEDIUM,   "Break-in attempt at Baner", id3);
        reportEmergency(9,  EmergencyType::ACCIDENT, Severity::CRITICAL, "Highway truck pileup at Wakad", id4);

        std::cout << "E" << id1 << " | MEDICAL   | Kothrud | CRITICAL\n";
        std::cout << "E" << id2 << " | FIRE      | Kharadi | HIGH\n";
        std::cout << "E" << id3 << " | CRIME     | Baner   | MEDIUM\n";
        std::cout << "E" << id4 << " | ACCIDENT  | Wakad   | CRITICAL\n\n";

        std::cout << "Processing priority queue (Critical First)...\n\n";
        printEmergencyQueue();

        std::cout << "\nExecuting automated dispatch for all emergencies in queue:\n";
        while (getPendingEmergenciesCount() > 0) {
            processNextEmergency();
        }

        std::cout << "\nSimulating emergency resolution and vehicle returns:\n";
        completeActiveEmergency(id1);
        completeActiveEmergency(id4);
        completeActiveEmergency(id2);
        completeActiveEmergency(id3);

        std::cout << "\nSimulation completed. All vehicles repositioned and available.\n";
    }
}

int main() {
    // Initialize all modules
    initGraph();
    initEmergencySystem();

    std::cout << "Initialization complete. City graph loaded with 16 locations and 28 roads.\n";
    std::cout << "Emergency fleet active with 8 units across the city.\n";

    bool running = true;
    while (running) {
        displayDashboard();
        int choice = getValidInt(1, 19, "Enter choice (1-19): ");

        switch (choice) {
            case 1:
                handleReportEmergency();
                break;
            case 2:
                displayCityMap();
                break;
            case 3:
                printAllVehicles();
                break;
            case 4:
                printEmergencyQueue();
                break;
            case 5: {
                if (processNextEmergency()) {
                    std::cout << "\nDispatch Actions:\n";
                    std::cout << "  1. Keep Dispatch Active\n  2. Undo Dispatch (Custom Stack)\n";
                    int uChoice = getValidInt(1, 2, "Select action (1-2): ");
                    if (uChoice == 2) {
                        undoLastDispatch();
                    }
                }
                break;
            }
            case 6: {
                int eId = getValidInt(1, 9999, "Enter Emergency ID to mark complete: ");
                completeActiveEmergency(eId);
                break;
            }
            case 7: {
                std::string locName = getValidString("Enter location name or prefix (e.g., Kothrud, Kot): ");
                searchLocationTrie(locName);
                int locId = getLocationId(locName);
                if (locId != -1) {
                    std::cout << "\nExact Match Found -> Location ID: " << locId << "\n";
                    printLocationNeighbors(locId);
                }
                break;
            }
            case 8: {
                int eId = getValidInt(1, 9999, "Enter Emergency ID to search: ");
                searchEmergencyBST(eId);
                break;
            }
            case 9: {
                std::string vId = getValidString("Enter Vehicle ID (e.g., A01, A02, F01, P01): ");
                printVehicleHistory(vId);
                break;
            }
            case 10:
                handleShortestRoute();
                break;
            case 11:
                handleAlternativeRoute();
                break;
            case 12: {
                std::cout << "\nCity Connectivity & Network Analysis Options:\n";
                std::cout << "  1. Run DFS City Connectivity Check\n";
                std::cout << "  2. View City Topology Metrics & Network Diameter\n";
                std::cout << "  3. View 16x16 All-Pairs Travel Time Grid\n";
                int cChoice = getValidInt(1, 3, "Select analysis option (1-3): ");
                if (cChoice == 1) printConnectivityReport();
                else if (cChoice == 2) printGraphTopologyStats();
                else printAllPairsShortestPaths();
                break;
            }
            case 13:
                handleBfsNearby();
                break;
            case 14: {
                int k = getValidInt(1, 5, "Enter number of standby positions to optimize (1-5): ");
                runVehicleStandbyOptimization(k);
                break;
            }
            case 15:
                runKruskalMST();
                break;
            case 16: {
                std::cout << "\nSorting and Searching Options:\n";
                std::cout << "  1. Merge Sort (Emergencies by Response Time)\n";
                std::cout << "  2. Quick Sort (Vehicles by Response Time)\n";
                std::cout << "  3. Linear vs Binary Search\n";
                int sub = getValidInt(1, 3, "Select option (1-3): ");
                if (sub == 1) runMergeSortEmergencies();
                else if (sub == 2) runQuickSortVehicles();
                else {
                    int sId = getValidInt(1, 999, "Enter Emergency ID to search: ");
                    runLinearVsBinarySearch(sId);
                }
                break;
            }
            case 17:
                runAlgorithmDemosMenu();
                break;
            case 18:
                runEmergencySimulation();
                break;
            case 19:
                std::cout << "\nShutting down Emergency Response System. Goodbye!\n";
                running = false;
                break;
        }

        if (running && choice != 17) {
            std::cout << "\nPress Enter to return to main menu...";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }

    return 0;
}
