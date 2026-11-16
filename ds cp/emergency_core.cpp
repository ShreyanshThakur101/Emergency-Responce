#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>

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

// Read-only web/API view of the existing C++ fleet.
struct WebVehicleInfo {
    std::string id;
    std::string type;
    int locationId;
    bool isAvailable;
    int currentEmergencyId; // -1 = none, -2 = returning to standby
    int totalEmergencies;
    int totalResponseTime;
    double totalDistance;
};

struct RouteResult {
    int travelTime;
    double distance;
    std::vector<int> path;
    bool found;
};

// External declarations from graph_algorithms.cpp
RouteResult dijkstra(int src, int dest);
std::string getLocationName(int id);
int getTotalLocations();

// Functions exported for main and other modules
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

std::vector<Emergency> getAllHistoricalEmergencies();
int getVehicleLocation(const std::string& vehicleId);
std::vector<std::string> getVehiclesAtLocation(int locId);
std::vector<WebVehicleInfo> getWebVehicleInfos();
bool webSetVehicleReturningToStandby(const std::string& vehicleId, int standbyLocationId);
bool webMarkStandbyArrived(const std::string& vehicleId, int standbyLocationId);
bool webReleaseVehicle(const std::string& vehicleId, int emergencyId);
bool webAssignVehicle(const std::string& vehicleId, int emergencyId, int destLocId, int& outTravelTime);

// ============================================================
// INTERNAL IMPLEMENTATION
// ============================================================

// 1. Custom Singly Linked List for Vehicle History
struct HistoryNode {
    int emergencyId;
    EmergencyType type;
    int responseTime;
    int destination;
    HistoryNode* next;

    HistoryNode(int eId, EmergencyType t, int rTime, int dest) {
        emergencyId = eId;
        type = t;
        responseTime = rTime;
        destination = dest;
        next = nullptr;
    }
};

class HistoryLinkedList {
private:
    HistoryNode* head;
public:
    HistoryLinkedList() {
        head = nullptr;
    }

    ~HistoryLinkedList() {
        HistoryNode* curr = head;
        while (curr != nullptr) {
            HistoryNode* temp = curr->next;
            delete curr;
            curr = temp;
        }
    }

    void addRecord(int eId, EmergencyType t, int rTime, int dest) {
        HistoryNode* newNode = new HistoryNode(eId, t, rTime, dest);
        if (head == nullptr) {
            head = newNode;
        } else {
            HistoryNode* curr = head;
            while (curr->next != nullptr) {
                curr = curr->next;
            }
            curr->next = newNode;
        }
    }

    void display() const {
        if (head == nullptr) {
            std::cout << "  No history recorded (None) -> NULL\n";
            return;
        }
        std::cout << "  ";
        HistoryNode* curr = head;
        while (curr != nullptr) {
            std::cout << "[E" << curr->emergencyId << " | " << getEmergencyTypeName(curr->type)
                      << " | " << curr->responseTime << "m | " << getLocationName(curr->destination) << "] -> ";
            curr = curr->next;
        }
        std::cout << "NULL\n";
    }

    bool isEmpty() const {
        return head == nullptr;
    }
};

// 2. Vehicle Representation
struct Vehicle {
    std::string id;
    VehicleType type;
    int currentLocation;
    bool isAvailable;
    int totalEmergencies;
    double totalDistance;
    int totalResponseTime;
    int currentEmergencyId;
    HistoryLinkedList history;

    Vehicle() {
        id = "";
        type = VehicleType::AMBULANCE;
        currentLocation = 1;
        isAvailable = true;
        totalEmergencies = 0;
        totalDistance = 0.0;
        totalResponseTime = 0;
        currentEmergencyId = -1;
    }

    Vehicle(std::string vId, VehicleType vType, int loc) {
        id = vId;
        type = vType;
        currentLocation = loc;
        isAvailable = true;
        totalEmergencies = 0;
        totalDistance = 0.0;
        totalResponseTime = 0;
        currentEmergencyId = -1;
    }
};

// 3. Custom FIFO Normal Queue for Incoming Emergencies
struct QueueNode {
    Emergency data;
    QueueNode* next;

    QueueNode(const Emergency& e) {
        data = e;
        next = nullptr;
    }
};

class EmergencyQueue {
private:
    QueueNode* frontNode;
    QueueNode* rearNode;
    int count;
public:
    EmergencyQueue() {
        frontNode = nullptr;
        rearNode = nullptr;
        count = 0;
    }

    ~EmergencyQueue() {
        while (!isEmpty()) {
            dequeue();
        }
    }

    void enqueue(const Emergency& e) {
        QueueNode* newNode = new QueueNode(e);
        if (rearNode == nullptr) {
            frontNode = rearNode = newNode;
        } else {
            rearNode->next = newNode;
            rearNode = newNode;
        }
        count++;
    }

    Emergency dequeue() {
        if (isEmpty()) {
            Emergency emptyE;
            emptyE.id = -1;
            return emptyE;
        }
        QueueNode* temp = frontNode;
        Emergency res = temp->data;
        frontNode = frontNode->next;
        if (frontNode == nullptr) {
            rearNode = nullptr;
        }
        delete temp;
        count--;
        return res;
    }

    Emergency front() const {
        if (isEmpty()) {
            Emergency emptyE;
            emptyE.id = -1;
            return emptyE;
        }
        return frontNode->data;
    }

    bool isEmpty() const {
        return frontNode == nullptr;
    }

    int size() const {
        return count;
    }
};

// 4. Custom Priority Queue based on Severity and Timestamp/ID
class EmergencyPriorityQueue {
private:
    Emergency heap[128];
    int count;

