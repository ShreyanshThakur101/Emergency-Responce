// Pulse Response - Vercel Serverless API Handler
// Provides complete backend API compatibility for Vercel deployment.

const LOCATIONS = [
  { id: 1, name: "Shivajinagar", lat: 18.5308, lng: 73.8475 },
  { id: 2, name: "Deccan", lat: 18.5164, lng: 73.8417 },
  { id: 3, name: "Yerwada", lat: 18.5515, lng: 73.8796 },
  { id: 4, name: "Viman Nagar", lat: 18.5679, lng: 73.9143 },
  { id: 5, name: "Baner", lat: 18.5590, lng: 73.7868 },
  { id: 6, name: "FC Road", lat: 18.5236, lng: 73.8417 },
  { id: 7, name: "Kalyani Nagar", lat: 18.5480, lng: 73.9032 },
  { id: 8, name: "Kharadi", lat: 18.5515, lng: 73.9348 },
  { id: 9, name: "Wakad", lat: 18.5975, lng: 73.7639 },
  { id: 10, name: "Kothrud", lat: 18.5074, lng: 73.8077 },
  { id: 11, name: "Swargate", lat: 18.5018, lng: 73.8636 },
  { id: 12, name: "Hadapsar", lat: 18.5089, lng: 73.9260 },
  { id: 13, name: "Hinjewadi", lat: 18.5913, lng: 73.7389 },
  { id: 14, name: "Warje", lat: 18.4825, lng: 73.8021 },
  { id: 15, name: "Camp", lat: 18.5134, lng: 73.8796 },
  { id: 16, name: "Kondhwa", lat: 18.4635, lng: 73.8710 }
];

let responders = [
  { id: "A01", type: "Ambulance", locationId: 6, locationName: "FC Road", available: true, status: "AVAILABLE", currentEmergencyId: null, standbyLocationId: 0, totalEmergencies: 4, totalResponseTime: 18, totalDistance: 14.6, lat: 18.5236, lng: 73.8417 },
  { id: "A02", type: "Ambulance", locationId: 6, locationName: "FC Road", available: true, status: "AVAILABLE", currentEmergencyId: null, standbyLocationId: 0, totalEmergencies: 4, totalResponseTime: 20, totalDistance: 16.2, lat: 18.5236, lng: 73.8417 },
  { id: "A03", type: "Ambulance", locationId: 16, locationName: "Kondhwa", available: true, status: "AVAILABLE", currentEmergencyId: null, standbyLocationId: 0, totalEmergencies: 2, totalResponseTime: 14, totalDistance: 11.2, lat: 18.4635, lng: 73.8710 },
  { id: "F01", type: "Fire Truck", locationId: 8, locationName: "Kharadi", available: true, status: "AVAILABLE", currentEmergencyId: null, standbyLocationId: 0, totalEmergencies: 2, totalResponseTime: 11, totalDistance: 8.8, lat: 18.5515, lng: 73.9348 },
  { id: "F02", type: "Fire Truck", locationId: 10, locationName: "Kothrud", available: true, status: "AVAILABLE", currentEmergencyId: null, standbyLocationId: 0, totalEmergencies: 3, totalResponseTime: 21, totalDistance: 16.8, lat: 18.5074, lng: 73.8077 },
  { id: "P01", type: "Police Vehicle", locationId: 7, locationName: "Kalyani Nagar", available: true, status: "AVAILABLE", currentEmergencyId: null, standbyLocationId: 0, totalEmergencies: 2, totalResponseTime: 10, totalDistance: 8.0, lat: 18.5480, lng: 73.9032 },
  { id: "P02", type: "Police Vehicle", locationId: 11, locationName: "Swargate", available: true, status: "AVAILABLE", currentEmergencyId: null, standbyLocationId: 0, totalEmergencies: 4, totalResponseTime: 23, totalDistance: 18.4, lat: 18.5018, lng: 73.8636 },
  { id: "P03", type: "Police Vehicle", locationId: 13, locationName: "Hinjewadi", available: true, status: "AVAILABLE", currentEmergencyId: null, standbyLocationId: 0, totalEmergencies: 3, totalResponseTime: 17, totalDistance: 13.6, lat: 18.5913, lng: 73.7389 }
];

let emergencies = [];
let nextEmergencyId = 201;

