import { AppState } from './app.js';

const devices = ['mcu', 'fpga', 'jtag'];
const deviceIds = {
    mcu: 'mcu_stm32f4',
    fpga: 'fpga_artix7',
    jtag: 'jtag_probe'
};
const pollIntervals = {};
const deviceStates = {};

export function initHardwareView() {
    devices.forEach(dev => {
        const devId = deviceIds[dev];
        const btnDiscover = document.getElementById(`btn-discover-${dev}`);
        const btnConnect = document.getElementById(`btn-connect-${dev}`);
        const btnCaps = document.getElementById(`btn-caps-${dev}`);
        const btnFail = document.getElementById(`btn-fail-${dev}`);
        const btnRecover = document.getElementById(`btn-recover-${dev}`);

        btnDiscover.addEventListener('click', async () => {
            const result = await callEmbeddedAction(devId, 'discover');
            if (result) {
                setDeviceState(dev, result.status);
                if (!result.success) AppState.showAlert(result.message || 'Device discovery was not available.', 'warning');
                else AppState.hideAlert();
            }
        });

        btnConnect.addEventListener('click', async () => {
            const isActive = ['CONNECTED', 'STREAMING', 'ERROR_TIMEOUT'].includes(deviceStates[dev]);
            const action = isActive ? 'disconnect' : 'connect';
            const result = await callEmbeddedAction(devId, action);
            if (!result) return;
            if (!result.success) {
                AppState.showAlert(result.message || `Could not ${action} ${dev.toUpperCase()}.`, 'warning');
                return;
            }

            setDeviceState(dev, result.status);
            AppState.hideAlert();
            if (action === 'connect') {
                startTelemetry(dev);
                showMessage(dev, 'Connection established. Waiting for the first telemetry sample…');
            } else {
                stopTelemetry(dev);
                showMessage(dev, 'Device disconnected. It remains discovered and ready to reconnect.');
            }
        });

        btnCaps.addEventListener('click', async () => {
            const data = await callEmbeddedAction(devId, 'capabilities');
            if (data) showMessage(dev, JSON.stringify(data, null, 2));
        });

        btnFail.addEventListener('click', async () => {
            const result = await callEmbeddedAction(devId, 'inject_failure');
            if (!result) return;
            setDeviceState(dev, result.status);
            if (result.success) {
                AppState.showAlert(`Simulated timeout on ${dev.toUpperCase()}: the 250 ms acknowledgement deadline expired.`, 'warning');
                showMessage(dev, `[ALERT] ${result.message}\nDeadline: 250 ms without acknowledgement.\nThe interface is suspended until recovery.`);
            } else {
                AppState.showAlert(result.message || 'Connect the simulated device before injecting a timeout.', 'warning');
            }
        });

        btnRecover.addEventListener('click', async () => {
            const result = await callEmbeddedAction(devId, 'recover');
            if (!result) return;
            setDeviceState(dev, result.status);
            if (result.recovered) {
                AppState.hideAlert();
                startTelemetry(dev);
                showMessage(dev, '[RECOVERY] Interface re-handshake completed. Buffers cleared and link resynchronized. Telemetry is resuming.');
            } else {
                AppState.showAlert(result.message || 'There is no active timeout to recover.', 'warning');
            }
        });
    });

    refreshDeviceStates();
}

async function refreshDeviceStates() {
    try {
        const response = await fetch('/api/embedded/devices');
        if (!response.ok) throw new Error(`Device status request failed (${response.status}).`);
        const currentDevices = await response.json();
        currentDevices.forEach(device => {
            const dev = Object.keys(deviceIds).find(key => deviceIds[key] === device.id);
            if (!dev) return;
            setDeviceState(dev, device.state);
            if (['CONNECTED', 'STREAMING', 'ERROR_TIMEOUT'].includes(device.state)) startTelemetry(dev);
        });
    } catch (error) {
        AppState.showAlert(`Could not read hardware simulator status: ${error.message}`, 'error');
    }
}

function setDeviceState(dev, state) {
    deviceStates[dev] = state;
    const badge = document.getElementById(`badge-${dev}`);
    const discover = document.getElementById(`btn-discover-${dev}`);
    const connect = document.getElementById(`btn-connect-${dev}`);
    const fail = document.getElementById(`btn-fail-${dev}`);
    const recover = document.getElementById(`btn-recover-${dev}`);
    const isActive = ['CONNECTED', 'STREAMING', 'ERROR_TIMEOUT'].includes(state);
    const className = {
        DISCONNECTED: 'disconnected',
        DISCOVERED: 'discovered',
        CONNECTED: 'connected',
        STREAMING: 'streaming',
        ERROR_TIMEOUT: 'error',
        RECOVERING: 'recovering'
    }[state] || 'disconnected';

    badge.className = `device-badge ${className}`;
    badge.textContent = state;
    discover.disabled = isActive || state === 'ERROR_TIMEOUT';
    connect.disabled = state === 'DISCONNECTED';
    connect.textContent = isActive ? 'Disconnect' : 'Connect';
    connect.className = `btn btn-sm ${isActive ? 'btn-danger' : 'btn-primary'}`;
    fail.disabled = !['CONNECTED', 'STREAMING'].includes(state);
    recover.disabled = state !== 'ERROR_TIMEOUT';
}

function startTelemetry(dev) {
    stopTelemetry(dev);
    pollIntervals[dev] = setInterval(async () => {
        try {
            const result = await callEmbeddedAction(deviceIds[dev], 'telemetry');
            if (!result) return;
            if (result.status) setDeviceState(dev, result.status);
            if (result.status === 'ERROR_TIMEOUT') {
                showMessage(dev, `[ERROR TIMEOUT] ${result.error}`);
            } else if (result.status === 'STREAMING') {
                showMessage(dev, JSON.stringify(result, null, 2));
            }
        } catch (_) {
            // A transient polling failure should not stop the next telemetry attempt.
        }
    }, 1200);
}

function stopTelemetry(dev) {
    if (pollIntervals[dev]) {
        clearInterval(pollIntervals[dev]);
        delete pollIntervals[dev];
    }
}

function showMessage(dev, message) {
    document.getElementById(`telemetry-${dev}`).textContent = message;
}

async function callEmbeddedAction(deviceId, action) {
    try {
        const response = await fetch('/api/embedded/action', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ device_id: deviceId, action })
        });
        const result = await response.json();
        if (!response.ok) {
            AppState.showAlert(result.message || result.error || `Hardware action failed (${response.status}).`, 'error');
            return null;
        }
        return result;
    } catch (error) {
        AppState.showAlert(`Embedded simulator communication error: ${error.message}`, 'error');
        return null;
    }
}