    bool hasHigherPriority(const Emergency& a, const Emergency& b) const {
        if (static_cast<int>(a.severity) != static_cast<int>(b.severity)) {
            return static_cast<int>(a.severity) > static_cast<int>(b.severity);
        }
        // Older emergency first (lower id)
        return a.id < b.id;
    }

    void heapifyUp(int index) {
        while (index > 0) {
            int parent = (index - 1) / 2;
            if (hasHigherPriority(heap[index], heap[parent])) {
                std::swap(heap[index], heap[parent]);
                index = parent;
            } else {
                break;
            }
        }
    }

    void heapifyDown(int index) {
        while (true) {
            int left = 2 * index + 1;
            int right = 2 * index + 2;
            int highest = index;

            if (left < count && hasHigherPriority(heap[left], heap[highest])) {
                highest = left;
            }
            if (right < count && hasHigherPriority(heap[right], heap[highest])) {
                highest = right;
            }

            if (highest != index) {
                std::swap(heap[index], heap[highest]);
                index = highest;
            } else {
                break;
            }
        }
    }

public:
    EmergencyPriorityQueue() {
        count = 0;
    }

    void push(const Emergency& e) {
        if (count >= 128) return;
        heap[count] = e;
        heapifyUp(count);
        count++;
    }

    Emergency pop() {
        if (isEmpty()) {
            Emergency emptyE;
            emptyE.id = -1;
            return emptyE;
        }
        Emergency top = heap[0];
        count--;
        if (count > 0) {
            heap[0] = heap[count];
            heapifyDown(0);
        }
        return top;
    }

    Emergency top() const {
        if (isEmpty()) {
            Emergency emptyE;
            emptyE.id = -1;
            return emptyE;
        }
        return heap[0];
    }

    bool isEmpty() const {
        return count == 0;
    }

    int size() const {
        return count;
    }

    std::vector<Emergency> toVector() const {
        // Return sorted elements without modifying original
        EmergencyPriorityQueue copyQ = *this;
        std::vector<Emergency> res;
        while (!copyQ.isEmpty()) {
            res.push_back(copyQ.pop());
        }
        return res;
    }
};

// 5. Custom Circular Queue for Recent Event Buffer
class CircularEventBuffer {
private:
    static const int CAPACITY = 8;
    std::string events[CAPACITY];
    int frontIdx;
    int rearIdx;
    int count;

public:
    CircularEventBuffer() {
        frontIdx = 0;
        rearIdx = 0;
        count = 0;
    }

    void enqueue(const std::string& event) {
        if (count == CAPACITY) {
            // Overwrite oldest
            events[rearIdx] = event;
            rearIdx = (rearIdx + 1) % CAPACITY;
            frontIdx = (frontIdx + 1) % CAPACITY;
        } else {
            events[rearIdx] = event;
            rearIdx = (rearIdx + 1) % CAPACITY;
            count++;
        }
    }

    void display() const {
        if (count == 0) {
            std::cout << "No recent events recorded.\n";
            return;
        }
        for (int i = 0; i < count; i++) {
            int idx = (frontIdx + i) % CAPACITY;
            std::cout << "  [" << (i + 1) << "] " << events[idx] << "\n";
        }
    }
};

// 5b. Custom Stack for Dispatch History & Undo
struct DispatchAction {
    int emergencyId;
    std::string vehicleId;
    int previousLocation;
    Emergency emergencySnapshot;
};

class DispatchUndoStack {
private:
    DispatchAction items[64];
    int topIndex;
public:
    DispatchUndoStack() {
        topIndex = -1;
    }

    void push(const DispatchAction& act) {
        if (topIndex < 63) {
            items[++topIndex] = act;
        }
    }

    DispatchAction pop() {
        if (topIndex >= 0) {
            return items[topIndex--];
        }
        Emergency emptyE;
        emptyE.id = -1;
        return {-1, "", -1, emptyE};
    }

    bool isEmpty() const {
        return topIndex == -1;
    }
};

// 6. Custom Binary Search Tree for Historical Emergency Records
struct BSTNode {
    Emergency record;
    BSTNode* left;
    BSTNode* right;

    BSTNode(const Emergency& e) {
        record = e;
        left = nullptr;
        right = nullptr;
    }
};

class EmergencyBST {
private:
    BSTNode* root;

    BSTNode* insertRec(BSTNode* node, const Emergency& e) {
        if (node == nullptr) {
            return new BSTNode(e);
        }
        if (e.id < node->record.id) {
            node->left = insertRec(node->left, e);
        } else if (e.id > node->record.id) {
            node->right = insertRec(node->right, e);
        }
        return node;
    }

    BSTNode* searchRec(BSTNode* node, int id) const {
        if (node == nullptr || node->record.id == id) {
            return node;
        }
        if (id < node->record.id) {
            return searchRec(node->left, id);
        }
        return searchRec(node->right, id);
    }

    BSTNode* findMin(BSTNode* node) {
        while (node && node->left != nullptr) {
            node = node->left;
        }
        return node;
    }

    BSTNode* deleteRec(BSTNode* node, int id) {
        if (node == nullptr) return nullptr;

        if (id < node->record.id) {
            node->left = deleteRec(node->left, id);
        } else if (id > node->record.id) {
            node->right = deleteRec(node->right, id);
        } else {
            // Case 1: Leaf node
            if (node->left == nullptr && node->right == nullptr) {
                delete node;
                return nullptr;
            }
            // Case 2: One child
            if (node->left == nullptr) {
                BSTNode* temp = node->right;
                delete node;
                return temp;
            } else if (node->right == nullptr) {
                BSTNode* temp = node->left;
                delete node;
                return temp;
            }
            // Case 3: Two children
            BSTNode* temp = findMin(node->right);
            node->record = temp->record;
            node->right = deleteRec(node->right, temp->record.id);
        }
        return node;
    }

