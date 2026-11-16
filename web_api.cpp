#include <algorithm>
#include <arpa/inet.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <limits>
#include <cctype>
#include <cstdio>
#include <sys/time.h>
#include <netinet/in.h>
#include <regex>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

// The API intentionally uses the same public types and functions as the existing
// C++ implementation. Dispatch decisions remain in emergency_core.cpp.
enum class VehicleType { AMBULANCE, FIRE_TRUCK, POLICE };
enum class EmergencyType { MEDICAL, FIRE, CRIME, ACCIDENT, DISASTER };
enum class Severity { LOW = 1, MEDIUM = 2, HIGH = 3, CRITICAL = 4 };
enum class EmergencyStatus { PENDING, DISPATCHED, IN_PROGRESS, COMPLETED, CANCELLED };
struct Emergency {
    int id; EmergencyType type; Severity severity; int locationId; EmergencyStatus status;
    int responseTime; std::string assignedVehicleId; std::string description;
};
struct RouteResult { int travelTime; double distance; std::vector<int> path; bool found; };
struct WebVehicleInfo {
    std::string id, type; int locationId; bool isAvailable; int currentEmergencyId;
    int totalEmergencies; int totalResponseTime; double totalDistance;
};
void initGraph();
void initEmergencySystem();
bool reportEmergency(int, EmergencyType, Severity, const std::string&, int&);
bool processNextEmergency();
bool completeActiveEmergency(int);
std::vector<Emergency> getAllHistoricalEmergencies();
std::vector<WebVehicleInfo> getWebVehicleInfos();
bool webSetVehicleReturningToStandby(const std::string&, int);
bool webMarkStandbyArrived(const std::string&, int);
bool webReleaseVehicle(const std::string&, int);
bool webAssignVehicle(const std::string&, int, int, int&);
std::string getLocationName(int);
int getLocationId(const std::string&);
int getTotalLocations();
RouteResult dijkstra(int, int);
std::vector<int> getOptimizedStandbyLocations(int);

namespace fs = std::filesystem;
struct Zone { int id; const char* name; double lat; double lng; };
static const Zone ZONES[] = {
    {1,"Shivajinagar",18.5308,73.8475},{2,"Deccan",18.5164,73.8417},
    {3,"Yerwada",18.5515,73.8796},{4,"Viman Nagar",18.5679,73.9143},
    {5,"Baner",18.5590,73.7868},{6,"FC Road",18.5236,73.8417},
    {7,"Kalyani Nagar",18.5480,73.9032},{8,"Kharadi",18.5515,73.9348},
    {9,"Wakad",18.5975,73.7639},{10,"Kothrud",18.5074,73.8077},
    {11,"Swargate",18.5018,73.8636},{12,"Hadapsar",18.5089,73.9260},
    {13,"Hinjewadi",18.5913,73.7389},{14,"Warje",18.4825,73.8021},
    {15,"Camp",18.5134,73.8796},{16,"Kondhwa",18.4635,73.8710}
};

// -----------------------------------------------------------------------
// ApiEmergency — extended with acceptance-deadline tracking and rejection
// -----------------------------------------------------------------------
struct ApiEmergency {
    int id = 0, locationId = 1, peopleAffected = 1, etaMinutes = 0;
    std::string type, severity, status = "RECEIVED", description, address, locationDetails, contact;
    std::string assignedVehicleId, createdAt, updatedAt;
    double lat = 0, lng = 0;
    int standbyLocationId = 0;
    // Acceptance-deadline fields (ISO strings; empty when no active assignment)
    std::string assignedAt;    // when the current assignment attempt was created
    std::string deadlineAt;    // assignedAt + ACCEPTANCE_DEADLINE_SECONDS
    // Rejection tracking — excluded from the next reassignment search
    std::set<std::string> rejectedBy;
    int rejectionCount = 0;
    std::string firstAssignedVehicleId;
};

static const int ACCEPTANCE_DEADLINE_SECONDS = 60;

static std::map<int, ApiEmergency> emergencies;
static std::map<std::string, int>  standbyTargets;
static std::string googleMapApi;
static const fs::path WEB_DIR = "web";

