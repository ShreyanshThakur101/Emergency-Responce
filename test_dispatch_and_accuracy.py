#!/usr/bin/env python3
"""
Pulse Response - Comprehensive Automated Test Suite
Testing GPS Accuracy, Map-First Integration, and Reassignment Pipeline.
"""

import urllib.request
import json
import time
import sys

BASE_URL = "http://127.0.0.1:8765"

def get(path):
    req = urllib.request.Request(f"{BASE_URL}{path}")
    with urllib.request.urlopen(req) as resp:
        return json.loads(resp.read().decode('utf-8'))

def post(path, data):
    body = json.dumps(data).encode('utf-8')
    req = urllib.request.Request(f"{BASE_URL}{path}", data=body, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req) as resp:
        return json.loads(resp.read().decode('utf-8'))

def test_config():
    print("\n--- TEST: System Configuration & Deadline ---")
    cfg = get("/api/config")
    assert cfg["mapsConfigured"] == True, "Google Maps should be configured"
    assert "googleMapApi" in cfg and cfg["googleMapApi"].startswith("AIza"), "API key present"
    assert cfg.get("acceptanceDeadlineSeconds") == 60, "Acceptance deadline must be 60 seconds"
    print("✓ /api/config returned valid key and 60-second deadline policy.")

def test_locations_and_kondhwa():
    print("\n--- TEST: Zone Geometries & Kondhwa Coordinates ---")
    locs = get("/api/locations")
    assert len(locs) == 16, f"Expected 16 Pune zones, got {len(locs)}"
    kondhwa = next(z for z in locs if z["id"] == 16)
    assert kondhwa["name"] == "Kondhwa"
    assert abs(kondhwa["lat"] - 18.4635) < 0.001
    assert abs(kondhwa["lng"] - 73.8710) < 0.001
    
    fc_road = next(z for z in locs if z["id"] == 6)
    assert fc_road["name"] == "FC Road"
    assert abs(fc_road["lat"] - 18.5236) < 0.001
    assert abs(fc_road["lng"] - 73.8417) < 0.001
    print(f"✓ Verified Zone 16 (Kondhwa: {kondhwa['lat']}, {kondhwa['lng']}) is separate from Zone 6 (FC Road: {fc_road['lat']}, {fc_road['lng']})")

def test_responders_have_coords():
    print("\n--- TEST: Responders Return Geographic Coordinates ---")
    responders = get("/api/responders")
    assert len(responders) == 8, f"Expected 8 fleet units, got {len(responders)}"
    for r in responders:
        assert "lat" in r and "lng" in r, f"Responder {r['id']} missing coordinates"
        assert -90 <= r["lat"] <= 90 and -180 <= r["lng"] <= 180, "Invalid coordinate bounds"
        assert r["locationId"] >= 1 and r["locationId"] <= 16
    print(f"✓ All {len(responders)} responders provide valid mapped geographic coordinates.")

def test_emergency_submission_kondhwa():
    print("\n--- TEST: Emergency Submission at Kondhwa (No Silent FC Road Fallback) ---")
    # Citizen located in Kondhwa reports an emergency with exact Kondhwa coordinates
    payload = {
        "type": "medical",
        "description": "Severe shortness of breath at Kondhwa residence",
        "locationId": 16,
        "lat": 18.4635,
        "lng": 73.8710,
        "address": "Kondhwa Budruk, Pune, Maharashtra",
        "peopleAffected": 1
    }
    res = post("/api/emergencies", payload)
    e = res["emergency"]
    
    assert e["locationId"] == 16, f"Expected locationId 16 (Kondhwa), got {e['locationId']}"
    assert abs(e["lat"] - 18.4635) < 0.001, "Confirmed coordinates must match Kondhwa latitude"
    assert abs(e["lng"] - 73.8710) < 0.001, "Confirmed coordinates must match Kondhwa longitude"
    assert "Kondhwa" in e["address"], "Address must specify Kondhwa"
    
    # Verify automatic dispatch assigned closest available ambulance (A03 is stationed at Kondhwa Zone 16!)
    print(f"✓ Emergency E{e['id']} created at Kondhwa ({e['lat']}, {e['lng']}). Assigned unit: {e['assignedVehicleId']}")
    assert e["assignedVehicleId"] == "A03", f"Expected nearest ambulance A03 (stationed in Kondhwa), got {e['assignedVehicleId']}"
    assert e["status"] == "ASSIGNED"
    assert "deadlineAt" in e and len(e["deadlineAt"]) > 0, "Emergency must have an active 60s deadline"
    print(f"✓ Nearest eligible responder A03 automatically assigned with 60s deadline {e['deadlineAt']}.")
    return e