    void inorderRec(BSTNode* node, std::vector<Emergency>& list) const {
        if (node != nullptr) {
            inorderRec(node->left, list);
            list.push_back(node->record);
            inorderRec(node->right, list);
        }
    }

    void preorderRec(BSTNode* node) const {
        if (node != nullptr) {
            std::cout << "E" << node->record.id << " ";
            preorderRec(node->left);
            preorderRec(node->right);
        }
    }

    void postorderRec(BSTNode* node) const {
        if (node != nullptr) {
            postorderRec(node->left);
            postorderRec(node->right);
            std::cout << "E" << node->record.id << " ";
        }
    }

    void destroyTree(BSTNode* node) {
        if (node != nullptr) {
            destroyTree(node->left);
            destroyTree(node->right);
            delete node;
        }
    }

public:
    EmergencyBST() {
        root = nullptr;
    }

    ~EmergencyBST() {
        destroyTree(root);
    }

    void insert(const Emergency& e) {
        root = insertRec(root, e);
    }

    bool search(int id, Emergency& found) const {
        BSTNode* node = searchRec(root, id);
        if (node != nullptr) {
            found = node->record;
            return true;
        }
        return false;
    }

    void deleteKey(int id) {
        root = deleteRec(root, id);
    }

    std::vector<Emergency> getInorderList() const {
        std::vector<Emergency> res;
        inorderRec(root, res);
        return res;
    }

    void printPreorder() const {
        preorderRec(root);
        std::cout << "\n";
    }

    void printPostorder() const {
        postorderRec(root);
        std::cout << "\n";
    }
};

// 7. Custom Trie for Location Name Autocomplete
struct TrieNode {
    TrieNode* children[26];
    bool isEndOfWord;
    int locationId;
    std::string fullName;

    TrieNode() {
        isEndOfWord = false;
        locationId = -1;
        fullName = "";
        for (int i = 0; i < 26; i++) {
            children[i] = nullptr;
        }
    }
};

class LocationTrie {
private:
    TrieNode* root;

    int getCharIndex(char ch) const {
        char lower = std::tolower(ch);
        if (lower >= 'a' && lower <= 'z') {
            return lower - 'a';
        }
        return -1;
    }

    void collectAll(TrieNode* node, std::vector<std::pair<std::string, int>>& suggestions) const {
        if (node == nullptr) return;
        if (node->isEndOfWord) {
            suggestions.push_back({node->fullName, node->locationId});
        }
        for (int i = 0; i < 26; i++) {
            if (node->children[i] != nullptr) {
                collectAll(node->children[i], suggestions);
            }
        }
    }

    void destroyTrie(TrieNode* node) {
        if (node == nullptr) return;
        for (int i = 0; i < 26; i++) {
            if (node->children[i] != nullptr) {
                destroyTrie(node->children[i]);
            }
        }
        delete node;
    }

public:
    LocationTrie() {
        root = new TrieNode();
    }

    ~LocationTrie() {
        destroyTrie(root);
    }

    void insert(const std::string& name, int id) {
        TrieNode* curr = root;
        for (char ch : name) {
            int idx = getCharIndex(ch);
            if (idx == -1) continue; // skip spaces or symbols
            if (curr->children[idx] == nullptr) {
                curr->children[idx] = new TrieNode();
            }
            curr = curr->children[idx];
        }
        curr->isEndOfWord = true;
        curr->locationId = id;
        curr->fullName = name;
    }

    int search(const std::string& name) const {
        TrieNode* curr = root;
        for (char ch : name) {
            int idx = getCharIndex(ch);
            if (idx == -1) continue;
            if (curr->children[idx] == nullptr) {
                return -1;
            }
            curr = curr->children[idx];
        }
        if (curr != nullptr && curr->isEndOfWord) {
            return curr->locationId;
        }
        return -1;
    }

    std::vector<std::pair<std::string, int>> autocomplete(const std::string& prefix) const {
        std::vector<std::pair<std::string, int>> results;
        TrieNode* curr = root;
        for (char ch : prefix) {
            int idx = getCharIndex(ch);
            if (idx == -1) continue;
            if (curr->children[idx] == nullptr) {
                return results; // No matches
            }
            curr = curr->children[idx];
        }
        collectAll(curr, results);
        return results;
    }
};

// Global module state
namespace {
    std::vector<Vehicle> fleet;
    EmergencyQueue intakeQueue;
    EmergencyPriorityQueue priorityQueue;
    CircularEventBuffer eventBuffer;
    DispatchUndoStack undoStack;
    EmergencyBST historicalBST;
    LocationTrie locationTrie;
    std::unordered_map<std::string, int> locationHashMap;

    int nextEmergencyId = 201;
    int completedCount = 0;

    bool isCompatible(VehicleType vType, EmergencyType eType) {
        switch (eType) {
            case EmergencyType::MEDICAL:
                return vType == VehicleType::AMBULANCE;
            case EmergencyType::FIRE:
                return vType == VehicleType::FIRE_TRUCK;
            case EmergencyType::CRIME:
                return vType == VehicleType::POLICE;
            case EmergencyType::ACCIDENT:
                return vType == VehicleType::AMBULANCE || vType == VehicleType::POLICE;
            case EmergencyType::DISASTER:
                return true; // Any emergency vehicle can assist
        }
        return false;
    }
}

std::string getVehicleTypeName(VehicleType t) {
    switch (t) {
        case VehicleType::AMBULANCE:  return "Ambulance";
        case VehicleType::FIRE_TRUCK: return "Fire Truck";
        case VehicleType::POLICE:     return "Police Vehicle";
    }
    return "Unknown";
}