function isCompatible(vehicleType, emergencyType) {
  const et = (emergencyType || '').toUpperCase();
  const vt = (vehicleType || '').toLowerCase();
  if (vt.includes('ambulance')) {
    return et === 'MEDICAL' || et === 'ACCIDENT';
  }
  if (vt.includes('fire')) {
    return et === 'FIRE' || et === 'HAZMAT' || et === 'COLLAPSE' || et === 'ACCIDENT';
  }
  if (vt.includes('police')) {
    return et === 'CRIME' || et === 'TRAFFIC' || et === 'ACCIDENT' || et === 'NATURAL_DISASTER';
  }
  return false;
}

function calcDist(lat1, lng1, lat2, lng2) {
  const dlat = (lat1 - lat2) * 111.0;
  const dlng = (lng1 - lng2) * 105.0;
  return Math.sqrt(dlat * dlat + dlng * dlng);
}

function assignResponder(emergency) {
  const loc = LOCATIONS.find(l => l.id === Number(emergency.locationId)) || LOCATIONS[0];
  const eLat = emergency.lat || loc.lat;
  const eLng = emergency.lng || loc.lng;

  const compatibleUnits = responders.filter(r => isCompatible(r.type, emergency.type));
  const rejectedSet = new Set(emergency.rejectedBy || []);

  const candidates = compatibleUnits.filter(r => r.available && !rejectedSet.has(r.id));

  if (candidates.length === 0) {
    // Check if cyclic restart is needed
    if (compatibleUnits.length > 0 && emergency.rejectedBy && emergency.rejectedBy.length >= compatibleUnits.length) {
      emergency.rejectedBy = [];
      const targetId = emergency.firstAssignedVehicleId || compatibleUnits[0].id;
      const targetUnit = responders.find(r => r.id === targetId);
      if (targetUnit) {
        targetUnit.available = false;
        targetUnit.status = 'ASSIGNED';
        targetUnit.currentEmergencyId = emergency.id;
        emergency.assignedVehicleId = targetUnit.id;
        emergency.status = 'ASSIGNED';
        emergency.assignedAt = new Date().toISOString();
        emergency.deadlineAt = new Date(Date.now() + 60000).toISOString();
        return;
      }
    }
    emergency.status = 'FINDING_RESPONDER';
    emergency.assignedVehicleId = '';
    emergency.deadlineAt = '';
    return;
  }

  // Sort by distance
  candidates.sort((a, b) => {
    const da = calcDist(a.lat, a.lng, eLat, eLng);
    const db = calcDist(b.lat, b.lng, eLat, eLng);
    return da - db;
  });

  const best = candidates[0];
  best.available = false;
  best.status = 'ASSIGNED';
  best.currentEmergencyId = emergency.id;

  emergency.assignedVehicleId = best.id;
  if (!emergency.firstAssignedVehicleId) {
    emergency.firstAssignedVehicleId = best.id;
  }
  emergency.status = 'ASSIGNED';
  emergency.assignedAt = new Date().toISOString();
  emergency.deadlineAt = new Date(Date.now() + 60000).toISOString();
}

function checkDeadlines() {
  const now = Date.now();
  for (const e of emergencies) {
    if (e.status === 'ASSIGNED' && e.deadlineAt) {
      const exp = new Date(e.deadlineAt).getTime();
      if (now >= exp) {
        // Expired -> auto-reject and reassign
        const rejectedId = e.assignedVehicleId;
        const resp = responders.find(r => r.id === rejectedId);
        if (resp) {
          resp.available = true;
          resp.status = 'AVAILABLE';
          resp.currentEmergencyId = null;
        }
        if (!e.rejectedBy) e.rejectedBy = [];
        if (rejectedId && !e.rejectedBy.includes(rejectedId)) {
          e.rejectedBy.push(rejectedId);
        }
        e.rejectionCount = (e.rejectionCount || 0) + 1;
        assignResponder(e);
      }
    }
  }
}

