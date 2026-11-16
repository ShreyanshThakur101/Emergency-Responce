(() => {
  'use strict';
  const $ = (s, root=document) => root.querySelector(s);
  const $$ = (s, root=document) => [...root.querySelectorAll(s)];

  const state = {
    config: null,
    locations: [],
    emergencies: [],
    responders: [],
    activePortal: 'citizen',
    // Citizen incident location
    rawGps: null,
    lat: null,
    lng: null,
    accuracy: null,
    locationSource: 'Not determined',
    locationId: null,
    address: '',
    // Maps & instances
    map: null,
    marker: null,
    responderMap: null,
    geocoder: null,
    // Marker registries for Responder Map
    responderMarkers: new Map(),
    emergencyMarkers: new Map(),
    standbyMarkers: new Map(),
    // UI selections & filters
    selectedResponder: '',
    selectedEmergency: null,
    responderFilter: 'all',
    responderSearch: '',
    layers: { responders: true, emergencies: true, standby: true },
    // Timers & errors
    lastError: '',
    pollTimer: null,
    countdownInterval: null
  };

  const api = async (path, options={}) => {
    const response = await fetch(path, { headers:{'Content-Type':'application/json', ...(options.headers||{})}, ...options });
    const data = await response.json().catch(()=>({error:'The server returned an unreadable response.'}));
    if (!response.ok) throw new Error(data.error || `Request failed (${response.status})`);
    return data;
  };

  const escapeHtml = (v='') => String(v).replace(/[&<>"']/g, c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const zone = id => state.locations.find(x => Number(x.id) === Number(id));

  const nearestZone = (lat, lng) => {
    if (!state.locations.length || !Number.isFinite(lat) || !Number.isFinite(lng)) return state.locationId || 1;
    let best = state.locations[0], score = Infinity;
    for (const z of state.locations) {
      const d = (Number(z.lat) - lat)**2 + (Number(z.lng) - lng)**2;
      if (d < score) { score = d; best = z; }
    }
    return Number(best.id);
  };

  const typeIcon = t => ({medical:'✚',fire:'♨',road_accident:'↗',police:'◇',other:'!'}[t] || '!');
  const statusLabel = s => ({
    RECEIVED: 'Request received',
    FINDING_RESPONDER: 'Finding a responder',
    ASSIGNED: 'Awaiting acceptance',
    EN_ROUTE: 'Responder en route',
    AT_SCENE: 'At scene',
    IN_PROGRESS: 'Handling emergency',
    RESOLVED: 'Resolved',
    RETURNING_TO_STANDBY: 'Returning to standby',
    CANCELLED: 'Cancelled',
    AVAILABLE: 'Available'
  }[s] || String(s||'Unknown').replaceAll('_',' '));

  const statusClass = s => 'status-pill status-' + String(s||'').toLowerCase();
  const dateLabel = iso => {
    if (!iso) return 'Just now';
    const d = new Date(iso);
    return Number.isNaN(d.getTime()) ? 'Just now' : d.toLocaleString([], {month:'short',day:'numeric',hour:'2-digit',minute:'2-digit'});
  };

  const showAlert = (message, warn=false) => {
    const el = $('#system-alert');
    if (!el) return;
    el.textContent = message;
    el.classList.toggle('hidden', !message);
    el.classList.toggle('alert-warn', warn);
  };

  const formMessage = message => {
    const el = $('#form-message');
    if (!el) return;
    el.textContent = message;
    el.classList.toggle('hidden', !message);
  };

  const setLocationStatus = (title, copy) => {
    const t = $('#location-status-title'), c = $('#location-status-copy');
    if (t) t.textContent = title;
    if (c) c.textContent = copy;
  };

  const updateDiagnosticsUI = () => {
    const diagRaw = $('#diag-raw-coords');
    const diagAcc = $('#diag-accuracy');
    const diagTime = $('#diag-timestamp');
    const diagConf = $('#diag-confirmed-coords');
    const diagZone = $('#diag-zone');
    const diagSrc = $('#diag-source');

    if (diagRaw) {
      diagRaw.textContent = state.rawGps
        ? `${state.rawGps.lat.toFixed(5)}, ${state.rawGps.lng.toFixed(5)}`
        : 'None recorded';
    }
    if (diagAcc) {
      diagAcc.textContent = state.accuracy != null
        ? `±${state.accuracy} meters (${state.accuracy <= 100 ? 'GPS High Precision' : state.accuracy <= 1000 ? 'Wi-Fi/Cellular' : 'Coarse IP/Network'})`
        : 'Unknown';
    }
    if (diagTime) {
      diagTime.textContent = state.rawGps?.timestamp
        ? new Date(state.rawGps.timestamp).toLocaleTimeString()
        : 'None';
    }
    if (diagConf) {
      diagConf.textContent = (state.lat != null && state.lng != null)
        ? `${state.lat.toFixed(5)}, ${state.lng.toFixed(5)}`
        : 'None confirmed';
    }
    if (diagZone) {
      const z = zone(state.locationId);
      diagZone.textContent = z ? `[${z.id}] ${z.name}` : 'Not mapped';
    }
    if (diagSrc) {
      diagSrc.textContent = state.locationSource || 'Pending';
    }
  };

  const setAddress = (address, source='') => {
    state.address = address || '';
    const addrInput = $('#address');
    if (addrInput) addrInput.value = state.address;
    const z = zone(state.locationId);
    const caption = $('#map-caption-text');
    if (caption) caption.textContent = state.address || (z ? `Near ${z.name}, Pune` : 'Location not confirmed');
    if (source) {
      state.locationSource = source;
      setLocationStatus(source, state.lat != null ? 'Pin confirmed on map. You can submit or adjust.' : 'Choose a zone or enter the address.');
    }
    updateDiagnosticsUI();
  };

  const showMapsFallback = message => {
    const alertEl = $('#maps-alert');
    if (alertEl) { alertEl.textContent = message; alertEl.classList.remove('hidden'); }
    $('#map-placeholder')?.classList.remove('hidden');
    $('#responder-map-placeholder')?.classList.remove('hidden');
    $('#citizen-map')?.classList.add('hidden');
    $('#responder-map')?.classList.add('hidden');
  };

  const hideMapsFallback = () => {
    $('#maps-alert')?.classList.add('hidden');
    $('#map-placeholder')?.classList.add('hidden');
    $('#responder-map-placeholder')?.classList.add('hidden');
    $('#citizen-map')?.classList.remove('hidden');
    $('#responder-map')?.classList.remove('hidden');
  };

  // -------------------------------------------------------------------
  // Marker styling helpers (SVG pins / icons)
  // -------------------------------------------------------------------
  function createPinSvg(color, text, isCircle=false) {
    if (isCircle) {
      return {
        url: 'data:image/svg+xml;charset=UTF-8,' + encodeURIComponent(
          `<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32">
             <circle cx="16" cy="16" r="14" fill="${color}" stroke="#ffffff" stroke-width="2.5"/>
             <text x="16" y="20" font-size="10" font-family="sans-serif" font-weight="bold" fill="#ffffff" text-anchor="middle">${text}</text>
           </svg>`
        ),
        scaledSize: new google.maps.Size(32, 32),
        anchor: new google.maps.Point(16, 16)
      };
    }
    return {
      url: 'data:image/svg+xml;charset=UTF-8,' + encodeURIComponent(
        `<svg xmlns="http://www.w3.org/2000/svg" width="34" height="42" viewBox="0 0 34 42">
           <path d="M17 0C7.6 0 0 7.6 0 17c0 11.9 17 25 17 25s17-13.1 17-25C34 7.6 26.4 0 17 0z" fill="${color}" stroke="#ffffff" stroke-width="1.8"/>
           <circle cx="17" cy="15" r="11" fill="#ffffff"/>
           <text x="17" y="19" font-size="9" font-family="sans-serif" font-weight="bold" fill="${color}" text-anchor="middle">${text}</text>
         </svg>`
      ),
      scaledSize: new google.maps.Size(34, 42),
      anchor: new google.maps.Point(17, 42)
    };
  }

  // -------------------------------------------------------------------
  // Google Maps Initialization
  // -------------------------------------------------------------------
  function loadGoogleMaps(key) {
    if (!key) {
      showMapsFallback('Google Maps is not configured. Add googleMapApi=YOUR_KEY to .env to enable interactive maps. Reporting and simulated dispatch remain fully operational.');
      return;
    }
    window.initGoogleMapsForPulse = () => {
      try {
        state.geocoder = new google.maps.Geocoder();
        const puneCenter = { lat: 18.5204, lng: 73.8567 };

        // 1. Citizen Map
        state.map = new google.maps.Map($('#citizen-map'), {
          center: puneCenter,
          zoom: 12,
          streetViewControl: false,
          mapTypeControl: false,
          fullscreenControl: true,
          clickableIcons: false
        });

        state.marker = new google.maps.Marker({
          map: state.map,
          draggable: true,
          title: 'Emergency incident location'
        });

        state.marker.addListener('dragend', () => {
          const p = state.marker.getPosition();
          setCoords(p.lat(), p.lng(), 'Pin moved by citizen on map');
          reverseGeocode(p.lat(), p.lng());
        });

        state.map.addListener('click', e => {
          setCoords(e.latLng.lat(), e.latLng.lng(), 'Point selected on map');
          reverseGeocode(e.latLng.lat(), e.latLng.lng());
        });

        // 2. Responder Map (Map-First Dispatch Dashboard)
        state.responderMap = new google.maps.Map($('#responder-map'), {
          center: puneCenter,
          zoom: 12,
          streetViewControl: false,
          mapTypeControl: false,
          fullscreenControl: true
        });

        state.responderMap.addListener('click', () => {
          // Deselect specific emergency when clicking neutral map
          if (state.selectedEmergency && !state.selectedResponder) {
            state.selectedEmergency = null;
            renderOperationsPanel();
          }
        });

        hideMapsFallback();

        if (state.lat != null) {
          setMapPosition(state.lat, state.lng);
          reverseGeocode(state.lat, state.lng);
        }
        if (state.address) geocodeAddress(state.address, false);

        syncResponderMapMarkers();
      } catch(err) {
        showMapsFallback('Google Maps could not initialize. Check Maps JavaScript API enablement and key restrictions.');
      }
    };

    const script = document.createElement('script');
    script.src = `https://maps.googleapis.com/maps/api/js?key=${encodeURIComponent(key)}&callback=initGoogleMapsForPulse&v=weekly`;
    script.async = true;
    script.defer = true;
    script.onerror = () => showMapsFallback('Google Maps failed to load. Check API key, allowed website referrers, and billing settings.');
    document.head.appendChild(script);
  }

  // -------------------------------------------------------------------
  // Coordinate & Geolocation Management (GPS Accuracy Fix)
  // -------------------------------------------------------------------
  function setCoords(lat, lng, source='Location updated', accuracyMeters=null) {
    if (!Number.isFinite(Number(lat)) || !Number.isFinite(Number(lng))) return;
    state.lat = Number(lat);
    state.lng = Number(lng);
    state.locationSource = source;
    if (accuracyMeters != null) state.accuracy = accuracyMeters;

    state.locationId = nearestZone(state.lat, state.lng);
    const zoneSelect = $('#manual-zone');
    if (zoneSelect) zoneSelect.value = String(state.locationId);

    const z = zone(state.locationId);
    setLocationStatus(source, `Coordinates ${state.lat.toFixed(5)}, ${state.lng.toFixed(5)} · zone: ${z?.name||'Pune'}`);

    if (state.marker) {
      state.marker.setPosition({ lat: state.lat, lng: state.lng });
      state.marker.setVisible(true);
    }
    if (state.map) {
      state.map.panTo({ lat: state.lat, lng: state.lng });
    }

    const caption = $('#map-caption-text');
    if (caption) caption.textContent = state.address || `Pin selected near ${z?.name||'Pune'}`;

    updateDiagnosticsUI();
  }

  function setMapPosition(lat, lng) {
    if (!state.map || !state.marker) return;
    state.marker.setPosition({ lat, lng });
    state.marker.setVisible(true);
    state.map.panTo({ lat, lng });
    state.map.setZoom(Math.max(state.map.getZoom() || 12, 14));
  }

  function reverseGeocode(lat, lng) {
    if (!state.geocoder) return;
    state.geocoder.geocode({ location: { lat, lng } }, (results, status) => {
      if (status === 'OK' && results && results[0]) {
        setAddress(results[0].formatted_address, state.locationSource || 'Location selected on map');
      } else {
        const z = zone(state.locationId);
        setAddress(`${z?.name||'Pune'}, Maharashtra`, state.locationSource || 'Zone estimated from map');
      }
    });
  }

  function geocodeAddress(address, moveMap=true) {
    if (!state.geocoder || !address) return Promise.resolve(false);
    return new Promise(resolve => {
      state.geocoder.geocode({ address: address + ', Pune, Maharashtra, India' }, (results, status) => {
        if (status === 'OK' && results && results[0]) {
          const p = results[0].geometry.location;
          setCoords(p.lat(), p.lng(), 'Address located via Google Geocoder');
          if (moveMap) setMapPosition(p.lat(), p.lng());
          setAddress(results[0].formatted_address, 'Address located via Google Geocoder');
          resolve(true);
        } else {
          resolve(false);
        }
      });
    });
  }

  function tryGps() {
    if (!navigator.geolocation) {
      setLocationStatus('GPS is not available in this browser', 'Enter an address or select a Pune zone below.');
      return;
    }

    setLocationStatus('Requesting device location…', 'Querying hardware GPS / network sensors...');
    const accPill = $('#accuracy-pill');
    const accWarn = $('#accuracy-warning');

    navigator.geolocation.getCurrentPosition(
      pos => {
        const lat = pos.coords.latitude;
        const lng = pos.coords.longitude;
        const acc = Math.round(pos.coords.accuracy || 0);

        state.rawGps = { lat, lng, accuracy: acc, timestamp: pos.timestamp };

        // Bounds validation
        if (lat < -90 || lat > 90 || lng < -180 || lng > 180) {
          setLocationStatus('Invalid coordinates received', 'Please select your location manually.');
          return;
        }

        // Accuracy classification and honest reporting
        if (accPill) {
          accPill.classList.remove('hidden', 'warn', 'danger');
          if (acc <= 100) {
            accPill.textContent = `GPS ±${acc}m (High precision)`;
            if (accWarn) accWarn.classList.add('hidden');
          } else if (acc <= 1000) {
            accPill.classList.add('warn');
            accPill.textContent = `Est ±${acc}m (Wi-Fi/Cellular)`;
            if (accWarn) accWarn.classList.add('hidden');
          } else {
            accPill.classList.add('danger');
            accPill.textContent = `Low Acc ±${(acc/1000).toFixed(1)}km`;
            if (accWarn) {
              accWarn.classList.remove('hidden');
              const warnText = $('#accuracy-warning-text');
              if (warnText) {
                warnText.textContent = `Browser returned a coarse network location (±${(acc/1000).toFixed(1)} km, typically Pune city center). If you are in Kondhwa or another district, please drag the map pin or select your zone below.`;
              }
            }
          }
        }

        setCoords(lat, lng, 'Device GPS detected', acc);

        if (state.geocoder) {
          reverseGeocode(lat, lng);
        } else {
          const z = zone(state.locationId);
          setAddress(`Near ${z?.name||'Pune'}, Maharashtra`, 'Device location detected');
        }
      },
      err => {
        const msg = err.code === 1
          ? 'Location permission denied by user.'
          : err.code === 2
          ? 'Device position unavailable.'
          : 'Location request timed out.';
        setLocationStatus(msg, 'You can select your Pune zone (e.g. Kondhwa) from the list below.');
        if (accPill) accPill.classList.add('hidden');
      },
      {
        enableHighAccuracy: true,
        timeout: 10000,
        maximumAge: 0 // Force fresh positioning to prevent stale city-center cached locks
      }
    );
  }

  function populateZones() {
    const sel = $('#manual-zone');
    if (!sel) return;
    sel.innerHTML = '<option value="">Choose a Pune zone…</option>' +
      state.locations.map(z => `<option value="${z.id}">[${z.id}] ${escapeHtml(z.name)}</option>`).join('');
    sel.value = state.locationId ? String(state.locationId) : '';
  }

  // -------------------------------------------------------------------
  // Map-First Marker Synchronization
  // -------------------------------------------------------------------
  function syncResponderMapMarkers() {
    if (!state.responderMap || !window.google || !google.maps) return;

    // 1. Responders layer
    const activeResponderIds = new Set();
    if (state.layers.responders) {
      for (const r of state.responders) {
        if (!r.lat || !r.lng) continue;
        activeResponderIds.add(r.id);

        let color = '#28a745'; // Available: Green
        if (r.status === 'ASSIGNED') color = '#fd7e14'; // Orange
        else if (['EN_ROUTE', 'AT_SCENE', 'IN_PROGRESS'].includes(r.status)) color = '#007bff'; // Blue
        else if (r.status === 'RETURNING_TO_STANDBY') color = '#6f42c1'; // Purple

        const pos = { lat: Number(r.lat), lng: Number(r.lng) };
        const icon = createPinSvg(color, r.id, true);

        if (state.responderMarkers.has(r.id)) {
          const m = state.responderMarkers.get(r.id);
          m.setPosition(pos);
          m.setIcon(icon);
          m.setTitle(`${r.id} (${r.type}) — ${statusLabel(r.status)}`);
          m.setVisible(true);
        } else {
          const m = new google.maps.Marker({
            position: pos,
            map: state.responderMap,
            title: `${r.id} (${r.type}) — ${statusLabel(r.status)}`,
            icon
          });
          m.addListener('click', () => {
            selectResponder(r.id);
          });
          state.responderMarkers.set(r.id, m);
        }
      }
    }
    // Remove or hide absent responders
    for (const [id, m] of state.responderMarkers) {
      if (!activeResponderIds.has(id)) m.setVisible(false);
    }

    // 2. Active Emergencies layer
    const activeEmergencyIds = new Set();
    if (state.layers.emergencies) {
      for (const e of state.emergencies) {
        if (['RESOLVED', 'CANCELLED'].includes(e.status)) continue;
        if (!e.lat || !e.lng) continue;
        activeEmergencyIds.add(e.id);

        const pos = { lat: Number(e.lat), lng: Number(e.lng) };
        const color = '#dc3545'; // Incident: Red
        const icon = createPinSvg(color, `E${e.id}`, false);

        if (state.emergencyMarkers.has(e.id)) {
          const m = state.emergencyMarkers.get(e.id);
          m.setPosition(pos);
          m.setIcon(icon);
          m.setTitle(`Emergency E${e.id} [${e.severity}] - ${e.typeLabel}`);
          m.setVisible(true);
        } else {
          const m = new google.maps.Marker({
            position: pos,
            map: state.responderMap,
            title: `Emergency E${e.id} [${e.severity}] - ${e.typeLabel}`,
            icon
          });
          m.addListener('click', () => {
            selectEmergency(e.id);
          });
          state.emergencyMarkers.set(e.id, m);
        }
      }
    }
    for (const [id, m] of state.emergencyMarkers) {
      if (!activeEmergencyIds.has(id)) m.setVisible(false);
    }

    // 3. Standby Hubs layer (Purple markers for optimized hubs)
    if (state.layers.standby && state.standbyMarkers.size === 0) {
      const topHubs = [1, 2, 7, 10, 16]; // Key coverage hubs in Pune graph
      for (const hid of topHubs) {
        const z = zone(hid);
        if (!z) continue;
        const icon = createPinSvg('#6f42c1', 'H' + hid, true);
        const m = new google.maps.Marker({
          position: { lat: Number(z.lat), lng: Number(z.lng) },
          map: state.responderMap,
          title: `Standby Hub: ${z.name}`,
          icon
        });
        m.addListener('click', () => {
          showStandbyHubInfo(z);
        });
        state.standbyMarkers.set(hid, m);
      }
    } else {
      for (const [, m] of state.standbyMarkers) {
        m.setVisible(state.layers.standby);
      }
    }
  }

  function fitMapToOverview() {
    if (!state.responderMap || !window.google) return;
    const bounds = new google.maps.LatLngBounds();
    let count = 0;

    for (const [, m] of state.responderMarkers) {
      if (m.getVisible()) { bounds.extend(m.getPosition()); count++; }
    }
    for (const [, m] of state.emergencyMarkers) {
      if (m.getVisible()) { bounds.extend(m.getPosition()); count++; }
    }

    if (count > 0) {
      state.responderMap.fitBounds(bounds, 50);
      if (state.responderMap.getZoom() > 14) state.responderMap.setZoom(14);
    } else {
      state.responderMap.setCenter({ lat: 18.5204, lng: 73.8567 });
      state.responderMap.setZoom(12);
    }
  }

  // -------------------------------------------------------------------
  // Responder & Incident Selection Handlers
  // -------------------------------------------------------------------
  function selectResponder(id) {
    state.selectedResponder = id;
    state.selectedEmergency = null;
    const r = state.responders.find(x => x.id === id);
    if (r && state.responderMap && r.lat && r.lng) {
      state.responderMap.panTo({ lat: Number(r.lat), lng: Number(r.lng) });
    }
    renderFleetSidebar();
    renderOperationsPanel();
  }

  function selectEmergency(id) {
    state.selectedEmergency = id;
    const e = state.emergencies.find(x => x.id === id);
    if (e && e.assignedVehicleId) {
      state.selectedResponder = e.assignedVehicleId;
    }
    if (e && state.responderMap && e.lat && e.lng) {
      state.responderMap.panTo({ lat: Number(e.lat), lng: Number(e.lng) });
    }
    renderFleetSidebar();
    renderOperationsPanel();
  }

  function showStandbyHubInfo(z) {
    const root = $('#active-item-panel');
    if (!root) return;
    root.innerHTML = `
      <div class="side-title">
        <span class="side-icon" style="background:#f2ecfa;color:#6f42c1">⌁</span>
        <div>
          <h3>Standby Hub: ${escapeHtml(z.name)}</h3>
          <p>Zone ${z.id} · Coverage Center</p>
        </div>
      </div>
      <div class="assignment-description">
        <b>Coverage optimization</b>
        This location is designated as a high-density coverage hub. Responders resolving emergencies nearby are routed here to balance city-wide response readiness.
      </div>
      <div class="detail-box">
        <small>COORDINATES</small>
        <b>${z.lat.toFixed(5)}, ${z.lng.toFixed(5)}</b>
      </div>
      <a class="primary-button" target="_blank" rel="noopener" href="${mapsDirections(z.lat, z.lng)}">
        Navigate to Hub in Google Maps ↗
      </a>
    `;
  }

  // -------------------------------------------------------------------
  // Left Sidebar: Fleet List & Filtering
  // -------------------------------------------------------------------
  function renderFleetSidebar() {
    const root = $('#fleet-sidebar-list');
    if (!root) return;

    let list = state.responders;

    // Apply status filter
    if (state.responderFilter === 'available') {
      list = list.filter(r => r.status === 'AVAILABLE');
    } else if (state.responderFilter === 'assigned') {
      list = list.filter(r => r.status === 'ASSIGNED');
    } else if (state.responderFilter === 'en_route') {
      list = list.filter(r => ['EN_ROUTE', 'AT_SCENE', 'IN_PROGRESS'].includes(r.status));
    } else if (state.responderFilter === 'returning') {
      list = list.filter(r => r.status === 'RETURNING_TO_STANDBY');
    }

    // Apply search filter
    if (state.responderSearch) {
      const q = state.responderSearch.toLowerCase();
      list = list.filter(r => r.id.toLowerCase().includes(q) || r.type.toLowerCase().includes(q) || r.locationName.toLowerCase().includes(q));
    }

    const summary = $('#fleet-count-summary');
    if (summary) summary.textContent = `${list.length} units listed`;

    if (!list.length) {
      root.innerHTML = '<div class="empty-state">No matching units found.</div>';
      return;
    }

    root.innerHTML = list.map(r => {
      const isSelected = r.id === state.selectedResponder;
      const assignmentId = r.currentEmergencyId > 0 ? `E${r.currentEmergencyId}` : '';
      return `
        <div class="fleet-item-card ${isSelected ? 'selected' : ''}" data-responder-id="${escapeHtml(r.id)}">
          <span class="fleet-avatar">${escapeHtml(r.id)}</span>
          <div class="fleet-item-main">
            <b>${escapeHtml(r.id)} · ${escapeHtml(r.type)} ${assignmentId ? `<span class="status-pill status-assigned" style="font-size:8px">${assignmentId}</span>` : ''}</b>
            <small>${escapeHtml(r.locationName)}</small>
          </div>
          <span class="${statusClass(r.status)}">${escapeHtml(statusLabel(r.status))}</span>
        </div>
      `;
    }).join('');

    root.querySelectorAll('.fleet-item-card').forEach(el => {
      el.addEventListener('click', () => {
        selectResponder(el.dataset.responderId);
      });
    });
  }

  // -------------------------------------------------------------------
  // Right Panel: Operations Desk & 60-Second Deadline
  // -------------------------------------------------------------------
  function mapsDirections(lat, lng) {
    if (!Number.isFinite(Number(lat)) || !Number.isFinite(Number(lng))) return 'https://www.google.com/maps';
    return `https://www.google.com/maps/dir/?api=1&destination=${encodeURIComponent(`${lat},${lng}`)}&travelmode=driving`;
  }

  function startCountdownTimer(deadlineIso, emergencyId, responderId) {
    if (state.countdownInterval) clearInterval(state.countdownInterval);

    const updateTimer = () => {
      const timerEl = $('#deadline-timer-display');
      const barEl = $('#deadline-progress-bar');
      if (!timerEl) {
        clearInterval(state.countdownInterval);
        return;
      }
      const deadlineMs = new Date(deadlineIso).getTime();
      const remainingSeconds = Math.max(0, Math.round((deadlineMs - Date.now()) / 1000));

      const mins = Math.floor(remainingSeconds / 60);
      const secs = remainingSeconds % 60;
      timerEl.textContent = `${String(mins).padStart(2,'0')}:${String(secs).padStart(2,'0')}`;

      if (barEl) {
        const pct = Math.max(0, Math.min(100, (remainingSeconds / 60) * 100));
        barEl.style.width = pct + '%';
      }

      if (remainingSeconds <= 0) {
        clearInterval(state.countdownInterval);
        timerEl.textContent = '00:00 (Expired)';
        // Trigger deadline refresh on backend
        refreshAll();
      }
    };

    updateTimer();
    state.countdownInterval = setInterval(updateTimer, 1000);
  }

  function renderOperationsPanel() {
    const root = $('#active-item-panel');
    if (!root) return;

    // Case 1: An Emergency was explicitly selected on the map
    if (state.selectedEmergency && !state.selectedResponder) {
      const e = state.emergencies.find(x => x.id === state.selectedEmergency);
      if (!e) {
        root.innerHTML = '<div class="empty-state">Emergency record not found.</div>';
        return;
      }
      root.innerHTML = `
        <div class="assignment-top">
          <div>
            <p class="eyebrow">INCIDENT INSPECTOR</p>
            <h3>${escapeHtml(e.typeLabel)} Emergency</h3>
            <p>${escapeHtml(statusLabel(e.status))} · ${escapeHtml(e.severity)} priority</p>
          </div>
          <span class="assignment-id">E${e.id}</span>
        </div>
        <div class="assignment-details">
          <div class="detail-box"><small>LOCATION</small><b>${escapeHtml(e.address || e.zoneName)}</b></div>
          <div class="detail-box"><small>ASSIGNED UNIT</small><b>${e.assignedVehicleId ? 'Unit ' + escapeHtml(e.assignedVehicleId) : 'Awaiting assignment'}</b></div>
          <div class="detail-box"><small>AFFECTED</small><b>${e.peopleAffected}</b></div>
          <div class="detail-box"><small>REJECTIONS</small><b>${e.rejectionCount || 0} attempt(s)</b></div>
        </div>
        <div class="assignment-description">
          <b>Description</b>
          ${escapeHtml(e.description)}
          ${e.locationDetails ? `<br><br><b>Location Instructions</b>${escapeHtml(e.locationDetails)}` : ''}
        </div>
        <div class="assignment-actions">
          <a class="primary-button" target="_blank" rel="noopener" href="${mapsDirections(e.lat, e.lng)}">
            Open Location in Google Maps ↗
          </a>
        </div>
      `;
      return;
    }

    // Case 2: A Responder is selected
    const r = state.responders.find(x => x.id === state.selectedResponder) || state.responders[0];
    if (!r) {
      root.innerHTML = '<div class="empty-state">No responder units available from backend.</div>';
      return;
    }
    state.selectedResponder = r.id;

    const e = state.emergencies.find(x => x.assignedVehicleId === r.id && !['RESOLVED', 'CANCELLED'].includes(x.status));
    const returning = r.status === 'RETURNING_TO_STANDBY'
      ? state.emergencies.find(x => x.assignedVehicleId === r.id && x.status === 'RESOLVED' && Number(x.standbyLocationId) > 0)
      : null;

    // Subcase 2A: Responder Returning to Standby
    if (returning) {
      const target = zone(r.standbyLocationId || returning.standbyLocationId);
      root.innerHTML = `
        <div class="assignment-top">
          <div>
            <p class="eyebrow">STANDBY REPOSITIONING</p>
            <h3>${escapeHtml(r.id)} · Returning</h3>
            <p>Unit returning to designated hub</p>
          </div>
          <span class="${statusClass('RETURNING_TO_STANDBY')}">Returning</span>
        </div>
        <div class="assignment-details">
          <div class="detail-box"><small>UNIT</small><b>${escapeHtml(r.id)} (${escapeHtml(r.type)})</b></div>
          <div class="detail-box"><small>STANDBY HUB</small><b>${escapeHtml(target?.name || 'Hub')}</b></div>
        </div>
        <div class="assignment-actions">
          <a class="primary-button" target="_blank" rel="noopener" href="${mapsDirections(target?.lat, target?.lng)}">
            Navigate to Standby ↗
          </a>
          <button class="secondary-button" id="btn-standby-arrived" type="button">
            Confirm Arrival at Standby
          </button>
        </div>
      `;
      $('#btn-standby-arrived')?.addEventListener('click', () => {
        doResponderAction('standby_arrived', returning.id);
      });
      return;
    }

    // Subcase 2B: Responder has an active Emergency assignment
    if (e) {
      const isAssignedPending = e.status === 'ASSIGNED';
      root.innerHTML = `
        <div class="assignment-top">
          <div>
            <p class="eyebrow">${isAssignedPending ? 'URGENT DISPATCH OFFER' : 'ACTIVE ASSIGNMENT'}</p>
            <h3>${escapeHtml(e.typeLabel)} Response</h3>
            <p>${escapeHtml(statusLabel(e.status))} · ${escapeHtml(e.severity)} Priority</p>
          </div>
          <span class="assignment-id">E${e.id}</span>
        </div>

        ${isAssignedPending && e.deadlineAt ? `
          <div class="deadline-countdown-box">
            <div class="deadline-header">
              <span>⏱ 60s Acceptance Deadline</span>
              <span id="deadline-timer-display" class="deadline-timer">01:00</span>
            </div>
            <div class="deadline-progress-bar">
              <div id="deadline-progress-bar" class="deadline-progress-fill"></div>
            </div>
          </div>
        ` : ''}

        <div class="assignment-details">
          <div class="detail-box"><small>INCIDENT LOCATION</small><b>${escapeHtml(e.address || e.zoneName)}</b></div>
          <div class="detail-box"><small>UNIT</small><b>${escapeHtml(r.id)} · ${escapeHtml(r.type)}</b></div>
          <div class="detail-box"><small>AFFECTED</small><b>${e.peopleAffected}</b></div>
          <div class="detail-box"><small>SIMULATED ETA</small><b>${e.graphEtaMinutes ? `${e.graphEtaMinutes} min` : 'Evaluating'}</b></div>
        </div>

        <div class="assignment-description">
          <b>Situation Report</b>
          ${escapeHtml(e.description)}
          ${e.locationDetails ? `<br><br><b>Location Instructions</b>${escapeHtml(e.locationDetails)}` : ''}
        </div>

        <div class="assignment-actions">
          ${isAssignedPending ? `
            <button id="btn-accept-nav" class="primary-button btn-accept-nav" type="button">
              Accept & Start Navigation ↗
            </button>
            <button id="btn-reject-assignment" class="secondary-button btn-reject" type="button">
              Reject Assignment ✕
            </button>
          ` : ''}

          ${e.status === 'EN_ROUTE' ? `
            <button id="btn-mark-arrived" class="secondary-button" type="button">Mark Arrival at Scene</button>
            <a class="primary-button" target="_blank" rel="noopener" href="${mapsDirections(e.lat, e.lng)}">Navigate ↗</a>
          ` : ''}

          ${e.status === 'AT_SCENE' ? `
            <button id="btn-start-work" class="secondary-button" type="button">Begin Handling Incident</button>
          ` : ''}

          ${e.status === 'IN_PROGRESS' ? `
            <button id="btn-resolve-emergency" class="primary-button" type="button">Resolve Emergency ✓</button>
          ` : ''}
        </div>
      `;

      if (isAssignedPending && e.deadlineAt) {
        startCountdownTimer(e.deadlineAt, e.id, r.id);
      } else if (state.countdownInterval) {
        clearInterval(state.countdownInterval);
      }

      // Wire up action buttons
      $('#btn-accept-nav')?.addEventListener('click', async () => {
        window.open(mapsDirections(e.lat, e.lng), '_blank');
        await doResponderAction('accept', e.id);
      });

      $('#btn-reject-assignment')?.addEventListener('click', async () => {
        await rejectAssignment(e.id, r.id);
      });

      $('#btn-mark-arrived')?.addEventListener('click', () => doResponderAction('arrived', e.id));
      $('#btn-start-work')?.addEventListener('click', () => doResponderAction('start_work', e.id));
      $('#btn-resolve-emergency')?.addEventListener('click', () => doResponderAction('resolve', e.id));
      return;
    }

    // Subcase 2C: Responder is Available (No active assignment)
    if (state.countdownInterval) clearInterval(state.countdownInterval);
    root.innerHTML = `
      <div class="assignment-top">
        <div>
          <p class="eyebrow">RESPONDER DESK</p>
          <h3>Unit ${escapeHtml(r.id)}</h3>
          <p>${escapeHtml(r.type)} · Stationed in ${escapeHtml(r.locationName)}</p>
        </div>
        <span class="${statusClass('AVAILABLE')}">Available</span>
      </div>
      <div class="assignment-details">
        <div class="detail-box"><small>CURRENT ZONE</small><b>${escapeHtml(r.locationName)}</b></div>
        <div class="detail-box"><small>INCIDENTS SERVED</small><b>${r.totalEmergencies}</b></div>
        <div class="detail-box"><small>TOTAL TIME</small><b>${r.totalResponseTime} min</b></div>
        <div class="detail-box"><small>TOTAL DISTANCE</small><b>${(r.totalDistance || 0).toFixed(1)} km</b></div>
      </div>
      <div class="assignment-description">
        <b>Ready for Dispatch</b>
        This unit is available. When a citizen reports an emergency, the C++ priority engine will evaluate Dijkstra route cost and assign eligible incidents automatically.
      </div>
      <div class="assignment-actions">
        <a class="secondary-button" target="_blank" rel="noopener" href="${mapsDirections(r.lat, r.lng)}">
          View Station on Map ↗
        </a>
      </div>
    `;
  }

  // -------------------------------------------------------------------
  // Action Handlers
  // -------------------------------------------------------------------
  async function doResponderAction(action, emergencyId) {
    try {
      showAlert('');
      await api(`/api/responders/${encodeURIComponent(state.selectedResponder)}/actions`, {
        method: 'POST',
        body: JSON.stringify({ action, emergencyId })
      });
      await refreshAll();
    } catch(err) {
      showAlert(err.message || 'Action could not be executed by backend.', true);
    }
  }

  async function rejectAssignment(emergencyId, responderId) {
    try {
      showAlert('');
      const res = await api(`/api/emergencies/${emergencyId}/reject`, {
        method: 'POST',
        body: JSON.stringify({ responderId, reason: 'Declined by responder' })
      });
      if (res && res.emergency && res.emergency.assignedVehicleId) {
        if (res.emergency.rejectionCount >= 2 && res.emergency.assignedVehicleId === res.emergency.firstAssignedVehicleId) {
          showAlert(`All eligible responders rejected E${emergencyId}. Cycling back to initial responder ${res.emergency.assignedVehicleId}.`, false);
        } else {
          showAlert(`Assignment E${emergencyId} rejected by ${responderId}. Reassigned to unit ${res.emergency.assignedVehicleId}.`, false);
        }
      } else {
        showAlert(`Assignment E${emergencyId} rejected. Emergency is in queue for next available responder.`, false);
      }
      await refreshAll();
    } catch(err) {
      showAlert(err.message || 'Rejection failed.', true);
    }
  }

  // -------------------------------------------------------------------
  // Citizen Emergency Submission
  // -------------------------------------------------------------------
  async function submitEmergency(ev) {
    ev.preventDefault();
    formMessage('');
    const button = $('#submit-emergency');
    button.disabled = true;
    button.querySelector('span:first-child').textContent = 'Submitting request…';

    try {
      let address = $('#address').value.trim();
      const zoneId = Number($('#manual-zone').value || state.locationId || 0);
      let lat = state.lat, lng = state.lng, locationId = state.locationId || zoneId;

      if ((lat == null || lng == null) && zoneId > 0) {
        const z = zone(zoneId);
        if (z) {
          lat = z.lat;
          lng = z.lng;
          locationId = zoneId;
          if (!address) address = `${z.name}, Pune, Maharashtra`;
        }
      }

      if (!address || !locationId || locationId < 1) {
        throw new Error('Confirm your location using GPS, dragging the map pin, or selecting a Pune zone.');
      }

      const payload = {
        type: $('input[name="type"]:checked').value,
        address,
        locationDetails: $('#location-details').value.trim(),
        description: $('#description').value.trim(),
        peopleAffected: Math.max(1, Number($('#people').value) || 1),
        contact: $('#contact').value.trim(),
        locationId,
        lat,
        lng
      };

      const result = await api('/api/emergencies', { method: 'POST', body: JSON.stringify(payload) });
      const e = result.emergency;

      renderConfirmation(e);
      await refreshAll();
      $('#confirmation-card')?.scrollIntoView({ behavior: 'smooth', block: 'nearest' });

      if (result.duplicate) {
        formMessage(`Showing recent existing request E${e.id} to avoid duplicate dispatch.`);
      }

      $('#description').value = '';
      $('#location-details').value = '';
    } catch(err) {
      formMessage(err.message || 'Request could not be submitted. Check connection.');
    } finally {
      button.disabled = false;
      button.querySelector('span:first-child').textContent = 'Report emergency';
    }
  }

  function renderConfirmation(e) {
    const card = $('#confirmation-card');
    if (!card) return;
    card.classList.remove('hidden');
    card.innerHTML = `
      <div class="confirmation-head">
        <span class="confirmation-check">✓</span>
        <div>
          <h3>${e.status === 'ASSIGNED' ? 'Responder Dispatched' : e.status === 'FINDING_RESPONDER' ? 'Request in Queue' : 'Incident Recorded'}</h3>
          <p>${e.status === 'ASSIGNED' ? 'A compatible responder was automatically assigned by the C++ engine.' : 'Your request is recorded and awaiting responder assignment.'}</p>
        </div>
      </div>
      <div class="confirmation-id">
        <div><small>EMERGENCY REFERENCE</small><div><strong>E${e.id}</strong></div></div>
        <span class="${statusClass(e.status)}">${escapeHtml(statusLabel(e.status))}</span>
      </div>
      <div class="request-meta">
        <span>${escapeHtml(e.typeLabel)} · ${escapeHtml(e.severity)} priority</span>
        <span>${escapeHtml(e.address || e.zoneName)}</span>
      </div>
      <button class="secondary-button" id="track-this-emergency" type="button">View request status ↓</button>
    `;
    $('#track-this-emergency')?.addEventListener('click', () => {
      $('#request-list')?.scrollIntoView({ behavior: 'smooth', block: 'start' });
    });
  }

  function renderRequestList() {
    const root = $('#request-list');
    if (!root) return;
    const list = state.emergencies.slice(0, 8);
    if (!list.length) {
      root.innerHTML = '<div class="empty-state" style="grid-column:1/-1">No emergencies submitted in this session yet.</div>';
      return;
    }
    root.innerHTML = list.map(e => `
      <article class="request-card">
        <div class="request-type-icon">${typeIcon(e.type)}</div>
        <div class="request-card-main">
          <div class="request-card-top">
            <h3>E${e.id} · ${escapeHtml(e.typeLabel)}</h3>
            <span class="${statusClass(e.status)}">${escapeHtml(statusLabel(e.status))}</span>
          </div>
          <p>${escapeHtml(e.address || e.zoneName)}${e.locationDetails ? ' · ' + escapeHtml(e.locationDetails) : ''}</p>
          <p>${escapeHtml(e.description)}</p>
          <div class="request-meta">
            <span>${escapeHtml(e.severity)} priority · ${dateLabel(e.createdAt)}</span>
            <span>${e.assignedVehicleId ? 'Unit ' + escapeHtml(e.assignedVehicleId) : 'Searching…'}</span>
          </div>
        </div>
      </article>
    `).join('');
  }

  function renderStats() {
    const avail = $('#stat-available');
    const act = $('#stat-active');
    const res = $('#stat-resolved');
    const updated = $('#last-updated');

    if (avail) avail.textContent = state.responders.filter(r => r.status === 'AVAILABLE').length;
    if (act) act.textContent = state.emergencies.filter(e => !['RESOLVED', 'CANCELLED'].includes(e.status)).length;
    if (res) res.textContent = state.emergencies.filter(e => e.status === 'RESOLVED').length;
    if (updated) updated.textContent = 'Updated ' + new Date().toLocaleTimeString([], { hour:'2-digit', minute:'2-digit', second:'2-digit' });
  }

  // -------------------------------------------------------------------
  // Full Refresh Loop
  // -------------------------------------------------------------------
  async function refreshAll() {
    try {
      const [emergencies, responders] = await Promise.all([
        api('/api/emergencies'),
        api('/api/responders')
      ]);
      state.emergencies = emergencies;
      state.responders = responders;

      if (!state.selectedResponder && responders.length) {
        state.selectedResponder = responders[0].id;
      }

      renderRequestList();
      renderFleetSidebar();
      renderOperationsPanel();
      renderStats();
      syncResponderMapMarkers();

      if (!state.lastError) {
        $('#system-alert')?.classList.add('hidden');
      }
    } catch(err) {
      state.lastError = err.message;
      showAlert(`Backend connection issue: ${err.message}`, true);
    }
  }

  function setPortal(name) {
    state.activePortal = name;
    $$('.portal-tab').forEach(b => {
      const active = b.dataset.portal === name;
      b.classList.toggle('active', active);
      b.setAttribute('aria-selected', String(active));
    });

    $('#citizen-portal')?.classList.toggle('hidden', name !== 'citizen');
    $('#responder-portal')?.classList.toggle('hidden', name !== 'responder');

    if (name === 'responder' && state.responderMap) {
      setTimeout(() => {
        google.maps.event.trigger(state.responderMap, 'resize');
        fitMapToOverview();
      }, 100);
    }
  }

  // -------------------------------------------------------------------
  // Initialization
  // -------------------------------------------------------------------
  async function init() {
    try {
      const [config, locations] = await Promise.all([
        api('/api/config'),
        api('/api/locations')
      ]);
      state.config = config;
      state.locations = locations;

      populateZones();
      state.locationId = null;
      state.lat = null;
      state.lng = null;
      setAddress('');

      // Zone dropdown listener
      $('#manual-zone')?.addEventListener('change', () => {
        const val = $('#manual-zone').value;
        const z = zone(val);
        if (!z) return;
        state.locationId = z.id;
        state.lat = z.lat;
        state.lng = z.lng;
        setAddress(`${z.name}, Pune, Maharashtra`, `Manual selection: ${z.name}`);
        if (state.map) setMapPosition(z.lat, z.lng);
        $('#accuracy-warning')?.classList.add('hidden');
        $('#accuracy-pill')?.classList.add('hidden');
      });

      // Address change listener
      $('#address')?.addEventListener('change', () => {
        const val = $('#address').value.trim();
        if (val) {
          if (state.geocoder) geocodeAddress(val);
          else {
            state.address = val;
            setAddress(val, 'Address entered manually');
          }
        }
      });

      $('#use-zone-location')?.addEventListener('click', () => {
        const z = zone($('#manual-zone')?.value || state.locationId || 1);
        if (z) {
          state.locationId = z.id;
          state.lat = z.lat;
          state.lng = z.lng;
          setAddress(`${z.name}, Pune, Maharashtra`, `Snapped to zone: ${z.name}`);
          if (state.map) setMapPosition(z.lat, z.lng);
        }
      });

      $('#retry-location')?.addEventListener('click', tryGps);
      $('#emergency-form')?.addEventListener('submit', submitEmergency);
      $('#refresh-requests')?.addEventListener('click', refreshAll);
      $('#fit-map-overview')?.addEventListener('click', fitMapToOverview);

      // Search & filter chips for responder sidebar
      $('#responder-search')?.addEventListener('input', e => {
        state.responderSearch = e.target.value.trim();
        renderFleetSidebar();
      });

      $$('.filter-chip').forEach(btn => {
        btn.addEventListener('click', () => {
          $$('.filter-chip').forEach(b => b.classList.remove('active'));
          btn.classList.add('active');
          state.responderFilter = btn.dataset.filter;
          renderFleetSidebar();
        });
      });

      // Map layer toggles
      $('#toggle-layer-responders')?.addEventListener('change', e => {
        state.layers.responders = e.target.checked;
        syncResponderMapMarkers();
      });
      $('#toggle-layer-emergencies')?.addEventListener('change', e => {
        state.layers.emergencies = e.target.checked;
        syncResponderMapMarkers();
      });
      $('#toggle-layer-standby')?.addEventListener('change', e => {
        state.layers.standby = e.target.checked;
        syncResponderMapMarkers();
      });

      $$('.portal-tab').forEach(b => b.addEventListener('click', () => setPortal(b.dataset.portal)));

      loadGoogleMaps(config.googleMapApi);
      tryGps();
      await refreshAll();
      state.pollTimer = setInterval(refreshAll, 3000);
    } catch(err) {
      showAlert(`Initialization error: ${err.message}. Start the C++ web server from the project directory.`, true);
      setLocationStatus('Backend unavailable', 'Run ./run_web.sh and reload.');
    }
  }

  document.addEventListener('visibilitychange', () => {
    if (!document.hidden) refreshAll();
  });
  document.addEventListener('DOMContentLoaded', init);
})();