std::string getEmergencyTypeName(EmergencyType t) {
    switch (t) {
        case EmergencyType::MEDICAL:  return "Medical";
        case EmergencyType::FIRE:     return "Fire";
        case EmergencyType::CRIME:    return "Crime";
        case EmergencyType::ACCIDENT: return "Accident";
        case EmergencyType::DISASTER: return "Disaster";
    }
    return "Unknown";
}

std::string getSeverityName(Severity s) {
    switch (s) {
        case Severity::CRITICAL: return "CRITICAL";
        case Severity::HIGH:     return "HIGH";
        case Severity::MEDIUM:   return "MEDIUM";
        case Severity::LOW:      return "LOW";
    }
    return "NORMAL";
}

std::string getStatusName(EmergencyStatus st) {
    switch (st) {
        case EmergencyStatus::PENDING:     return "PENDING";
        case EmergencyStatus::DISPATCHED:  return "DISPATCHED";
        case EmergencyStatus::IN_PROGRESS: return "IN_PROGRESS";
        case EmergencyStatus::COMPLETED:   return "COMPLETED";
        case EmergencyStatus::CANCELLED:   return "CANCELLED";
    }
    return "UNKNOWN";
}

void initEmergencySystem() {
    fleet.clear();

    // Preset fleet of 8 vehicles at diverse locations
    fleet.push_back(Vehicle("A01", VehicleType::AMBULANCE, 1));   // Shivajinagar
    fleet.push_back(Vehicle("A02", VehicleType::AMBULANCE, 2));   // Deccan
    fleet.push_back(Vehicle("A03", VehicleType::AMBULANCE, 16));  // Kondhwa
    fleet.push_back(Vehicle("F01", VehicleType::FIRE_TRUCK, 8));  // Kharadi
    fleet.push_back(Vehicle("F02", VehicleType::FIRE_TRUCK, 10)); // Kothrud
    fleet.push_back(Vehicle("P01", VehicleType::POLICE, 7));      // Kalyani Nagar
    fleet.push_back(Vehicle("P02", VehicleType::POLICE, 11));     // Swargate
    fleet.push_back(Vehicle("P03", VehicleType::POLICE, 13));     // Hinjewadi

    // Populate Location Trie and Hash Map
    for (int i = 1; i <= 16; i++) {
        std::string name = getLocationName(i);
        locationTrie.insert(name, i);
        locationHashMap[name] = i;
    }

    // Embed 20+ Historical Emergency Records into BST
    struct PresetRecord {
        int id;
        EmergencyType type;
        Severity sev;
        int loc;
        int rTime;
        std::string vId;
        std::string desc;
    };

    PresetRecord presets[] = {
        {101, EmergencyType::MEDICAL,  Severity::CRITICAL, 10, 6,  "A02", "Cardiac arrest near Kothrud stand"},
        {102, EmergencyType::FIRE,     Severity::CRITICAL, 8,  4,  "F01", "Commercial kitchen fire Kharadi"},
        {103, EmergencyType::CRIME,    Severity::HIGH,     5,  7,  "P03", "Store break-in Baner highway"},
        {104, EmergencyType::ACCIDENT, Severity::MEDIUM,   9,  5,  "P03", "Two-wheeler collision Wakad"},
        {105, EmergencyType::MEDICAL,  Severity::HIGH,     2,  3,  "A02", "Breathing difficulty Deccan gym"},
        {106, EmergencyType::FIRE,     Severity::MEDIUM,   14, 8,  "F02", "Electrical transformer sparks Warje"},
        {107, EmergencyType::CRIME,    Severity::LOW,      7,  4,  "P01", "Suspicious vehicle Kalyani Nagar"},
        {108, EmergencyType::MEDICAL,  Severity::CRITICAL, 1,  5,  "A01", "Severe injury Shivajinagar station"},
        {109, EmergencyType::ACCIDENT, Severity::CRITICAL, 6,  4,  "A01", "Multi-vehicle crash FC Road"},
        {110, EmergencyType::DISASTER, Severity::HIGH,     15, 9,  "P02", "Localized basement flooding Camp"},
        {111, EmergencyType::MEDICAL,  Severity::MEDIUM,   11, 4,  "P02", "Elderly fall Swargate junction"},
        {112, EmergencyType::CRIME,    Severity::HIGH,     3,  6,  "P01", "Burglary reported Yerwada"},
        {113, EmergencyType::FIRE,     Severity::CRITICAL, 4,  7,  "F01", "Warehouse fire Viman Nagar"},
        {114, EmergencyType::MEDICAL,  Severity::LOW,      12, 11, "A03", "Minor heat exhaustion Hadapsar"},
        {115, EmergencyType::ACCIDENT, Severity::HIGH,     13, 5,  "P03", "Cab collision Hinjewadi Phase 1"},
        {116, EmergencyType::MEDICAL,  Severity::CRITICAL, 16, 3,  "A03", "Asthma attack Kondhwa road"},
        {117, EmergencyType::FIRE,     Severity::LOW,      10, 5,  "F02", "Rubbish bin fire Kothrud"},
        {118, EmergencyType::CRIME,    Severity::MEDIUM,   6,  6,  "P02", "Pickpocket incident FC road"},
        {119, EmergencyType::MEDICAL,  Severity::HIGH,     7,  5,  "A01", "Diabetic emergency Kalyani"},
        {120, EmergencyType::ACCIDENT, Severity::MEDIUM,   14, 7,  "A02", "Bicycle incident Warje bridge"},
        {121, EmergencyType::DISASTER, Severity::CRITICAL, 9,  8,  "F02", "Gas pipeline leak Wakad chowk"},
        {122, EmergencyType::CRIME,    Severity::HIGH,     11, 4,  "P02", "Shoplifting Swargate market"}
    };

    for (const auto& p : presets) {
        Emergency e;
        e.id = p.id;
        e.type = p.type;
        e.severity = p.sev;
        e.locationId = p.loc;
        e.status = EmergencyStatus::COMPLETED;
        e.responseTime = p.rTime;
        e.assignedVehicleId = p.vId;
        e.description = p.desc;
        historicalBST.insert(e);

        // Pre-populate vehicle history
        for (auto& v : fleet) {
            if (v.id == p.vId) {
                v.totalEmergencies++;
                v.totalResponseTime += p.rTime;
                v.totalDistance += (p.rTime * 0.8);
                v.history.addRecord(p.id, p.type, p.rTime, p.loc);
                break;
            }
        }
    }

    eventBuffer.enqueue("System initialized with 8 vehicles and 22 historical records.");
}