// -----------------------------------------------------------------------
// Utility helpers
// -----------------------------------------------------------------------
std::string nowIso() {
    std::time_t t = std::time(nullptr); std::tm tm{}; gmtime_r(&t, &tm);
    char b[32]; std::strftime(b, sizeof(b), "%Y-%m-%dT%H:%M:%SZ", &tm); return b;
}
std::string isoFromEpoch(std::time_t t) {
    std::tm tm{}; gmtime_r(&t, &tm);
    char b[32]; std::strftime(b, sizeof(b), "%Y-%m-%dT%H:%M:%SZ", &tm); return b;
}
std::time_t epochFromIso(const std::string& s) {
    if (s.empty()) return 0;
    std::tm tm{}; strptime(s.c_str(), "%Y-%m-%dT%H:%M:%SZ", &tm); return timegm(&tm);
}
bool isDeadlineExpired(const std::string& deadlineAt) {
    if (deadlineAt.empty()) return false;
    return std::time(nullptr) >= epochFromIso(deadlineAt);
}
std::string trim(const std::string& s) {
    size_t a=s.find_first_not_of(" \t\r\n"); if(a==std::string::npos)return "";
    size_t b=s.find_last_not_of(" \t\r\n"); return s.substr(a,b-a+1);
}
std::string jsonEscape(const std::string& s) {
    std::ostringstream o;
    for (unsigned char c : s) {
        switch(c) { case '"': o<<"\\\""; break; case '\\': o<<"\\\\"; break; case '\b': o<<"\\b"; break;
            case '\f': o<<"\\f"; break; case '\n': o<<"\\n"; break; case '\r': o<<"\\r"; break; case '\t': o<<"\\t"; break;
            default: if(c<0x20) { char b[7]; std::snprintf(b,sizeof(b),"\\u%04x",c); o<<b; } else o<<c;
        }
    }
    return o.str();
}
std::string js(const std::string& s) { return "\"" + jsonEscape(s) + "\""; }
std::string jsonString(const std::string& body, const std::string& key, const std::string& fallback="") {
    // Match: "key": "value"   (handles escaped chars inside value)
    std::string pat = "\"" + key + "\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\"";
    std::regex r(pat); std::smatch m;
    if(!std::regex_search(body,m,r)) return fallback;
    std::string raw=m[1], out;
    for(size_t i=0;i<raw.size();++i) { if(raw[i]=='\\' && i+1<raw.size()) { char c=raw[++i]; switch(c){case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;default:out+=c;} } else out+=raw[i]; }
    return out;
}
double jsonNumber(const std::string& body, const std::string& key, double fallback=0) {
    std::regex r("\""+key+"\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)"); std::smatch m;
    if(!std::regex_search(body,m,r)) return fallback;
    try{return std::stod(m[1]);}catch(...){return fallback;}
}
bool jsonHas(const std::string& body, const std::string& key) {
    return std::regex_search(body, std::regex("\""+key+"\"\\s*:"));
}
std::string lower(std::string s) { std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));}); return s; }
double radians(double d){return d*3.14159265358979323846/180.0;}
double geoDistanceKm(double aLat,double aLng,double bLat,double bLng) {
    double dLat=radians(bLat-aLat), dLng=radians(bLng-aLng);
    double a=std::sin(dLat/2)*std::sin(dLat/2)+std::cos(radians(aLat))*std::cos(radians(bLat))*std::sin(dLng/2)*std::sin(dLng/2);
    return 6371.0*2*std::atan2(std::sqrt(a),std::sqrt(std::max(0.0,1-a)));
}
int nearestZone(double lat,double lng) {
    int best=1; double d=1e99;
    for(const auto& z:ZONES){double x=geoDistanceKm(lat,lng,z.lat,z.lng);if(x<d){d=x;best=z.id;}}
    return best;
}
std::string typeDisplay(const std::string& t) {
    if (t == "medical") return "Medical";
    if (t == "fire") return "Fire";
    if (t == "road_accident") return "Road accident";
    if (t == "police") return "Police assistance";
    return "Other emergency";
}
EmergencyType coreType(const std::string& t) {
    if (t == "medical") return EmergencyType::MEDICAL;
    if (t == "fire") return EmergencyType::FIRE;
    if (t == "road_accident") return EmergencyType::ACCIDENT;
    if (t == "police") return EmergencyType::CRIME;
    return EmergencyType::DISASTER;
}
std::string priorityFor(const std::string& type,const std::string& desc,int people) {
    std::string s=lower(desc);
    const char* critical[] = {"unconscious","not breathing","cardiac","severe bleeding","trapped","explosion","collapsed","weapon","multiple casualties","life threatening"};
    if(people>=4) return "CRITICAL";
    for(const char* k:critical) if(s.find(k)!=std::string::npos) return "CRITICAL";
    if(type=="medical"||type=="fire"||type=="road_accident"||type=="other") return "HIGH";
    if(type=="police") return "MEDIUM";
    return "HIGH";
}
Severity coreSeverity(const std::string& s) { if(s=="CRITICAL")return Severity::CRITICAL; if(s=="HIGH")return Severity::HIGH; if(s=="MEDIUM")return Severity::MEDIUM; return Severity::LOW; }
std::string coreStatus(EmergencyStatus s) {
    switch(s){case EmergencyStatus::PENDING:return "FINDING_RESPONDER";case EmergencyStatus::DISPATCHED:return "ASSIGNED";
        case EmergencyStatus::IN_PROGRESS:return "ASSIGNED";case EmergencyStatus::COMPLETED:return "RESOLVED";case EmergencyStatus::CANCELLED:return "CANCELLED";}
    return "UNKNOWN";
}

