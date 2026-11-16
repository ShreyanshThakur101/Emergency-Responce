#!/usr/bin/env python3
import urllib.request
import json
import sys

BASE_URL = "http://127.0.0.1:8765"

def post(path, data):
    req = urllib.request.Request(f"{BASE_URL}{path}", data=json.dumps(data).encode("utf-8"), headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req) as resp:
        return json.loads(resp.read().decode("utf-8"))

def get(path):
    with urllib.request.urlopen(f"{BASE_URL}{path}") as resp:
        return json.loads(resp.read().decode("utf-8"))

def test_rejection_cycle_medical():
    print("\n--- TEST: Medical Emergency 3-Unit Rejection Cycle (A03 -> A01 -> A02 -> A03) ---")
    res = post("/api/emergencies", {
        "type": "medical",
        "description": "Shortness of breath in Kondhwa",
        "locationId": 16,
        "lat": 18.4635,
        "lng": 73.8710,
        "address": "Kondhwa, Pune",
        "peopleAffected": 1
    })
    e = res["emergency"]
    eid = e["id"]
    init_v = e["assignedVehicleId"]
    print(f"Created Emergency E{eid}. Initial assigned unit: {init_v}")
    assert init_v == "A03", f"Expected A03 initially, got {init_v}"
    assert e["status"] == "ASSIGNED"

    # 1st cycle: A03 -> A01 -> A02 -> A03
    r1 = post(f"/api/emergencies/{eid}/reject", {"responderId": "A03", "reason": "Busy 1"})["emergency"]
    print(f"Attempt 1: A03 rejected -> assigned to {r1['assignedVehicleId']} (rejections: {r1['rejectionCount']})")
    assert r1["assignedVehicleId"] == "A01"
    assert r1["status"] == "ASSIGNED"
    assert r1["rejectionCount"] == 1

    r2 = post(f"/api/emergencies/{eid}/reject", {"responderId": "A01", "reason": "Busy 2"})["emergency"]
    print(f"Attempt 2: A01 rejected -> assigned to {r2['assignedVehicleId']} (rejections: {r2['rejectionCount']})")
    assert r2["assignedVehicleId"] == "A02"
    assert r2["status"] == "ASSIGNED"
    assert r2["rejectionCount"] == 2

    # All 3 have rejected! Cycle must loop back to A03!
    r3 = post(f"/api/emergencies/{eid}/reject", {"responderId": "A02", "reason": "Busy 3"})["emergency"]
    print(f"Attempt 3: A02 rejected (ALL rejected!) -> assigned to {r3['assignedVehicleId']} (rejections: {r3['rejectionCount']})")
    assert r3["assignedVehicleId"] == "A03", f"Expected cycle back to A03, got {r3['assignedVehicleId']}"
    assert r3["status"] == "ASSIGNED"
    assert r3["rejectionCount"] == 3
    assert len(r3["deadlineAt"]) > 0, "Deadline must be reset on cycle"
    print("✓ Successfully completed Cycle 1: Request sent back to first responder A03!")

    # 2nd cycle: A03 -> A01 -> A02 -> A03
    r4 = post(f"/api/emergencies/{eid}/reject", {"responderId": "A03", "reason": "Busy 4"})["emergency"]
    print(f"Attempt 4: A03 rejected again -> assigned to {r4['assignedVehicleId']} (rejections: {r4['rejectionCount']})")
    assert r4["assignedVehicleId"] == "A01"

    r5 = post(f"/api/emergencies/{eid}/reject", {"responderId": "A01", "reason": "Busy 5"})["emergency"]
    print(f"Attempt 5: A01 rejected again -> assigned to {r5['assignedVehicleId']} (rejections: {r5['rejectionCount']})")
    assert r5["assignedVehicleId"] == "A02"

    r6 = post(f"/api/emergencies/{eid}/reject", {"responderId": "A02", "reason": "Busy 6"})["emergency"]
    print(f"Attempt 6: A02 rejected again (ALL rejected 2nd time!) -> assigned to {r6['assignedVehicleId']} (rejections: {r6['rejectionCount']})")
    assert r6["assignedVehicleId"] == "A03", f"Expected cycle back to A03 on 2nd cycle, got {r6['assignedVehicleId']}"
    print("✓ Successfully completed Cycle 2: Request cycled back to first responder A03 again!")

    # Complete the workflow
    post("/api/responders/A03/actions", {"action": "accept", "emergencyId": eid})
    post("/api/responders/A03/actions", {"action": "arrived", "emergencyId": eid})
    post("/api/responders/A03/actions", {"action": "start_work", "emergencyId": eid})
    res_resolve = post("/api/responders/A03/actions", {"action": "resolve", "emergencyId": eid})
    assert res_resolve["emergency"]["status"] == "RESOLVED"
    post("/api/responders/A03/actions", {"action": "standby_arrived", "emergencyId": eid})
    print("✓ Emergency resolved and unit returned to standby.")

def test_rejection_cycle_fire():
    print("\n--- TEST: Fire Emergency 2-Unit Rejection Cycle (F01 <-> F02) ---")
    res = post("/api/emergencies", {
        "type": "fire",
        "description": "Kitchen fire in Kharadi",
        "locationId": 8,
        "lat": 18.5515,
        "lng": 73.9348,
        "address": "Kharadi, Pune",
        "peopleAffected": 2
    })
    e = res["emergency"]
    eid = e["id"]
    print(f"Created Fire Emergency E{eid}. Initial assigned unit: {e['assignedVehicleId']}")
    assert e["assignedVehicleId"] == "F01"

    # F01 rejects -> F02
    r1 = post(f"/api/emergencies/{eid}/reject", {"responderId": "F01", "reason": "At station"})["emergency"]
    assert r1["assignedVehicleId"] == "F02"
    print(f"F01 rejected -> assigned to {r1['assignedVehicleId']}")

    # F02 rejects -> all 2 fire units rejected! Cycles back to F01!
    r2 = post(f"/api/emergencies/{eid}/reject", {"responderId": "F02", "reason": "Maintenance"})["emergency"]
    assert r2["assignedVehicleId"] == "F01", f"Expected cycle back to F01, got {r2['assignedVehicleId']}"
    assert r2["status"] == "ASSIGNED"
    assert r2["rejectionCount"] == 2
    print(f"F02 rejected (ALL rejected!) -> Cycled back to {r2['assignedVehicleId']}!")

    # Clean up
    post("/api/responders/F01/actions", {"action": "accept", "emergencyId": eid})
    post("/api/responders/F01/actions", {"action": "arrived", "emergencyId": eid})
    post("/api/responders/F01/actions", {"action": "start_work", "emergencyId": eid})
    post("/api/responders/F01/actions", {"action": "resolve", "emergencyId": eid})
    post("/api/responders/F01/actions", {"action": "standby_arrived", "emergencyId": eid})
    print("✓ Fire emergency completed successfully.")

if __name__ == "__main__":
    test_rejection_cycle_medical()
    test_rejection_cycle_fire()
    print("\n==================================================")
    print("  ALL REJECTION CYCLE TESTS PASSED (100% SUCCESS) ")
    print("==================================================")