bool reportEmergency(int locId, EmergencyType type, Severity sev, const std::string& desc, int& outId) {
    if (locId < 1 || locId > 16) return false;

    Emergency e;
    e.id = nextEmergencyId++;
    e.type = type;
    e.severity = sev;
    e.locationId = locId;
    e.status = EmergencyStatus::PENDING;
    e.responseTime = 0;
    e.assignedVehicleId = "UNASSIGNED";
    e.description = desc;

    outId = e.id;

    // Place into normal queue first (FIFO intake)
    intakeQueue.enqueue(e);

    // Also transfer to priority queue for dispatch prioritization
    priorityQueue.push(e);

    eventBuffer.enqueue("Reported E" + std::to_string(e.id) + " [" + getEmergencyTypeName(e.type) +
                        "] at " + getLocationName(e.locationId) + " (" + getSeverityName(e.severity) + ")");
    return true;
}

// Greedy Vehicle Selection & Dispatch
bool processNextEmergency() {
    if (priorityQueue.isEmpty()) {
        std::cout << "\nEmergency priority queue is empty. No pending emergencies to dispatch.\n";
        return false;
    }

    Emergency current = priorityQueue.pop();

    // Also pop from FIFO intake queue if matches front
    if (!intakeQueue.isEmpty() && intakeQueue.front().id == current.id) {
        intakeQueue.dequeue();
    }

    std::cout << "\n============================================================\n";
    std::cout << "             PROCESSING EMERGENCY DISPATCH\n";
    std::cout << "============================================================\n";
    std::cout << "Emergency ID : E" << current.id << "\n";
    std::cout << "Type         : " << getEmergencyTypeName(current.type) << "\n";
    std::cout << "Severity     : " << getSeverityName(current.severity) << "\n";
    std::cout << "Location     : [" << current.locationId << "] " << getLocationName(current.locationId) << "\n";
    std::cout << "Description  : " << current.description << "\n";
    std::cout << "------------------------------------------------------------\n";
    std::cout << "EVALUATING AVAILABLE COMPATIBLE VEHICLES (GREEDY CRITERIA):\n\n";

    int bestVehicleIdx = -1;
    int minTime = 999999;
    RouteResult bestRoute;

    for (size_t i = 0; i < fleet.size(); i++) {
        if (!fleet[i].isAvailable) continue;
        if (!isCompatible(fleet[i].type, current.type)) continue;

        RouteResult route = dijkstra(fleet[i].currentLocation, current.locationId);
        if (route.found) {
            std::cout << "  " << fleet[i].id << " (" << getVehicleTypeName(fleet[i].type)
                      << ") at " << getLocationName(fleet[i].currentLocation)
                      << " -> ETA: " << route.travelTime << " min, Distance: "
                      << route.distance << " km\n";

            if (route.travelTime < minTime) {
                minTime = route.travelTime;
                bestVehicleIdx = static_cast<int>(i);
                bestRoute = route;
            } else if (route.travelTime == minTime && bestVehicleIdx != -1) {
                // Deterministic tie breaker: vehicle ID
                if (fleet[i].id < fleet[bestVehicleIdx].id) {
                    bestVehicleIdx = static_cast<int>(i);
                    bestRoute = route;
                }
            }
        } else {
            std::cout << "  " << fleet[i].id << " (" << getVehicleTypeName(fleet[i].type)
                      << ") at " << getLocationName(fleet[i].currentLocation)
                      << " -> UNREACHABLE due to road closures\n";
        }
    }

    if (bestVehicleIdx == -1) {
        std::cout << "\nWARNING: No compatible available vehicle can reach the incident site!\n";
        std::cout << "Emergency E" << current.id << " re-queued with CRITICAL priority escalation.\n";
        current.severity = Severity::CRITICAL;
        priorityQueue.push(current);
        return false;
    }

    // Assign vehicle
    Vehicle& chosen = fleet[bestVehicleIdx];
    chosen.isAvailable = false;
    chosen.currentEmergencyId = current.id;

    current.status = EmergencyStatus::IN_PROGRESS;
    current.assignedVehicleId = chosen.id;
    current.responseTime = bestRoute.travelTime;

    std::cout << "\nSelected Vehicle : " << chosen.id << " (" << getVehicleTypeName(chosen.type) << ")\n";
    std::cout << "Dispatch Reason  : Minimum estimated response time (" << minTime << " minutes)\n";
    std::cout << "Optimal Route    : ";
    for (size_t i = 0; i < bestRoute.path.size(); i++) {
        std::cout << getLocationName(bestRoute.path[i]);
        if (i + 1 < bestRoute.path.size()) std::cout << " -> ";
    }
    std::cout << "\nStatus           : Vehicle DISPATCHED & EN ROUTE\n";

    // Insert active record into historical BST for immediate indexing
    historicalBST.insert(current);

    // Record action for Stack Undo capability
    undoStack.push({current.id, chosen.id, chosen.currentLocation, current});

    eventBuffer.enqueue("Dispatched " + chosen.id + " to E" + std::to_string(current.id) +
                        " at " + getLocationName(current.locationId) + " (ETA: " + std::to_string(minTime) + "m)");
    return true;
}