// -----------------------------------------------------------------------
// JSON serialisation
// -----------------------------------------------------------------------
std::string emergencyJson(const ApiEmergency& e) {
    std::ostringstream o;
    o<<"{\"id\":"<<e.id<<",\"type\":"<<js(e.type)<<",\"typeLabel\":"<<js(typeDisplay(e.type))
     <<",\"severity\":"<<js(e.severity)<<",\"status\":"<<js(e.status)<<",\"locationId\":"<<e.locationId
     <<",\"zoneName\":"<<js(getLocationName(e.locationId))<<",\"lat\":"<<e.lat<<",\"lng\":"<<e.lng
     <<",\"address\":"<<js(e.address)<<",\"locationDetails\":"<<js(e.locationDetails)
     <<",\"description\":"<<js(e.description)<<",\"peopleAffected\":"<<e.peopleAffected
     <<",\"contact\":"<<js(e.contact)<<",\"assignedVehicleId\":"<<js(e.assignedVehicleId)
     <<",\"createdAt\":"<<js(e.createdAt)<<",\"updatedAt\":"<<js(e.updatedAt)
     <<",\"standbyLocationId\":"<<e.standbyLocationId<<",\"graphEtaMinutes\":"<<e.etaMinutes
     <<",\"assignedAt\":"<<js(e.assignedAt)<<",\"deadlineAt\":"<<js(e.deadlineAt)
     <<",\"rejectionCount\":"<<e.rejectionCount
     <<",\"firstAssignedVehicleId\":"<<js(e.firstAssignedVehicleId)<<"}";
    return o.str();
}

// -----------------------------------------------------------------------
// Assignment tracking
// -----------------------------------------------------------------------
void setAssignmentDeadline(ApiEmergency& e) {
    std::time_t now = std::time(nullptr);
    e.assignedAt = isoFromEpoch(now);
    e.deadlineAt = isoFromEpoch(now + ACCEPTANCE_DEADLINE_SECONDS);
}
void clearAssignmentDeadline(ApiEmergency& e) {
    e.assignedAt.clear();
    e.deadlineAt.clear();
}

// -----------------------------------------------------------------------
// Core sync and dispatch
// -----------------------------------------------------------------------
void syncCoreAssignments() {
    for(const auto& e : getAllHistoricalEmergencies()) {
        auto it=emergencies.find(e.id); if(it==emergencies.end()) continue;
        if(e.assignedVehicleId!="UNASSIGNED" && !e.assignedVehicleId.empty() && e.status!=EmergencyStatus::COMPLETED && e.status!=EmergencyStatus::CANCELLED) {
            if(it->second.firstAssignedVehicleId.empty()) {
                it->second.firstAssignedVehicleId = e.assignedVehicleId;
            }
            // Only update assignment if it changed (avoid resetting deadline)
            if(it->second.assignedVehicleId != e.assignedVehicleId) {
                it->second.assignedVehicleId=e.assignedVehicleId;
                it->second.etaMinutes=e.responseTime;
                if(it->second.status=="RECEIVED"||it->second.status=="FINDING_RESPONDER") {
                    it->second.status="ASSIGNED";
                    setAssignmentDeadline(it->second);
                }
                it->second.updatedAt=nowIso();
            } else {
                // Keep existing etaMinutes if already assigned
                if(it->second.etaMinutes==0) it->second.etaMinutes=e.responseTime;
                if(it->second.status=="RECEIVED"||it->second.status=="FINDING_RESPONDER") {
                    it->second.status="ASSIGNED";
                    if(it->second.assignedAt.empty()) setAssignmentDeadline(it->second);
                    it->second.updatedAt=nowIso();
                }
            }
        }
        if(e.status==EmergencyStatus::COMPLETED) {
            it->second.status="RESOLVED";
            clearAssignmentDeadline(it->second);
            it->second.updatedAt=nowIso();
        }
    }
}

bool isVehicleCompatible(const std::string& vTypeStr, const std::string& eTypeStr) {
    if (eTypeStr == "medical") return vTypeStr == "Ambulance";
    if (eTypeStr == "fire") return vTypeStr == "Fire Truck";
    if (eTypeStr == "road_accident") return vTypeStr == "Ambulance" || vTypeStr == "Police Vehicle";
    if (eTypeStr == "police") return vTypeStr == "Police Vehicle";
    return true; // disaster / other
}