module.exports = async (req, res) => {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

  if (req.method === 'OPTIONS') {
    return res.status(200).json({});
  }

  // Parse path
  const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  let pathname = url.pathname;
  if (!pathname.startsWith('/api')) {
    pathname = '/api' + pathname;
  }

  checkDeadlines();

  // Route: /api/health
  if (pathname === '/api/health') {
    return res.status(200).json({
      ok: true,
      service: "C++ emergency dispatch simulation (Vercel Serverless API)",
      simulation: true
    });
  }

  // Route: /api/config
  if (pathname === '/api/config') {
    const key = process.env.googleMapApi || process.env.GOOGLE_MAPS_API_KEY || "";
    return res.status(200).json({
      googleMapApi: key,
      mapsConfigured: Boolean(key),
      simulation: true,
      acceptanceDeadlineSeconds: 60
    });
  }

  // Route: /api/locations
  if (pathname === '/api/locations') {
    return res.status(200).json(LOCATIONS);
  }

  // Route: /api/responders
  if (pathname === '/api/responders') {
    return res.status(200).json(responders);
  }

  // Route: /api/emergencies (GET / POST)
  if (pathname === '/api/emergencies') {
    if (req.method === 'GET') {
      return res.status(200).json(emergencies);
    }
    if (req.method === 'POST') {
      let body = req.body;
      if (typeof body === 'string') {
        try { body = JSON.parse(body); } catch(err) { body = {}; }
      }
      body = body || {};

      const locId = Number(body.locationId) || 1;
      const zone = LOCATIONS.find(l => l.id === locId) || LOCATIONS[0];

      const newEmergency = {
        id: nextEmergencyId++,
        type: (body.type || 'MEDICAL').toUpperCase(),
        severity: (body.severity || 'HIGH').toUpperCase(),
        status: 'RECEIVED',
        locationId: locId,
        peopleAffected: Number(body.peopleAffected) || 1,
        description: body.description || '',
        address: body.address || zone.name,
        locationDetails: body.locationDetails || '',
        contact: body.contact || '',
        lat: Number(body.lat) || zone.lat,
        lng: Number(body.lng) || zone.lng,
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        rejectedBy: [],
        rejectionCount: 0,
        firstAssignedVehicleId: ''
      };

      assignResponder(newEmergency);
      emergencies.unshift(newEmergency);

      return res.status(201).json({
        duplicate: false,
        emergency: newEmergency
      });
    }
  }

  // Route: /api/emergencies/:id/reject
  const rejectMatch = pathname.match(/^\/api\/emergencies\/([0-9]+)\/reject$/);
  if (rejectMatch && req.method === 'POST') {
    const id = Number(rejectMatch[1]);
    const emergency = emergencies.find(e => e.id === id);
    if (!emergency) {
      return res.status(404).json({ error: "Emergency not found." });
    }

    let body = req.body;
    if (typeof body === 'string') {
      try { body = JSON.parse(body); } catch(e) {}
    }
    const responderId = (body && body.responderId) ? body.responderId.trim() : '';

    const currentResp = responders.find(r => r.id === emergency.assignedVehicleId);
    if (currentResp) {
      currentResp.available = true;
      currentResp.status = 'AVAILABLE';
      currentResp.currentEmergencyId = null;
    }

    if (!emergency.rejectedBy) emergency.rejectedBy = [];
    if (responderId && !emergency.rejectedBy.includes(responderId)) {
      emergency.rejectedBy.push(responderId);
    }
    emergency.rejectionCount = (emergency.rejectionCount || 0) + 1;

    assignResponder(emergency);

    return res.status(200).json({
      emergency,
      responders
    });
  }

  // Route: /api/emergencies/:id
  const singleMatch = pathname.match(/^\/api\/emergencies\/([0-9]+)$/);
  if (singleMatch && req.method === 'GET') {
    const id = Number(singleMatch[1]);
    const emergency = emergencies.find(e => e.id === id);
    if (!emergency) {
      return res.status(404).json({ error: "Emergency not found." });
    }
    return res.status(200).json(emergency);
  }

  // Route: /api/responders/:id/actions
  const actionMatch = pathname.match(/^\/api\/responders\/([A-Za-z0-9_-]+)\/actions$/);
  if (actionMatch && req.method === 'POST') {
    const responderId = actionMatch[1];
    let body = req.body;
    if (typeof body === 'string') {
      try { body = JSON.parse(body); } catch(e) {}
    }
    const action = ((body && body.action) || '').toLowerCase();
    const emergencyId = Number(body && body.emergencyId);

    const responder = responders.find(r => r.id === responderId);
    const emergency = emergencies.find(e => e.id === emergencyId);

    if (!responder || !emergency) {
      return res.status(404).json({ error: "Assignment or responder not found." });
    }

    if (action === 'accept') {
      emergency.status = 'EN_ROUTE';
      emergency.deadlineAt = '';
      responder.status = 'ASSIGNED';
    } else if (action === 'arrived') {
      emergency.status = 'AT_SCENE';
    } else if (action === 'start_work') {
      emergency.status = 'IN_PROGRESS';
    } else if (action === 'resolve') {
      emergency.status = 'RESOLVED';
      emergency.updatedAt = new Date().toISOString();
      responder.status = 'AVAILABLE';
      responder.available = true;
      responder.currentEmergencyId = null;
      responder.totalEmergencies = (responder.totalEmergencies || 0) + 1;
    }

    return res.status(200).json({
      emergency,
      responders
    });
  }

  return res.status(404).json({ error: "API endpoint not found." });
};