bool completeActiveEmergency(int emergencyId) {
    int foundIdx = -1;
    for (size_t i = 0; i < fleet.size(); i++) {
        if (!fleet[i].isAvailable && fleet[i].currentEmergencyId == emergencyId) {
            foundIdx = static_cast<int>(i);
            break;
        }
    }

    if (foundIdx == -1) {
        std::cout << "No active dispatched vehicle found for Emergency ID E" << emergencyId << ".\n";
        return false;
    }

    Vehicle& v = fleet[foundIdx];
    Emergency e;
    if (!historicalBST.search(emergencyId, e)) {
        std::cout << "Emergency record not found in system.\n";
        return false;
    }

    // Vehicle relocates to emergency site
    v.isAvailable = true;
    v.currentEmergencyId = -1;
    v.currentLocation = e.locationId;
    v.totalEmergencies++;
    v.totalResponseTime += e.responseTime;
    v.totalDistance += (e.responseTime * 0.85);

    // Add to linked list history
    v.history.addRecord(e.id, e.type, e.responseTime, e.locationId);

    // Update status in BST
    e.status = EmergencyStatus::COMPLETED;
    historicalBST.deleteKey(e.id);
    historicalBST.insert(e);

    completedCount++;

    std::cout << "\nEmergency E" << e.id << " marked COMPLETED successfully.\n";
    std::cout << "Vehicle " << v.id << " is now AVAILABLE at [" << v.currentLocation
              << "] " << getLocationName(v.currentLocation) << ".\n";

    eventBuffer.enqueue("Completed E" + std::to_string(e.id) + " by " + v.id +
                        " at " + getLocationName(e.locationId));
    return true;
}

bool cancelEmergency(int emergencyId) {
    for (auto& v : fleet) {
        if (!v.isAvailable && v.currentEmergencyId == emergencyId) {
            v.isAvailable = true;
            v.currentEmergencyId = -1;
            break;
        }
    }

    Emergency e;
    if (historicalBST.search(emergencyId, e)) {
        e.status = EmergencyStatus::CANCELLED;
        historicalBST.deleteKey(emergencyId);
        historicalBST.insert(e);
        std::cout << "Emergency E" << emergencyId << " has been CANCELLED.\n";
        eventBuffer.enqueue("Cancelled E" + std::to_string(emergencyId));
        return true;
    }
    return false;
}

int getAvailableVehiclesCount() {
    int count = 0;
    for (const auto& v : fleet) {
        if (v.isAvailable) count++;
    }
    return count;
}

int getBusyVehiclesCount() {
    int count = 0;
    for (const auto& v : fleet) {
        if (!v.isAvailable) count++;
    }
    return count;
}

int getPendingEmergenciesCount() {
    return priorityQueue.size();
}

int getCompletedEmergenciesCount() {
    return completedCount + 22; // 22 preset completed records
}

void printDashboardStatus() {
    std::cout << "Locations       : 16\n";
    std::cout << "Available       : " << getAvailableVehiclesCount() << "\n";
    std::cout << "Busy Vehicles   : " << getBusyVehiclesCount() << "\n";
    std::cout << "Pending         : " << getPendingEmergenciesCount() << "\n";
    std::cout << "Completed       : " << getCompletedEmergenciesCount() << "\n";
}

void printEmergencyQueue() {
    std::cout << "\n============================================================\n";
    std::cout << "              EMERGENCY PRIORITY QUEUE\n";
    std::cout << "============================================================\n";
    if (priorityQueue.isEmpty()) {
        std::cout << "Priority queue is currently empty. No pending emergencies.\n";
        return;
    }

    std::vector<Emergency> queueList = priorityQueue.toVector();
    std::cout << "ID\tType\t\tLocation\tPriority\tDescription\n";
    std::cout << "------------------------------------------------------------\n";
    for (const auto& e : queueList) {
        std::cout << "E" << e.id << "\t"
                  << getEmergencyTypeName(e.type) << "\t\t"
                  << getLocationName(e.locationId) << "\t"
                  << getSeverityName(e.severity) << "\t"
                  << e.description << "\n";
    }
}

void printAllVehicles() {
    std::cout << "\n============================================================\n";
    std::cout << "                 EMERGENCY FLEET STATUS\n";
    std::cout << "============================================================\n";
    std::cout << "ID\tType\t\tCurrent Station\t\tStatus\t\tTotal Served\n";
    std::cout << "------------------------------------------------------------\n";
    for (const auto& v : fleet) {
        std::cout << v.id << "\t"
                  << getVehicleTypeName(v.type) << "\t"
                  << "[" << v.currentLocation << "] " << getLocationName(v.currentLocation) << "\t\t"
                  << (v.isAvailable ? "AVAILABLE" : ("BUSY (E" + std::to_string(v.currentEmergencyId) + ")")) << "\t"
                  << v.totalEmergencies << " incidents\n";
    }
}

void printVehicleHistory(const std::string& vehicleId) {
    std::cout << "\n============================================================\n";
    std::cout << "         VEHICLE DISPATCH HISTORY (LINKED LIST)\n";
    std::cout << "============================================================\n";
    for (const auto& v : fleet) {
        if (v.id == vehicleId) {
            std::cout << "Vehicle ID  : " << v.id << " (" << getVehicleTypeName(v.type) << ")\n";
            std::cout << "Station     : [" << v.currentLocation << "] " << getLocationName(v.currentLocation) << "\n";
            std::cout << "Total Runs  : " << v.totalEmergencies << "\n";
            std::cout << "Total Dist  : " << v.totalDistance << " km\n";
            std::cout << "Total Time  : " << v.totalResponseTime << " min\n";
            std::cout << "History Chain:\n";
            v.history.display();
            return;
        }
    }
    std::cout << "Vehicle with ID '" << vehicleId << "' was not found.\n";
}