bool reassignEmergency(ApiEmergency& e) {
    if (!e.assignedVehicleId.empty()) {
        webReleaseVehicle(e.assignedVehicleId, e.id);
    }
    auto vehicles = getWebVehicleInfos();
    std::string bestId = "";
    int minTime = 999999;

    // First attempt: pick closest available compatible responder not yet in rejectedBy
    for (const auto& v : vehicles) {
        if (!v.isAvailable) continue;
        if (e.rejectedBy.count(v.id)) continue;
        if (!isVehicleCompatible(v.type, e.type)) continue;

        RouteResult route = dijkstra(v.locationId, e.locationId);
        if (route.found) {
            if (route.travelTime < minTime) {
                minTime = route.travelTime;
                bestId = v.id;
            } else if (route.travelTime == minTime && !bestId.empty()) {
                if (v.id < bestId) {
                    bestId = v.id;
                }
            }
        }
    }

    // If no candidate was found and there are recorded rejections, check if all compatible responders have rejected.
    // If so, cycle back: clear rejectedBy so the request is offered back to the first responder again in a cycle.
    if (bestId.empty() && !e.rejectedBy.empty()) {
        int totalCompatible = 0;
        int rejectedCompatible = 0;
        for (const auto& v : vehicles) {
            if (isVehicleCompatible(v.type, e.type)) {
                totalCompatible++;
                if (e.rejectedBy.count(v.id)) {
                    rejectedCompatible++;
                }
            }
        }

        if (totalCompatible > 0 && rejectedCompatible >= totalCompatible) {
            std::cerr << "[reassign-cycle] E" << e.id << " all " << totalCompatible
                      << " compatible responders rejected. Cycling back to first responder.\n";
            e.rejectedBy.clear();
            minTime = 999999;

            // Prioritize firstAssignedVehicleId if available and compatible
            if (!e.firstAssignedVehicleId.empty()) {
                for (const auto& v : vehicles) {
                    if (v.id == e.firstAssignedVehicleId && v.isAvailable && isVehicleCompatible(v.type, e.type)) {
                        RouteResult route = dijkstra(v.locationId, e.locationId);
                        if (route.found) {
                            bestId = v.id;
                            minTime = route.travelTime;
                            break;
                        }
                    }
                }
            }

            // Fallback to Dijkstra's best available compatible responder
            if (bestId.empty()) {
                for (const auto& v : vehicles) {
                    if (!v.isAvailable) continue;
                    if (!isVehicleCompatible(v.type, e.type)) continue;

                    RouteResult route = dijkstra(v.locationId, e.locationId);
                    if (route.found) {
                        if (route.travelTime < minTime) {
                            minTime = route.travelTime;
                            bestId = v.id;
                        } else if (route.travelTime == minTime && !bestId.empty()) {
                            if (v.id < bestId) {
                                bestId = v.id;
                            }
                        }
                    }
                }
            }
        }
    }

    if (bestId.empty()) {
        e.assignedVehicleId = "";
        e.status = "FINDING_RESPONDER";
        clearAssignmentDeadline(e);
        e.updatedAt = nowIso();
        return false;
    }

    if (e.firstAssignedVehicleId.empty()) {
        e.firstAssignedVehicleId = bestId;
    }

    int travelTime = 0;
    if (webAssignVehicle(bestId, e.id, e.locationId, travelTime)) {
        e.assignedVehicleId = bestId;
        e.etaMinutes = travelTime;
        e.status = "ASSIGNED";
        setAssignmentDeadline(e);
        e.updatedAt = nowIso();
        return true;
    }
    return false;
}

// Check all ASSIGNED emergencies for expired deadlines and trigger reassignment,
// and check pending emergencies that need dispatch/cycle reassignment.
void checkAndExpireDeadlines() {
    for(auto& pair : emergencies) {
        ApiEmergency& e = pair.second;
        if(e.status == "ASSIGNED") {
            if(e.deadlineAt.empty()) continue;
            if(!isDeadlineExpired(e.deadlineAt)) continue;
            std::cerr << "[deadline-expired] E" << e.id << " assigned to " << e.assignedVehicleId << " — reassigning\n";
            if(!e.assignedVehicleId.empty()) e.rejectedBy.insert(e.assignedVehicleId);
            e.rejectionCount++;
            reassignEmergency(e);
        } else if((e.status == "FINDING_RESPONDER" || e.status == "RECEIVED") && e.assignedVehicleId.empty()) {
            reassignEmergency(e);
        }
    }
}

void tryDispatch() {
    // The existing priority queue, compatibility rules and Dijkstra-based greedy
    // responder selection remain the dispatch authority.
    processNextEmergency();
    syncCoreAssignments();
}