def test_rejection_and_reassignment(emergency):
    eid = emergency["id"]
    assigned_v = emergency["assignedVehicleId"]
    print(f"\n--- TEST: Responder {assigned_v} Rejects E{eid} -> Automatic Reassignment ---")
    
    # Responder rejects assignment
    reject_res = post(f"/api/emergencies/{eid}/reject", {
        "responderId": assigned_v,
        "reason": "Unit undergoing unexpected maintenance"
    })
    
    updated_e = reject_res["emergency"]
    new_v = updated_e["assignedVehicleId"]
    
    assert new_v != assigned_v, f"Reassigned vehicle ({new_v}) must differ from rejected vehicle ({assigned_v})"
    assert new_v.startswith("A"), f"Reassigned vehicle must be a compatible Ambulance, got {new_v}"
    assert updated_e["rejectionCount"] >= 1, "Rejection count must be incremented"
    print(f"✓ Rejection processed: E{eid} automatically reassigned from {assigned_v} to {new_v} (Rejections: {updated_e['rejectionCount']})")
    return updated_e

def test_acceptance_and_workflow(emergency):
    eid = emergency["id"]
    v = emergency["assignedVehicleId"]
    print(f"\n--- TEST: Unit {v} Accepts E{eid} & Full Workflow to Resolution ---")
    
    # 1. Accept
    res_accept = post(f"/api/responders/{v}/actions", {"action": "accept", "emergencyId": eid})
    e = res_accept["emergency"]
    assert e["status"] == "EN_ROUTE", f"Expected EN_ROUTE after accept, got {e['status']}"
    assert e["deadlineAt"] == "", "Deadline must be cleared once accepted"
    print(f"✓ Unit {v} accepted assignment -> status EN_ROUTE, deadline timer cancelled.")

    # 2. Mark arrived
    res_arrived = post(f"/api/responders/{v}/actions", {"action": "arrived", "emergencyId": eid})
    e = res_arrived["emergency"]
    assert e["status"] == "AT_SCENE", f"Expected AT_SCENE, got {e['status']}"
    print(f"✓ Unit {v} arrived at scene -> status AT_SCENE.")

    # 3. Start work
    res_work = post(f"/api/responders/{v}/actions", {"action": "start_work", "emergencyId": eid})
    e = res_work["emergency"]
    assert e["status"] == "IN_PROGRESS", f"Expected IN_PROGRESS, got {e['status']}"
    print(f"✓ Unit {v} started work -> status IN_PROGRESS.")

    # 4. Resolve
    res_resolve = post(f"/api/responders/{v}/actions", {"action": "resolve", "emergencyId": eid})
    e = res_resolve["emergency"]
    assert e["status"] == "RESOLVED", f"Expected RESOLVED, got {e['status']}"
    print(f"✓ Unit {v} resolved emergency -> status RESOLVED, standby hub assigned: Zone {e['standbyLocationId']}.")

    # 5. Standby arrival
    res_standby = post(f"/api/responders/{v}/actions", {"action": "standby_arrived", "emergencyId": eid})
    responders = res_standby["responders"]
    unit = next(r for r in responders if r["id"] == v)
    assert unit["status"] == "AVAILABLE", f"Unit should be AVAILABLE after standby arrival, got {unit['status']}"
    print(f"✓ Unit {v} confirmed arrival at standby -> status restored to AVAILABLE.")

def run_all():
    print("==============================================================")
    print("   PULSE RESPONSE: MAP-FIRST & LOCATION VERIFICATION SUITE    ")
    print("==============================================================")
    test_config()
    test_locations_and_kondhwa()
    test_responders_have_coords()
    e = test_emergency_submission_kondhwa()
    e2 = test_rejection_and_reassignment(e)
    test_acceptance_and_workflow(e2)
    print("\n==============================================================")
    print("   ALL INTEGRATION TESTS PASSED (100% SUCCESS)                ")
    print("==============================================================")

if __name__ == "__main__":
    run_all()
