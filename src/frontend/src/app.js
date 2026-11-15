import { initGeometryView } from './view_geometry.js';
import { initThermalView } from './view_thermal.js';
import { initPerformanceView } from './view_performance.js';
import { initHardwareView } from './view_hardware.js';

export class AppState {
    static showAlert(message, type = 'error') {
        const banner = document.getElementById('alert-banner');
        const msg = document.getElementById('alert-message');
        msg.textContent = message;
        banner.className = type;
        banner.style.display = 'flex';
    }

    static hideAlert() {
        const banner = document.getElementById('alert-banner');
        banner.style.display = 'none';
    }

    static setStatus(text, state = 'ready') {
        // Status details are shown through the active control or an actionable alert.
    }

}

document.addEventListener('DOMContentLoaded', async () => {
    const viewMeta = {
        geometry: ['CAD Geometry', 'Import, inspect, and validate STL models.'],
        thermal: ['Thermal Analysis', 'Solve a conduction model or inspect nodal temperature data.'],
        performance: ['Performance Lab', 'Measure serial and parallel workloads.'],
        hardware: ['Hardware Workbench', 'Explore target interfaces, telemetry, and recovery states.']
    };

    // Alert close button
    document.getElementById('alert-close-btn').addEventListener('click', () => {
        AppState.hideAlert();
    });

    // Tab navigation
    const tabButtons = document.querySelectorAll('.tab-btn');
    const viewContainers = document.querySelectorAll('.view-container');

    const activateView = (target, persist = true) => {
        if (!viewMeta[target]) return;
        tabButtons.forEach(btn => {
            const selected = btn.getAttribute('data-tab') === target;
            btn.classList.toggle('active', selected);
            if (selected) btn.setAttribute('aria-current', 'page');
            else btn.removeAttribute('aria-current');
        });
        viewContainers.forEach(view => view.classList.toggle('active', view.id === `view-${target}`));
        document.getElementById('workspace-title').textContent = viewMeta[target][0];
        document.getElementById('workspace-description').textContent = viewMeta[target][1];
        document.getElementById('workspace-section').textContent = target.toUpperCase();
        if (persist) {
            try { localStorage.setItem('uew.activeView', target); } catch (_) { /* storage is optional */ }
        }
        window.dispatchEvent(new Event('resize'));
    };

    tabButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            activateView(btn.getAttribute('data-tab'));
        });
    });

    const appShell = document.querySelector('.app-shell');
    const railToggle = document.getElementById('rail-toggle');
    const setRailCollapsed = (collapsed, persist = true) => {
        appShell.classList.toggle('rail-collapsed', collapsed);
        railToggle.setAttribute('aria-expanded', String(!collapsed));
        railToggle.setAttribute('aria-label', collapsed ? 'Expand navigation' : 'Collapse navigation');
        railToggle.title = collapsed ? 'Expand navigation' : 'Collapse navigation';
        if (persist) {
            try { localStorage.setItem('uew.railCollapsed', String(collapsed)); } catch (_) { /* storage is optional */ }
        }
        window.dispatchEvent(new Event('resize'));
        window.setTimeout(() => window.dispatchEvent(new Event('resize')), 260);
    };
    railToggle.addEventListener('click', () => {
        setRailCollapsed(!appShell.classList.contains('rail-collapsed'));
    });
    try {
        setRailCollapsed(localStorage.getItem('uew.railCollapsed') === 'true', false);
    } catch (_) { /* storage is optional */ }

    try {
        const lastView = localStorage.getItem('uew.activeView');
        if (viewMeta[lastView]) activateView(lastView, false);
    } catch (_) { /* storage is optional */ }

    // Query system info from backend
    try {
        const res = await fetch('/api/system/info');
        if (!res.ok) throw new Error(`System profile request failed (${res.status}).`);
        const info = await res.json();
        const hwCpu = document.getElementById('hw-cpu');
        const hwCores = document.getElementById('hw-cores');
        const hwComp = document.getElementById('hw-compiler');
        const hwFlags = document.getElementById('hw-flags');
        const railHostName = document.getElementById('rail-host-name');
        const railHostMeta = document.getElementById('rail-host-meta');
        const railHostDevice = document.getElementById('rail-host-device');
        const railHostStatus = document.getElementById('rail-host-status');
        const hostName = info.cpu_model || info.platform || 'Host system';
        const hostMeta = [
            info.platform,
            info.logical_cpus ? `${info.logical_cpus} logical CPUs` : null
        ].filter(Boolean).join(' · ');
        if (hwCpu) hwCpu.textContent = info.cpu_model || info.platform;
        if (hwCores) hwCores.textContent = `${info.physical_cpus} Physical / ${info.logical_cpus} Logical`;
        if (hwComp) hwComp.textContent = info.compiler;
        if (hwFlags) hwFlags.textContent = info.optimization_flags || 'Unavailable';
        if (railHostName) {
            railHostName.textContent = hostName;
            railHostName.title = hostName;
        }
        if (railHostMeta) railHostMeta.textContent = hostMeta || 'Local host profile';
        if (railHostDevice) railHostDevice.title = `Host device: ${hostName}${hostMeta ? ` · ${hostMeta}` : ''}`;
        if (railHostStatus) {
            railHostStatus.classList.remove('is-offline');
            railHostStatus.title = 'Host profile loaded';
            railHostStatus.setAttribute('aria-label', 'Host profile loaded');
        }
    } catch (e) {
        const railHostName = document.getElementById('rail-host-name');
        const railHostMeta = document.getElementById('rail-host-meta');
        const railHostDevice = document.getElementById('rail-host-device');
        const railHostStatus = document.getElementById('rail-host-status');
        if (railHostName) railHostName.textContent = 'Host unavailable';
        if (railHostMeta) railHostMeta.textContent = 'Local backend not detected';
        if (railHostDevice) railHostDevice.title = 'Host profile unavailable. Start the local C++ backend.';
        if (railHostStatus) {
            railHostStatus.classList.add('is-offline');
            railHostStatus.title = 'Host profile unavailable';
            railHostStatus.setAttribute('aria-label', 'Host profile unavailable');
        }
        AppState.setStatus('Service connection unavailable', 'error');
    }

    // Initialize all views
    initGeometryView();
    initThermalView();
    initPerformanceView();
    initHardwareView();
});