// -----------------------------------------------------------------------
// JSON builders
// -----------------------------------------------------------------------
std::string respondersJson() {
    auto vehicles=getWebVehicleInfos(); std::ostringstream o; o<<"["; bool first=true;
    for(const auto& v:vehicles) {
        if (!first) o << ",";
        first = false;
        std::string status="AVAILABLE", assignment=""; int standby=0;
        if(v.currentEmergencyId==-2) { status="RETURNING_TO_STANDBY"; auto st=standbyTargets.find(v.id); if(st!=standbyTargets.end())standby=st->second; }
        else if(!v.isAvailable && v.currentEmergencyId>0) {
            auto e=emergencies.find(v.currentEmergencyId); status=e==emergencies.end()?"ASSIGNED":e->second.status; assignment=std::to_string(v.currentEmergencyId);
        }
        // Zone coordinates for the responder's current simulated location
        double lat=0,lng=0;
        if(v.locationId>=1&&v.locationId<=16){lat=ZONES[v.locationId-1].lat;lng=ZONES[v.locationId-1].lng;}
        o<<"{\"id\":"<<js(v.id)<<",\"type\":"<<js(v.type)<<",\"locationId\":"<<v.locationId
         <<",\"locationName\":"<<js(getLocationName(v.locationId))<<",\"available\":"<<(v.isAvailable?"true":"false")
         <<",\"status\":"<<js(status)<<",\"currentEmergencyId\":"<<(assignment.empty()?"null":assignment)
         <<",\"standbyLocationId\":"<<standby<<",\"totalEmergencies\":"<<v.totalEmergencies
         <<",\"totalResponseTime\":"<<v.totalResponseTime<<",\"totalDistance\":"<<v.totalDistance
         <<",\"lat\":"<<lat<<",\"lng\":"<<lng<<"}";
    }
    o<<"]"; return o.str();
}
std::string emergenciesJson() {
    std::ostringstream o; o<<"["; bool first=true;
    for(auto it=emergencies.rbegin();it!=emergencies.rend();++it){if(!first)o<<",";first=false;o<<emergencyJson(it->second);} o<<"]"; return o.str();
}
std::string locationsJson() {
    std::ostringstream o; o<<"[";
    for(size_t i=0;i<sizeof(ZONES)/sizeof(ZONES[0]);++i){if(i)o<<",";const auto& z=ZONES[i];o<<"{\"id\":"<<z.id<<",\"name\":"<<js(z.name)<<",\"lat\":"<<z.lat<<",\"lng\":"<<z.lng<<"}";}
    o<<"]";return o.str();
}

// -----------------------------------------------------------------------
// .env loading
// -----------------------------------------------------------------------
void loadEnv() {
    std::ifstream f(".env"); std::string line;
    while(std::getline(f,line)){line=trim(line);if(line.empty()||line[0]=='#')continue;auto p=line.find('=');if(p==std::string::npos)continue;
        std::string k=trim(line.substr(0,p)),v=trim(line.substr(p+1));
        if(v.size()>=2&&((v.front()=='"'&&v.back()=='"')||(v.front()=='\''&&v.back()=='\'')))v=v.substr(1,v.size()-2);
        if(k=="googleMapApi")googleMapApi=v;
    }
    if(googleMapApi.empty()){const char* v=std::getenv("googleMapApi");if(v)googleMapApi=v;}
    if(googleMapApi.empty()){const char* v=std::getenv("GOOGLE_MAPS_API_KEY");if(v)googleMapApi=v;}
    if(googleMapApi=="YOUR_GOOGLE_MAPS_API_KEY"||googleMapApi=="YOUR_GOOGLE_MAPS_BROWSER_API_KEY")googleMapApi.clear();
}