void printRecentEvents() {
    std::cout << "\n============================================================\n";
    std::cout << "      RECENT EVENT LOG (CIRCULAR BUFFER - LAST 8 EVENTS)\n";
    std::cout << "============================================================\n";
    eventBuffer.display();
}

void searchLocationTrie(const std::string& prefix) {
    std::cout << "\n============================================================\n";
    std::cout << "         LOCATION SEARCH & AUTOCOMPLETE (TRIE)\n";
    std::cout << "============================================================\n";
    std::cout << "Search query: \"" << prefix << "\"\n";
    std::cout << "------------------------------------------------------------\n";

    auto results = locationTrie.autocomplete(prefix);
    if (results.empty()) {
        std::cout << "No matching locations found for prefix '" << prefix << "'.\n";
        return;
    }

    std::cout << "Matching Suggestions (" << results.size() << " found):\n";
    for (size_t i = 0; i < results.size(); i++) {
        std::cout << "  " << (i + 1) << ". " << results[i].first
                  << " (Location ID: " << results[i].second << ")\n";
    }
}

void searchEmergencyBST(int eId) {
    std::cout << "\n============================================================\n";
    std::cout << "           EMERGENCY RECORD LOOKUP (BST)\n";
    std::cout << "============================================================\n";
    Emergency e;
    if (historicalBST.search(eId, e)) {
        std::cout << "Record Found:\n";
        std::cout << "  ID          : E" << e.id << "\n";
        std::cout << "  Type        : " << getEmergencyTypeName(e.type) << "\n";
        std::cout << "  Severity    : " << getSeverityName(e.severity) << "\n";
        std::cout << "  Location    : [" << e.locationId << "] " << getLocationName(e.locationId) << "\n";
        std::cout << "  Status      : " << getStatusName(e.status) << "\n";
        std::cout << "  Vehicle     : " << e.assignedVehicleId << "\n";
        std::cout << "  Response Tm : " << e.responseTime << " minutes\n";
        std::cout << "  Description : " << e.description << "\n";
    } else {
        std::cout << "Emergency ID E" << eId << " not found in historical records.\n";
    }
}

void deleteEmergencyBSTRecord(int eId) {
    historicalBST.deleteKey(eId);
    std::cout << "Emergency record E" << eId << " deleted from BST.\n";
}

void printBSTTraversals() {
    std::cout << "\n============================================================\n";
    std::cout << "           BST TRAVERSALS FOR EMERGENCY RECORDS\n";
    std::cout << "============================================================\n";
    std::cout << "1. Preorder Traversal  : ";
    historicalBST.printPreorder();

    std::cout << "2. Postorder Traversal : ";
    historicalBST.printPostorder();

    std::cout << "3. Inorder Traversal (Sorted by ID):\n";
    auto list = historicalBST.getInorderList();
    for (size_t i = 0; i < list.size(); i++) {
        std::cout << "E" << list[i].id << " (" << getEmergencyTypeName(list[i].type)
                  << ", " << getSeverityName(list[i].severity) << ") ";
        if ((i + 1) % 5 == 0) std::cout << "\n";
    }
    std::cout << "\n";
}

std::vector<Emergency> getAllHistoricalEmergencies() {
    return historicalBST.getInorderList();
}

int getVehicleLocation(const std::string& vehicleId) {
    for (const auto& v : fleet) {
        if (v.id == vehicleId) return v.currentLocation;
    }
    return -1;
}

std::vector<std::string> getVehiclesAtLocation(int locId) {
    std::vector<std::string> res;
    for (const auto& v : fleet) {
        if (v.currentLocation == locId) {
            res.push_back(v.id);
        }
    }
    return res;
}

std::vector<WebVehicleInfo> getWebVehicleInfos() {
    std::vector<WebVehicleInfo> result;
    result.reserve(fleet.size());
    for (const auto& v : fleet) {
        WebVehicleInfo info;
        info.id = v.id;
        info.type = getVehicleTypeName(v.type);
        info.locationId = v.currentLocation;
        info.isAvailable = v.isAvailable;
        info.currentEmergencyId = v.currentEmergencyId;
        info.totalEmergencies = v.totalEmergencies;
        info.totalResponseTime = v.totalResponseTime;
        info.totalDistance = v.totalDistance;
        result.push_back(info);
    }
    return result;
}

bool webSetVehicleReturningToStandby(const std::string& vehicleId, int standbyLocationId) {
    if (standbyLocationId < 1 || standbyLocationId > 16) return false;
    for (auto& v : fleet) {
        if (v.id == vehicleId && v.isAvailable && v.currentEmergencyId == -1) {
            // Keep the vehicle unavailable while it is travelling back to standby.
            v.isAvailable = false;
            v.currentEmergencyId = -2;
            eventBuffer.enqueue("Responder " + v.id + " returning to standby at " + getLocationName(standbyLocationId));
            return true;
        }
    }
    return false;
}

bool webMarkStandbyArrived(const std::string& vehicleId, int standbyLocationId) {
    if (standbyLocationId < 1 || standbyLocationId > 16) return false;
    for (auto& v : fleet) {
        if (v.id == vehicleId && !v.isAvailable && v.currentEmergencyId == -2) {
            v.currentLocation = standbyLocationId;
            v.currentEmergencyId = -1;
            v.isAvailable = true;
            eventBuffer.enqueue("Responder " + v.id + " arrived at standby " + getLocationName(standbyLocationId));
            return true;
        }
    }
    return false;
}

