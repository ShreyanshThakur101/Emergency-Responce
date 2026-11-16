#!/usr/bin/env python3
"""Black-box integration test for the C++ web adapter. Uses simulated units only."""
import json
import os
import socket
import subprocess
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent
PORT = 8766
BASE = f"http://127.0.0.1:{PORT}"

def request(path, method="GET", data=None):
    raw = None if data is None else json.dumps(data).encode()
    req = urllib.request.Request(BASE + path, data=raw, method=method,
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=5) as response:
            raw_body = response.read().decode()
            if "application/json" in response.headers.get("Content-Type", ""):
                return response.status, json.loads(raw_body)
            return response.status, raw_body
    except urllib.error.HTTPError as e:
        raw_body = e.read().decode()
        try: return e.code, json.loads(raw_body)
        except json.JSONDecodeError: return e.code, raw_body

def wait_for_server(proc):
    for _ in range(80):
        if proc.poll() is not None:
            raise RuntimeError("The web server exited before becoming ready.")
        try:
            status, data = request("/api/health")
            if status == 200 and data.get("ok"):
                return
        except Exception:
            time.sleep(.15)
    raise RuntimeError("Timed out waiting for the test server.")

def main():
    # Build a fresh executable so the test is reproducible from the project files.
    binary = ROOT / "emergency_web_test"
    cmd = ["g++", "-std=c++17", "-O2", "web_api.cpp", "ds cp/emergency_core.cpp",
           "ds cp/graph_algorithms.cpp", "ds cp/emergency_system.cpp", "-o", str(binary)]
    subprocess.run(cmd, cwd=ROOT, check=True)
    env = os.environ.copy(); env["WEB_HOST"] = "127.0.0.1"; env["WEB_PORT"] = str(PORT)
    proc = subprocess.Popen([str(binary)], cwd=ROOT, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        wait_for_server(proc)
        status, health = request("/api/health")
        assert status == 200 and health["simulation"] is True
        status, locations = request("/api/locations")
        assert status == 200 and len(locations) == 16
        status, html = request("/")
        assert status == 200 and "Citizen portal" in html

        reports = []
        for n in range(3):
            report = {
                "type": "medical", "description": f"Integration test medical request {n} - breathing difficulty",
                "address": "Deccan, Pune, Maharashtra", "locationId": 2,
                "peopleAffected": 1, "contact": f"TEST-{n}"
            }
            # First request represents GPS/map coordinates; the second exercises
            # the manual-zone fallback when no device coordinates are supplied.
            if n != 1:
                report.update({"lat": 18.5164, "lng": 73.8417})
            status, payload = request("/api/emergencies", "POST", report)
            assert status == 201, payload
            reports.append(payload["emergency"])
            assert reports[-1]["status"] == "ASSIGNED", reports[-1]
        # The three simulated ambulances are busy, so another medical request must remain queued.
        status, pending_payload = request("/api/emergencies", "POST", {
            "type": "medical", "description": "Integration test fourth medical request pending availability",
            "address": "Kothrud, Pune, Maharashtra", "locationId": 10,
            "lat": 18.5074, "lng": 73.8077, "peopleAffected": 1, "contact": "TEST-PENDING"
        })
        assert status == 201, pending_payload
        pending = pending_payload["emergency"]
        assert pending["status"] == "FINDING_RESPONDER", pending

        # Repeated submission in the retry window must return the existing request.
        status, duplicate = request("/api/emergencies", "POST", {
            "type": "medical", "description": "Integration test fourth medical request pending availability",
            "address": "Kothrud, Pune, Maharashtra", "locationId": 10,
            "lat": 18.5074, "lng": 73.8077, "peopleAffected": 1, "contact": "TEST-PENDING"
        })
        assert status == 200 and duplicate.get("duplicate") is True

        first = reports[0]; responder = first["assignedVehicleId"]; eid = first["id"]
        for action, expected in [("accept", "EN_ROUTE"), ("arrived", "AT_SCENE"),
                                 ("start_work", "IN_PROGRESS"), ("resolve", "RESOLVED")]:
            status, result = request(f"/api/responders/{responder}/actions", "POST", {"action": action, "emergencyId": eid})
            assert status == 200, result
            assert result["emergency"]["status"] == expected, result
        resolved = result["emergency"]
        assert resolved["standbyLocationId"] > 0, resolved
        status, result = request(f"/api/responders/{responder}/actions", "POST", {"action": "standby_arrived", "emergencyId": eid})
        assert status == 200, result
        status, updated = request("/api/emergencies/" + str(pending["id"]))
        assert status == 200 and updated["status"] == "ASSIGNED", updated
        status, fleet = request("/api/responders")
        assert status == 200 and any(r["id"] == responder and r["status"] == "ASSIGNED" for r in fleet), fleet
        print("PASS: health, locations, static UI, coordinate and manual-zone reports, automatic dispatch,")
        print("      responder status transitions, standby selection/arrival, and queued auto-dispatch.")
    finally:
        proc.terminate()
        try: proc.wait(timeout=4)
        except subprocess.TimeoutExpired: proc.kill()
        try: binary.unlink()
        except OSError: pass

if __name__ == "__main__":
    main()