// -----------------------------------------------------------------------
// HTTP helpers
// -----------------------------------------------------------------------
struct HttpRequest { std::string method,path,body; };
std::string contentType(const fs::path& p) {
    std::string ext=p.extension().string(); if(ext==".html")return "text/html; charset=utf-8";if(ext==".css")return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".svg") return "image/svg+xml";
    return "application/octet-stream";
}
void respond(int fd,int code,const std::string& type,const std::string& body) {
    std::string label=code==200?"OK":code==201?"Created":code==400?"Bad Request":code==404?"Not Found":code==409?"Conflict":code==422?"Unprocessable Entity":code==500?"Internal Server Error":"OK";
    std::ostringstream h;h<<"HTTP/1.1 "<<code<<" "<<label<<"\r\nContent-Type: "<<type<<"\r\nContent-Length: "<<body.size()<<"\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n\r\n";
    std::string all=h.str()+body;size_t sent=0;while(sent<all.size()){ssize_t n=send(fd,all.data()+sent,all.size()-sent,0);if(n<=0)break;sent+=static_cast<size_t>(n);}
}
HttpRequest readRequest(int fd) {
    std::string raw;char buf[8192];size_t headerEnd=std::string::npos;size_t expected=0;
    while((headerEnd=raw.find("\r\n\r\n"))==std::string::npos){ssize_t n=recv(fd,buf,sizeof(buf),0);if(n<=0)break;raw.append(buf,n);if(raw.size()>1024*1024)break;}
    if(headerEnd==std::string::npos)return {};
    std::string headers=raw.substr(0,headerEnd);std::istringstream hs(headers);HttpRequest r;hs>>r.method>>r.path;
    std::smatch m;if(std::regex_search(headers,m,std::regex("Content-Length:\\s*([0-9]+)",std::regex::icase)))expected=std::stoul(m[1]);
    std::string body=raw.substr(headerEnd+4);while(body.size()<expected){ssize_t n=recv(fd,buf,sizeof(buf),0);if(n<=0)break;body.append(buf,n);if(body.size()>2*1024*1024)break;}
    if (body.size() > expected) body.resize(expected);
    r.body = body;
    auto q=r.path.find('?');if(q!=std::string::npos)r.path.resize(q);
    return r;
}
std::string errorJson(const std::string& msg){return "{\"error\":"+js(msg)+"}";}