bool webReleaseVehicle(const std::string& vehicleId, int emergencyId) {
    for (auto& v : fleet) {
        if (v.id == vehicleId && (emergencyId <= 0 || v.currentEmergencyId == emergencyId)) {
            v.isAvailable = true;
            v.currentEmergencyId = -1;
            eventBuffer.enqueue("Responder " + v.id + " released from emergency E" + std::to_string(emergencyId));
            return true;
        }
    }
    return false;
}

bool webAssignVehicle(const std::string& vehicleId, int emergencyId, int destLocId, int& outTravelTime) {
    if (destLocId < 1 || destLocId > 16) return false;
    for (auto& v : fleet) {
        if (v.id == vehicleId && v.isAvailable) {
            RouteResult route = dijkstra(v.currentLocation, destLocId);
            if (!route.found) return false;
            v.isAvailable = false;
            v.currentEmergencyId = emergencyId;
            outTravelTime = route.travelTime;
            eventBuffer.enqueue("Reassigned " + v.id + " to E" + std::to_string(emergencyId) +
                                " at " + getLocationName(destLocId) + " (ETA: " + std::to_string(outTravelTime) + "m)");
            return true;
        }
    }
    return false;
}

void runEmergencyCoreDemos() {
    std::cout << "\n--- Emergency Core Data Structure Demonstrations ---\n";
    std::cout << "1. Location Trie Autocomplete for 'Kot':\n";
    searchLocationTrie("Kot");

    std::cout << "\n2. BST Search for Emergency E101:\n";
    searchEmergencyBST(101);

    std::cout << "\n3. Vehicle History Linked List for A02:\n";
    printVehicleHistory("A02");

    std::cout << "\n4. Circular Event Buffer:\n";
    printRecentEvents();
}

bool undoLastDispatch() {
    if (undoStack.isEmpty()) {
        std::cout << "\nNo previous dispatch actions available in Undo Stack.\n";
        return false;
    }

    DispatchAction act = undoStack.pop();
    for (auto& v : fleet) {
        if (v.id == act.vehicleId && v.currentEmergencyId == act.emergencyId) {
            v.isAvailable = true;
            v.currentEmergencyId = -1;
            v.currentLocation = act.previousLocation;
            break;
        }
    }

    historicalBST.deleteKey(act.emergencyId);
    act.emergencySnapshot.status = EmergencyStatus::PENDING;
    act.emergencySnapshot.assignedVehicleId = "UNASSIGNED";
    priorityQueue.push(act.emergencySnapshot);

    std::cout << "\n============================================================\n";
    std::cout << "          UNDO LAST DISPATCH (CUSTOM STACK)\n";
    std::cout << "============================================================\n";
    std::cout << "SUCCESS: Dispatch undone using Stack!\n";
    std::cout << "Vehicle " << act.vehicleId << " returned to station ["
              << act.previousLocation << "] " << getLocationName(act.previousLocation) << ".\n";
    std::cout << "Emergency E" << act.emergencyId << " restored to priority queue.\n";

    eventBuffer.enqueue("Undid dispatch of " + act.vehicleId + " to E" + std::to_string(act.emergencyId));
    return true;
}

void demonstrateQueueVsPriorityQueue() {
    std::cout << "\n============================================================\n";
    std::cout << "      QUEUE VS PRIORITY QUEUE DEMONSTRATION (VIVA TOPIC)\n";
    std::cout << "============================================================\n";
    std::cout << "Inserting 4 incoming emergencies in sequential order of arrival:\n";
    std::cout << "  1. E301 | Medical  | Low Severity\n";
    std::cout << "  2. E302 | Crime    | High Severity\n";
    std::cout << "  3. E303 | Accident | Medium Severity\n";
    std::cout << "  4. E304 | Fire     | CRITICAL Severity\n\n";

    Emergency e1{301, EmergencyType::MEDICAL,  Severity::LOW,      1, EmergencyStatus::PENDING, 0, "", "Minor sprain"};
    Emergency e2{302, EmergencyType::CRIME,    Severity::HIGH,     2, EmergencyStatus::PENDING, 0, "", "Armed robbery"};
    Emergency e3{303, EmergencyType::ACCIDENT, Severity::MEDIUM,   3, EmergencyStatus::PENDING, 0, "", "Vehicle collision"};
    Emergency e4{304, EmergencyType::FIRE,     Severity::CRITICAL, 4, EmergencyStatus::PENDING, 0, "", "Building fire"};

    EmergencyQueue standardQueue;
    EmergencyPriorityQueue pQueue;

    standardQueue.enqueue(e1);
    standardQueue.enqueue(e2);
    standardQueue.enqueue(e3);
    standardQueue.enqueue(e4);

    pQueue.push(e1);
    pQueue.push(e2);
    pQueue.push(e3);
    pQueue.push(e4);

    std::cout << "1. NORMAL FIFO QUEUE (Order of Departure = Order of Arrival):\n";
    int step = 1;
    while (!standardQueue.isEmpty()) {
        Emergency e = standardQueue.dequeue();
        std::cout << "   Step " << step++ << ": E" << e.id << " (" << getEmergencyTypeName(e.type)
                  << ") -> Severity: " << getSeverityName(e.severity) << "\n";
    }

    std::cout << "\n2. PRIORITY QUEUE (Order of Departure = Highest Severity First):\n";
    step = 1;
    while (!pQueue.isEmpty()) {
        Emergency e = pQueue.pop();
        std::cout << "   Step " << step++ << ": E" << e.id << " (" << getEmergencyTypeName(e.type)
                  << ") -> Severity: " << getSeverityName(e.severity) << "\n";
    }
    std::cout << "\nViva Takeaway: Normal Queue guarantees First-In-First-Out fairness,\n"
              << "while Priority Queue organizes triage where life-threatening\n"
              << "CRITICAL emergencies preempt lower-priority incidents.\n";
}