// -----------------------------------------------------------------------
// API handler
// -----------------------------------------------------------------------
std::string handleApi(const HttpRequest& r,int& code,std::string& type) {
    code=200;type="application/json; charset=utf-8";
    if(r.method=="OPTIONS")return "{}";

    // ── Health & config ──────────────────────────────────────────────
    if(r.method=="GET"&&r.path=="/api/health")
        return "{\"ok\":true,\"service\":\"C++ emergency dispatch backend\",\"simulation\":true}";
    if(r.method=="GET"&&r.path=="/api/config")
        return "{\"googleMapApi\":"+js(googleMapApi)+",\"mapsConfigured\":"+(googleMapApi.empty()?"false":"true")+",\"simulation\":true,\"acceptanceDeadlineSeconds\":"+std::to_string(ACCEPTANCE_DEADLINE_SECONDS)+"}";

    // ── Locations ────────────────────────────────────────────────────
    if(r.method=="GET"&&r.path=="/api/locations")return locationsJson();

    // ── Responders ───────────────────────────────────────────────────
    if(r.method=="GET"&&r.path=="/api/responders")return respondersJson();

    // ── Emergencies list ─────────────────────────────────────────────
    if(r.method=="GET"&&r.path=="/api/emergencies") {
        // Expire timed-out assignments before returning
        checkAndExpireDeadlines();
        return emergenciesJson();
    }

    // ── Submit new emergency ─────────────────────────────────────────
    if(r.method=="POST"&&r.path=="/api/emergencies") {
        std::string rawType=lower(trim(jsonString(r.body,"type"))), desc=trim(jsonString(r.body,"description"));
        if(rawType!="medical"&&rawType!="fire"&&rawType!="road_accident"&&rawType!="police"&&rawType!="other") {code=422;return errorJson("Choose a supported emergency type.");}
        if(desc.empty()){code=422;return errorJson("Add a short description of what is happening.");}
        double lat=jsonNumber(r.body,"lat",std::numeric_limits<double>::quiet_NaN()),lng=jsonNumber(r.body,"lng",std::numeric_limits<double>::quiet_NaN());
        bool hasCoords=std::isfinite(lat)&&std::isfinite(lng)&&std::abs(lat)<=90&&std::abs(lng)<=180;
        int loc=jsonNumber(r.body,"locationId",0); if(loc<1||loc>16) loc=hasCoords?nearestZone(lat,lng):0;
        std::string address=trim(jsonString(r.body,"address"));
        if(address.empty()&&loc>=1&&loc<=16)address=getLocationName(loc)+", Pune, Maharashtra";
        if(loc<1||loc>16){code=422;return errorJson("Location is required. Allow location access or choose a Pune zone from the manual fallback.");}
        if(!hasCoords){lat=ZONES[loc-1].lat;lng=ZONES[loc-1].lng;}
        int people=static_cast<int>(jsonNumber(r.body,"peopleAffected",1));people=std::max(1,std::min(people,1000));
        std::string details=trim(jsonString(r.body,"locationDetails")),contact=trim(jsonString(r.body,"contact"));
        std::string severity=priorityFor(rawType,desc,people);
        // Idempotency guard
        std::time_t now=std::time(nullptr);
        static std::map<int,std::time_t> createdEpoch;
        for(auto& pair:emergencies){auto& old=pair.second;auto ei=createdEpoch.find(old.id);
            if(old.type==rawType&&old.locationId==loc&&old.description==desc&&old.contact==contact&&old.status!="RESOLVED"&&old.status!="CANCELLED"&&ei!=createdEpoch.end()&&now-ei->second<=45) {
                code=200;return "{\"duplicate\":true,\"emergency\":"+emergencyJson(old)+"}";
            }
        }
        int id=0; if(!reportEmergency(loc,coreType(rawType),coreSeverity(severity),desc+(details.empty()?"":" | Location details: "+details),id)) {code=500;return errorJson("The C++ backend could not record this emergency.");}
        ApiEmergency e;e.id=id;e.type=rawType;e.severity=severity;e.status="RECEIVED";e.locationId=loc;e.peopleAffected=people;e.description=desc;e.address=address;e.locationDetails=details;e.contact=contact;e.lat=lat;e.lng=lng;e.createdAt=e.updatedAt=nowIso();emergencies[id]=e;createdEpoch[id]=now;
        tryDispatch();
        auto it=emergencies.find(id);
        if(it!=emergencies.end()) {
            if(it->second.status=="RECEIVED") it->second.status="FINDING_RESPONDER";
            if(!it->second.assignedVehicleId.empty() && it->second.firstAssignedVehicleId.empty()) {
                it->second.firstAssignedVehicleId = it->second.assignedVehicleId;
            }
            it->second.updatedAt=nowIso();
        }
        code=201;return "{\"duplicate\":false,\"emergency\":"+emergencyJson(emergencies[id])+"}";
    }

    std::smatch m;

    // ── Single emergency ─────────────────────────────────────────────
    if(r.method=="GET"&&std::regex_match(r.path,m,std::regex("/api/emergencies/([0-9]+)"))) {
        int id=std::stoi(m[1]);auto it=emergencies.find(id);if(it==emergencies.end()){code=404;return errorJson("Emergency not found in this simulation session.");}return emergencyJson(it->second);
    }

    // ── Reject assignment ─────────────────────────────────────────────
    // POST /api/emergencies/:id/reject   body: {"responderId":"A01","reason":"..."}
    if(r.method=="POST"&&std::regex_match(r.path,m,std::regex("/api/emergencies/([0-9]+)/reject"))) {
        int id=std::stoi(m[1]);
        auto it=emergencies.find(id);
        if(it==emergencies.end()){code=404;return errorJson("Emergency not found.");}
        ApiEmergency& e=it->second;
        std::string responderId=trim(jsonString(r.body,"responderId"));
        if(responderId.empty()){code=422;return errorJson("responderId is required.");}
        // Only the currently assigned responder may reject
        if(e.assignedVehicleId!=responderId){code=409;return errorJson("This responder is not currently assigned to this emergency.");}
        // Valid rejection states
        if(e.status!="ASSIGNED"&&e.status!="EN_ROUTE"){code=409;return errorJson("Only an assigned or en-route emergency can be rejected.");}
        std::string reason=trim(jsonString(r.body,"reason","No reason given"));
        std::cerr << "[reject] E" << id << " rejected by " << responderId << ": " << reason << "\n";
        e.rejectedBy.insert(responderId);
        e.rejectionCount++;
        reassignEmergency(e);
        return "{\"emergency\":"+emergencyJson(e)+",\"responders\":"+respondersJson()+"}";
    }

    // ── Responder actions ─────────────────────────────────────────────
    if(r.method=="POST"&&std::regex_match(r.path,m,std::regex("/api/responders/([A-Za-z0-9_-]+)/actions"))) {
        std::string responder=m[1],action=lower(jsonString(r.body,"action"));int id=static_cast<int>(jsonNumber(r.body,"emergencyId",0));
        auto ei=emergencies.find(id);if(ei==emergencies.end()){code=404;return errorJson("Assignment not found.");}
        ApiEmergency& e=ei->second;
        if(e.assignedVehicleId!=responder){code=409;return errorJson("This responder is not assigned to that emergency.");}
        if(action=="accept") {
            if(e.status!="ASSIGNED"){code=409;return errorJson("Only an assigned emergency can be accepted.");}
            // Validate deadline hasn't expired
            if(!e.deadlineAt.empty()&&isDeadlineExpired(e.deadlineAt)){code=409;return errorJson("The assignment deadline has expired. This emergency has been reassigned.");}
            e.status="EN_ROUTE";
            clearAssignmentDeadline(e); // deadline cancelled — responder accepted
        } else if(action=="arrived") {
            if(e.status!="EN_ROUTE"){code=409;return errorJson("Mark en route before marking arrival.");}
            e.status="AT_SCENE";
        } else if(action=="start_work") {
            if(e.status!="AT_SCENE"){code=409;return errorJson("Arrival must be recorded before work begins.");}
            e.status="IN_PROGRESS";
        } else if(action=="resolve") {
            if(e.status!="IN_PROGRESS"){code=409;return errorJson("The emergency must be in progress before it can be resolved.");}
            if(!completeActiveEmergency(id)){code=409;return errorJson("The C++ core could not complete this active assignment.");}
            e.status="RESOLVED";e.updatedAt=nowIso();
            auto hubs=getOptimizedStandbyLocations(3);auto vs=getWebVehicleInfos();int currentLoc=1;
            for(const auto& v:vs)if(v.id==responder){currentLoc=v.locationId;break;}
            int chosen=0,bestScore=std::numeric_limits<int>::max();
            for(int hub:hubs){RouteResult route=dijkstra(currentLoc,hub);if(!route.found)continue;int occupied=0;
                auto all=getWebVehicleInfos();for(const auto& v:all){if(v.id==responder)continue;if(v.locationId==hub)occupied++;auto st=standbyTargets.find(v.id);if(st!=standbyTargets.end()&&st->second==hub)occupied++;}
                int score=route.travelTime+occupied*5;if(score<bestScore){bestScore=score;chosen=hub;}}
            if(chosen>0&&webSetVehicleReturningToStandby(responder,chosen)){e.standbyLocationId=chosen;standbyTargets[responder]=chosen;}
            else { e.standbyLocationId=0; }
        } else if(action=="standby_arrived") {
            if(e.status!="RESOLVED"){code=409;return errorJson("Resolve the emergency before returning to standby.");}
            auto st=standbyTargets.find(responder);if(st==standbyTargets.end()||st->second<1){code=409;return errorJson("No standby destination is assigned to this responder.");}
            if(!webMarkStandbyArrived(responder,st->second)){code=409;return errorJson("The responder is not currently returning to standby.");}
            standbyTargets.erase(st);e.updatedAt=nowIso();tryDispatch();
        } else {code=422;return errorJson("Unsupported assignment action.");}
        e.updatedAt=nowIso();return "{\"emergency\":"+emergencyJson(e)+",\"responders\":"+respondersJson()+"}";
    }

    code=404;return errorJson("API endpoint not found.");
}

// -----------------------------------------------------------------------
// Client handler and main
// -----------------------------------------------------------------------
void handleClient(int fd) {
    timeval tv{5,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
    HttpRequest r=readRequest(fd);if(r.method.empty()){respond(fd,400,"application/json",errorJson("Malformed HTTP request."));return;}
    if(r.method=="OPTIONS"){respond(fd,200,"application/json","{}");return;}
    if(r.path.rfind("/api/",0)==0){int code=200;std::string type;std::string body=handleApi(r,code,type);respond(fd,code,type,body);return;}
    std::string path=r.path=="/"?"index.html":r.path.substr(1);
    if(path.find("..")!=std::string::npos||path.find('/')!=std::string::npos){respond(fd,404,"text/plain","Not found");return;}
    fs::path file=WEB_DIR/path;std::ifstream in(file,std::ios::binary);if(!in){respond(fd,404,"text/plain; charset=utf-8","File not found. Start the server from the project directory.");return;}
    std::ostringstream content;content<<in.rdbuf();respond(fd,200,contentType(file),content.str());
}

int main() {
    loadEnv();initGraph();initEmergencySystem();
    const char* hp=std::getenv("WEB_HOST");std::string host=hp?hp:"127.0.0.1";
    int port=8765; if(const char* pp=std::getenv("WEB_PORT")){try{port=std::stoi(pp);}catch(...){}}
    int server=socket(AF_INET,SOCK_STREAM,0);if(server<0){std::cerr<<"socket() failed\n";return 1;}
    int yes=1;setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(static_cast<uint16_t>(port));
    if(inet_pton(AF_INET,host.c_str(),&addr.sin_addr)!=1){std::cerr<<"WEB_HOST must be an IPv4 address (default 127.0.0.1).\n";close(server);return 1;}
    if(bind(server,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))<0||listen(server,32)<0){std::cerr<<"Could not bind "<<host<<":"<<port<<". Is the port already in use?\n";close(server);return 1;}
    std::cout<<"Emergency Response Web API ready at http://"<<host<<":"<<port<<"\n";
    std::cout<<"Mode: simulated dispatch; no real emergency services are contacted.\n";
    std::cout<<"Acceptance deadline: "<<ACCEPTANCE_DEADLINE_SECONDS<<" seconds per assignment attempt.\n";
    std::cout<<(googleMapApi.empty()?"Google Maps is not configured. Add googleMapApi to .env to enable interactive maps.\n":"Google Maps browser key loaded; restrict it by HTTP referrer and API.\n");
    while(true){sockaddr_in client{};socklen_t len=sizeof(client);int fd=accept(server,reinterpret_cast<sockaddr*>(&client),&len);if(fd<0)continue;handleClient(fd);close(fd);}
    close(server);return 0;
}
