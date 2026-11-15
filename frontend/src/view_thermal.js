import { CanvasChart } from './chart_engine.js';
import { Renderer3D } from './renderer3d.js';
import { AppState } from './app.js';

let feaPlot = null;
let convPlot = null;
let ansysRenderer = null;
let lastFEAResult = null;
let thermal3DSource = 'rod';
let ansysParseRevision = 0;

export function initThermalView() {
    const modeSelect = document.getElementById('thermal-mode-select');
    const feaPanel = document.getElementById('fea-controls-panel');
    const ansysPanel = document.getElementById('ansys-controls-panel');
    const feaDisplay = document.getElementById('fea-display-area');
    const ansysDisplay = document.getElementById('ansys-display-area');
    const modeButtons = [...document.querySelectorAll('[data-thermal-mode]')];

    modeButtons.forEach(button => {
        button.addEventListener('click', () => {
            modeSelect.value = button.dataset.thermalMode;
            modeSelect.dispatchEvent(new Event('change'));
        });
    });

    const visualizationTabs = [...document.querySelectorAll('[data-fea-view]')];
    visualizationTabs.forEach(button => {
        button.addEventListener('click', () => setFEAVisualization(button.dataset.feaView));
        button.addEventListener('keydown', event => {
            const current = visualizationTabs.indexOf(button);
            let next = current;
            if (event.key === 'ArrowRight') next = (current + 1) % visualizationTabs.length;
            else if (event.key === 'ArrowLeft') next = (current - 1 + visualizationTabs.length) % visualizationTabs.length;
            else if (event.key === 'Home') next = 0;
            else if (event.key === 'End') next = visualizationTabs.length - 1;
            else return;
            event.preventDefault();
            visualizationTabs[next].focus();
            visualizationTabs[next].click();
        });
    });

    modeSelect.addEventListener('change', () => {
        const isFEA = modeSelect.value === 'fea';
        modeButtons.forEach(button => {
            const selected = button.dataset.thermalMode === modeSelect.value;
            button.classList.toggle('active', selected);
            button.setAttribute('aria-pressed', String(selected));
        });
        feaPanel.hidden = !isFEA;
        ansysPanel.hidden = isFEA;
        feaDisplay.hidden = !isFEA;
        ansysDisplay.hidden = isFEA;
        document.getElementById('node-tooltip').style.display = 'none';
        if (isFEA) {
            if (feaPlot) feaPlot.resize();
            if (convPlot) convPlot.resize();
        } else {
            if (!ansysRenderer) {
                initANSYSRenderer();
            } else {
                ansysRenderer.resize();
            }
            if (thermal3DSource === 'rod') {
                if (lastFEAResult) updateThermalRodView(lastFEAResult);
                else setThermalRodWaiting();
            } else if (ansysRenderer?.ansysDataSet) {
                ansysRenderer.resize();
            } else {
                document.getElementById('ansys-parse-btn').click();
            }
        }
    });

    // --- 1D FEA Initialization ---
    initFEASolver();

    // --- ANSYS Initialization ---
    initANSYSControls();

    window.addEventListener('resize', () => {
        const view = document.getElementById('view-thermal');
        if (view && view.classList.contains('active')) {
            if (modeSelect.value === 'fea') {
                if (feaPlot) feaPlot.resize();
                if (convPlot) convPlot.resize();
            } else if (ansysRenderer) {
                ansysRenderer.resize();
            }
        }
    });
}

function initFEASolver() {
    feaPlot = new CanvasChart('canvas-fea-plot', {
        title: '',
        xLabel: 'Rod Position x (m)',
        yLabel: 'Temperature T (°C)',
        showLegend: false,
        padding: { top: 18, right: 24, bottom: 42, left: 62 }
    });

    convPlot = new CanvasChart('canvas-convergence-plot', {
        title: '',
        xLabel: 'Mesh Element Length h (m) [Log Scale]',
        yLabel: 'Max Absolute Midpoint Error (°C) [Log Scale]',
        logX: true,
        logY: true,
        showLegend: false,
        padding: { top: 18, right: 24, bottom: 42, left: 72 }
    });

    const solveBtn = document.getElementById('fea-solve-btn');
    const exportBtn = document.getElementById('fea-export-btn');
    const lengthInput = document.getElementById('fea-input-L');
    updateFEAMeshOptionLabels(parseFloat(lengthInput.value));
    lengthInput.addEventListener('input', () => updateFEAMeshOptionLabels(parseFloat(lengthInput.value)));

    solveBtn.addEventListener('click', runFEASolve);
    exportBtn.addEventListener('click', exportFEACSV);
    exportBtn.disabled = true;
    setFEAVisualization('profile');

    // Initial FEA solve
    runFEASolve();
}

function updateFEAMeshOptionLabels(length) {
    if (!Number.isFinite(length) || length <= 0) return;
    const meshSelect = document.getElementById('fea-input-N');
    if (!meshSelect) return;
    [...meshSelect.options].forEach(option => {
        const elements = Number(option.value);
        if (Number.isInteger(elements) && elements > 0) {
            option.textContent = `N = ${elements} · h = ${(length / elements).toPrecision(4)} m`;
        }
    });
}

async function runFEASolve() {
    const L = parseFloat(document.getElementById('fea-input-L').value);
    const A = parseFloat(document.getElementById('fea-input-A').value);
    const k = parseFloat(document.getElementById('fea-input-k').value);
    const q = parseFloat(document.getElementById('fea-input-q').value);
    const T0 = parseFloat(document.getElementById('fea-input-T0').value);
    const TL = parseFloat(document.getElementById('fea-input-TL').value);
    const N = parseInt(document.getElementById('fea-input-N').value);
    updateFEAMeshOptionLabels(L);
    const solveBtn = document.getElementById('fea-solve-btn');
    const exportBtn = document.getElementById('fea-export-btn');

    lastFEAResult = null;
    exportBtn.disabled = true;
    feaPlot.setSeries([]);
    convPlot.setSeries([]);
    document.getElementById('fea-conv-tbody').replaceChildren();
    resetFEASummary();
    setThermalStatus(document.getElementById('fea-run-status'), 'attention', 'Checking inputs');

    // Frontend validation
    if (![L, A, k, q, T0, TL].every(Number.isFinite) || L <= 0 || A <= 0 || k <= 0 || !Number.isInteger(N) || N < 1) {
        AppState.showAlert('Enter finite numeric values. Length, area, and conductivity must be positive, and the element count must be at least 1.', 'warning');
        AppState.setStatus('FEA input validation failed', 'error');
        setThermalStatus(document.getElementById('fea-run-status'), 'attention', 'Review inputs');
        return;
    }

    solveBtn.disabled = true;
    solveBtn.innerHTML = '<span aria-hidden="true">◌</span> Solving…';
    setThermalStatus(document.getElementById('fea-run-status'), 'busy', 'Solving model');
    AppState.setStatus('Solving thermal model and convergence study', 'busy');

    try {
        const res = await fetch('/api/fea/solve', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ L, A, k, q, T0, TL, num_elements: N })
        });
        const data = await res.json();

        if (!data.success) {
            AppState.showAlert(`FEA Solver Error: ${data.error_message}`, 'error');
            AppState.setStatus('FEA solve failed', 'error');
            setThermalStatus(document.getElementById('fea-run-status'), 'error', 'Solve failed');
            return;
        }

        AppState.hideAlert();
        lastFEAResult = data;
        updateFEAChartsAndTable(data);
        updateFEASummary(data);
        if (thermal3DSource === 'rod') updateThermalRodView(data);
        exportBtn.disabled = false;
        setThermalStatus(document.getElementById('fea-run-status'), 'ready', 'Solved');
        AppState.setStatus(`FEA Solved: N=${N}, Max Midpoint Error=${data.max_midpoint_error.toFixed(6)} °C, Max Nodal Error=${data.max_nodal_error.toExponential(2)} °C`, 'ready');
    } catch (e) {
        AppState.showAlert(`Failed to run FEA solver: ${e.message}`, 'error');
        AppState.setStatus('FEA error', 'error');
        setThermalStatus(document.getElementById('fea-run-status'), 'error', 'Solve failed');
    } finally {
        solveBtn.disabled = false;
        solveBtn.innerHTML = '<span aria-hidden="true">▶</span> Solve model';
    }
}

function setThermalStatus(element, state, label) {
    if (!element) return;
    element.dataset.state = state;
    const text = element.querySelector('span');
    if (text) text.textContent = label;
}

function setFEAVisualization(view) {
    document.querySelectorAll('[data-fea-view]').forEach(button => {
        const selected = button.dataset.feaView === view;
        button.classList.toggle('active', selected);
        button.setAttribute('aria-selected', String(selected));
        button.tabIndex = selected ? 0 : -1;
    });
    document.querySelectorAll('.thermal-viz-panel').forEach(panel => {
        panel.hidden = panel.id !== `thermal-viz-${view}`;
        panel.classList.toggle('active', !panel.hidden);
    });
    requestAnimationFrame(() => {
        if (view === 'profile' && feaPlot) feaPlot.resize();
        if (view === 'convergence' && convPlot) convPlot.resize();
    });
}

function updateFEASummary(data) {
    const cfg = data.config;
    const exact = x => cfg.T0 + (cfg.TL - cfg.T0) * (x / cfg.L) + (cfg.q / (2 * cfg.k)) * x * (cfg.L - x);
    const candidates = [{ x: 0, temperature: cfg.T0 }, { x: cfg.L, temperature: cfg.TL }];
    if (cfg.q !== 0) {
        const peakX = (cfg.L / 2) + (cfg.k * (cfg.TL - cfg.T0) / (cfg.q * cfg.L));
        if (peakX >= 0 && peakX <= cfg.L) candidates.push({ x: peakX, temperature: exact(peakX) });
    }
    const peak = candidates.reduce((best, item) => item.temperature > best.temperature ? item : best);
    const values = {
        'fea-stat-peak': `${peak.temperature.toFixed(1)} °C`,
        'fea-stat-peak-position': `at x = ${peak.x.toFixed(3)} m`,
        'fea-stat-midpoint': `${formatFEAError(data.max_midpoint_error)} °C`,
        'fea-stat-nodal': data.max_nodal_error.toExponential(2) + ' °C',
        'fea-stat-mesh': `${cfg.num_elements} elements`,
        'fea-stat-spacing': `h = ${(cfg.L / cfg.num_elements).toPrecision(4)} m`
    };
    Object.entries(values).forEach(([id, value]) => {
        const element = document.getElementById(id);
        if (element) element.textContent = value;
    });
}

function resetFEASummary() {
    ['fea-stat-peak', 'fea-stat-midpoint', 'fea-stat-nodal', 'fea-stat-mesh'].forEach(id => {
        const element = document.getElementById(id);
        if (element) element.textContent = '—';
    });
    document.getElementById('fea-stat-peak-position').textContent = 'Solve to calculate';
    document.getElementById('fea-stat-spacing').textContent = 'Element spacing';
    document.getElementById('fea-conv-order').textContent = '—';
    document.getElementById('fea-conv-reduction').textContent = '—';
    document.getElementById('fea-conv-agreement').textContent = '—';
}

function formatFEAError(value, decimals = 6) {
    if (!Number.isFinite(value)) return '—';
    const magnitude = Math.abs(value);
    if (value !== 0 && (magnitude < 1e-4 || magnitude >= 1e7)) return value.toExponential(3);
    return value.toFixed(decimals);
}

function updateFEAChartsAndTable(data) {
    // 1. FEA vs Exact Plot
    const exactSeries = [];
    const numFine = 100;
    const L = data.config.L, T0 = data.config.T0, TL = data.config.TL, k = data.config.k, q = data.config.q;
    for (let i = 0; i <= numFine; ++i) {
        const x = (i / numFine) * L;
        const T_ex = T0 + (TL - T0) * (x / L) + (q / (2 * k)) * x * (L - x);
        exactSeries.push({ x, y: T_ex });
    }

    const feaSeries = data.nodes.map(n => ({ x: n.x, y: n.T_fea }));

    feaPlot.setSeries([
        { label: 'Exact Analytical T_exact(x)', data: exactSeries, color: '#b49bff', lineWidth: 2, showPoints: false },
        { label: `FEA Discrete Nodes (N=${data.config.num_elements})`, data: feaSeries, color: '#e4b15d', lineWidth: 1.5, pointRadius: 4.5, showPoints: true }
    ]);

    // 2. Convergence Plot (Midpoint Error vs h)
    if (data.convergence && data.convergence.length > 0) {
        const convergence = data.convergence;
        const convData = convergence.map(c => ({
            x: c.h,
            y: c.max_midpoint_error,
            N: c.N,
            order: c.convergence_rate
        })).filter(point => point.x > 0 && point.y > 0).sort((a, b) => a.x - b.x);
        const segmentLabels = [];
        for (let i = 1; i < convData.length; ++i) {
            const fine = convData[i - 1];
            const coarse = convData[i];
            const reduction = coarse.y / fine.y;
            const order = Math.log(reduction) / Math.log(coarse.x / fine.x);
            segmentLabels.push({
                fromIndex: i - 1,
                toIndex: i,
                text: `${reduction.toFixed(2)}× · p=${order.toFixed(2)}`
            });
        }
        convPlot.setSeries(convData.length ? [{
            label: 'Measured midpoint error',
            data: convData,
            color: '#b49bff',
            lineWidth: 2.5,
            pointRadius: 5,
            pointLabels: convData.map(point => `N=${point.N}`),
            segmentLabels
        }] : []);

        const globalOrder = convergence.length > 1 && convergence[0].max_midpoint_error > 0 && convergence[convergence.length - 1].max_midpoint_error > 0
            ? Math.log(convergence[0].max_midpoint_error / convergence[convergence.length - 1].max_midpoint_error) /
                Math.log(convergence[0].h / convergence[convergence.length - 1].h)
            : NaN;
        const coarseError = convergence[0].max_midpoint_error;
        const fineError = convergence[convergence.length - 1].max_midpoint_error;
        const totalReduction = fineError > 0 ? coarseError / fineError : NaN;
        const maxRelativeBoundDifference = convergence.reduce((maxDiff, c) => {
            if (c.theoretical_error === 0) return Math.max(maxDiff, c.max_midpoint_error === 0 ? 0 : Infinity);
            return Math.max(maxDiff, Math.abs(c.max_midpoint_error - c.theoretical_error) / c.theoretical_error);
        }, 0);
        document.getElementById('fea-conv-order').textContent = Number.isFinite(globalOrder) ? globalOrder.toFixed(3) : '—';
        document.getElementById('fea-conv-reduction').textContent = Number.isFinite(totalReduction) ? `${totalReduction.toPrecision(3)}×` : '—';
        document.getElementById('fea-conv-agreement').textContent = coarseError === 0 && fineError === 0
            ? 'Zero error'
            : maxRelativeBoundDifference <= 1e-8 ? 'Matches' : 'Review';

        const activeMesh = data.config.num_elements;
        const meshNote = document.getElementById('fea-conv-mesh-note');
        meshNote.textContent = `Study: N = ${convergence.map(c => c.N).join(' · ')} · selected N = ${activeMesh}`;

        // 3. Update Table
        const tbody = document.getElementById('fea-conv-tbody');
        tbody.innerHTML = '';
        convergence.forEach((c, index) => {
            const tr = document.createElement('tr');
            if (c.N === activeMesh) tr.classList.add('active-mesh');
            const previous = convergence[index - 1];
            const reduction = previous && c.max_midpoint_error > 0
                ? `${(previous.max_midpoint_error / c.max_midpoint_error).toFixed(2)}×`
                : index === 0 ? 'Reference mesh' : 'No measurable error';
            tr.innerHTML = `
                <td>${c.N}${c.N === activeMesh ? '<span class="mesh-active-tag">SELECTED</span>' : ''}</td>
                <td>${c.h.toFixed(4)}</td>
                <td style="color:#b49bff;">${formatFEAError(c.max_midpoint_error, 8)}</td>
                <td style="color:#d5c9f5;">${formatFEAError(c.theoretical_error, 8)}</td>
                <td style="color:#99b184;">${reduction}</td>
                <td style="font-weight:bold; color:#99b184;">${c.convergence_rate > 0 ? c.convergence_rate.toFixed(3) : 'Base (Ref)'}</td>
            `;
            tbody.appendChild(tr);
        });
    }
}

function exportFEACSV() {
    if (!lastFEAResult) return;
    const cfg = lastFEAResult.config;
    let csv = `# Unified Engineering Workbench - 1D Thermal FEA Results\n`;
    csv += `# L=${cfg.L}m, A=${cfg.A}m2, k=${cfg.k}W/(m K), q=${cfg.q}W/m3, T0=${cfg.T0}C, TL=${cfg.TL}C, N=${cfg.num_elements}\n`;
    csv += `# mesh_profile\n`;
    csv += `type,x_m,T_fea_C,T_exact_C,absolute_error_C\n`;

    lastFEAResult.nodes.forEach(n => {
        const T_ex = cfg.T0 + (cfg.TL - cfg.T0) * (n.x / cfg.L) + (cfg.q / (2 * cfg.k)) * n.x * (cfg.L - n.x);
        csv += `node,${n.x},${n.T_fea},${T_ex},${Math.abs(n.T_fea - T_ex)}\n`;
    });

    lastFEAResult.midpoints.forEach(m => {
        csv += `midpoint,${m.x},${m.T_fea},${m.T_exact},${m.error}\n`;
    });

    csv += `# mesh_convergence\n`;
    csv += `elements_N,element_length_h_m,max_midpoint_error_C,theoretical_error_bound_C,observed_convergence_rate\n`;
    (lastFEAResult.convergence || []).forEach(c => {
        csv += `${c.N},${c.h},${c.max_midpoint_error},${c.theoretical_error},${c.convergence_rate}\n`;
    });

    downloadBlob(csv, `fea_thermal_N${cfg.num_elements}_results.csv`, 'text/csv');
}

// --- ANSYS 3D Point Cloud Implementation ---

function initANSYSRenderer() {
    ansysRenderer = new Renderer3D('canvas-ansys');
    ansysRenderer.setTooltipElement(document.getElementById('node-tooltip'));
}

function initANSYSControls() {
    const fileSelect = document.getElementById('ansys-file-select');
    const fileInput = document.getElementById('ansys-file-input');
    const parseBtn = document.getElementById('ansys-parse-btn');
    const fitBtn = document.getElementById('ansys-fit-btn');
    const sourceButtons = [...document.querySelectorAll('[data-thermal-source]')];

    sourceButtons.forEach(button => {
        button.addEventListener('click', () => setThermal3DSource(button.dataset.thermalSource));
    });

    document.getElementById('thermal-open-solver').addEventListener('click', () => {
        document.querySelector('[data-thermal-mode="fea"]').click();
        document.getElementById('fea-input-L').focus();
    });

    fitBtn.addEventListener('click', () => {
        if (!ansysRenderer) return;
        if (thermal3DSource === 'rod' && ansysRenderer.thermalRodBounds) {
            ansysRenderer.fitToView(ansysRenderer.thermalRodBounds.min, ansysRenderer.thermalRodBounds.max);
        } else if (ansysRenderer.ansysDataSet) {
            ansysRenderer.fitToView(ansysRenderer.ansysDataSet.bbox_min, ansysRenderer.ansysDataSet.bbox_max);
        }
    });

    parseBtn.addEventListener('click', async () => {
        if (thermal3DSource !== 'csv') return;
        if (!ansysRenderer) initANSYSRenderer();
        parseBtn.disabled = true;
        setThermalStatus(document.getElementById('ansys-result-state'), 'busy', 'Loading dataset');
        if (fileInput.files.length > 0) {
            const file = fileInput.files[0];
            if (file.size > 50 * 1024 * 1024) {
                AppState.showAlert('CSV upload exceeds the 50 MiB supported file limit. Choose a smaller nodal export.', 'warning');
                AppState.setStatus('ANSYS CSV exceeds the supported size limit', 'error');
                setThermalStatus(document.getElementById('ansys-result-state'), 'error', 'File too large');
                parseBtn.disabled = false;
                return;
            }
            try {
                const text = await file.text();
                parseANSYSContent(text, file.name);
            } catch (e) {
                AppState.showAlert(`Could not read the selected CSV: ${e.message}`, 'error');
                AppState.setStatus('ANSYS CSV read failed', 'error');
                setThermalStatus(document.getElementById('ansys-result-state'), 'error', 'Could not read file');
                parseBtn.disabled = false;
            }
        } else {
            parseANSYSFilePath(fileSelect.value);
        }
    });

    fileInput.addEventListener('change', () => {
        if (fileInput.files.length > 0) parseBtn.click();
    });

    fileSelect.addEventListener('change', () => {
        fileInput.value = '';
        parseBtn.click();
    });

    setThermal3DHeader('rod');
}

function setThermal3DSource(source) {
    thermal3DSource = source;
    if (source === 'rod') ansysParseRevision++;
    const isRod = source === 'rod';
    document.querySelectorAll('[data-thermal-source]').forEach(button => {
        const selected = button.dataset.thermalSource === source;
        button.classList.toggle('active', selected);
        button.setAttribute('aria-pressed', String(selected));
    });
    document.getElementById('thermal-rod-source-panel').hidden = !isRod;
    document.getElementById('thermal-csv-source-panel').hidden = isRod;
    setThermal3DHeader(source);

    if (!ansysRenderer) initANSYSRenderer();
    if (isRod) {
        document.getElementById('ansys-parse-btn').disabled = false;
        document.getElementById('ansys-meta-card').hidden = true;
        if (lastFEAResult) updateThermalRodView(lastFEAResult);
        else setThermalRodWaiting();
        ansysRenderer.resize();
    } else if (ansysRenderer.ansysDataSet) {
        ansysRenderer.resize();
    } else {
        document.getElementById('ansys-parse-btn').click();
    }
}

function setThermal3DHeader(source) {
    const isRod = source === 'rod';
    document.getElementById('thermal-3d-eyebrow').textContent = isRod ? '3D ROD VISUALIZATION' : '3D NODAL FIELD';
    document.getElementById('thermal-3d-title').textContent = isRod ? 'Solved rod temperature' : 'Temperature field';
    document.getElementById('thermal-3d-description').textContent = isRod
        ? 'Temperature mapped over the rod · fine rings mark the finite-element mesh.'
        : 'Hover to inspect a node · click to pin details · click the background to clear.';
    document.getElementById('canvas-ansys').setAttribute('aria-label', isRod
        ? 'Interactive 3D rod colored by the solved nodal temperatures'
        : 'Interactive 3D CSV nodal temperature point cloud');
    document.getElementById('thermal-rod-axis').hidden = !isRod;
    document.getElementById('thermal-3d-hint').hidden = isRod;
    document.getElementById('thermal-3d-footer-left').textContent = isRod
        ? 'Rod axis: x · Length: m · Temperature: °C'
        : 'Coordinates: metres · Temperature: °C';
    document.getElementById('thermal-3d-footer-right').textContent = isRod
        ? 'Cross-section enlarged schematically for visibility'
        : 'Standalone CSV point cloud · no CAD registration';
}

function updateThermalRodView(result) {
    if (!result?.success || !result.nodes?.length) return;
    let minT = Infinity;
    let maxT = -Infinity;
    for (const node of result.nodes) {
        minT = Math.min(minT, node.T_fea);
        maxT = Math.max(maxT, node.T_fea);
    }
    const cfg = result.config;
    document.getElementById('thermal-rod-node-count').textContent = result.nodes.length.toLocaleString();
    document.getElementById('thermal-rod-length').textContent = `${cfg.L.toPrecision(4)} m`;
    document.getElementById('thermal-rod-temp-range').textContent = `${minT.toFixed(1)}–${maxT.toFixed(1)} °C`;
    document.getElementById('thermal-rod-start').textContent = 'x = 0 m';
    document.getElementById('thermal-rod-end').textContent = `x = ${cfg.L.toPrecision(4)} m`;
    document.getElementById('thermal-rod-axis-title').textContent = `${cfg.num_elements} elements · ${result.nodes.length} mesh nodes`;
    document.getElementById('legend-min-temp').textContent = `${minT.toFixed(1)} °C`;
    document.getElementById('legend-mid-temp').textContent = `${((minT + maxT) / 2).toFixed(1)} °C`;
    document.getElementById('legend-max-temp').textContent = `${maxT.toFixed(1)} °C`;
    if (ansysRenderer && thermal3DSource === 'rod') {
        ansysRenderer.setThermalRod(result);
        document.getElementById('ansys-fit-btn').disabled = false;
        setThermalStatus(document.getElementById('ansys-result-state'), 'ready', `${cfg.num_elements} elements`);
    }
}

function setThermalRodWaiting() {
    if (ansysRenderer) ansysRenderer.clearANSYSDataSet();
    document.getElementById('thermal-rod-node-count').textContent = '—';
    document.getElementById('thermal-rod-length').textContent = '—';
    document.getElementById('thermal-rod-temp-range').textContent = '—';
    document.getElementById('ansys-fit-btn').disabled = true;
    setThermalStatus(document.getElementById('ansys-result-state'), 'attention', 'Solve model first');
}

async function parseANSYSFilePath(filepath) {
    if (thermal3DSource !== 'csv') return;
    const revision = ++ansysParseRevision;
    AppState.setStatus(`Parsing ANSYS CSV: ${filepath}...`, 'busy');
    try {
        const res = await fetch('/api/cad/ansys/parse', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ filepath })
        });
        const dataset = await res.json();
        if (revision === ansysParseRevision && thermal3DSource === 'csv') {
            handleANSYSResult(dataset, filepath.split('/').pop());
        }
    } catch (e) {
        if (revision === ansysParseRevision && thermal3DSource === 'csv') {
            AppState.showAlert(`Failed to parse ANSYS CSV: ${e.message}`, 'error');
            AppState.setStatus('Error parsing ANSYS CSV', 'error');
            setThermalStatus(document.getElementById('ansys-result-state'), 'error', 'Load failed');
        }
    } finally {
        const button = document.getElementById('ansys-parse-btn');
        if (button && revision === ansysParseRevision) button.disabled = false;
    }
}

async function parseANSYSContent(content, filename) {
    if (thermal3DSource !== 'csv') {
        document.getElementById('ansys-parse-btn').disabled = false;
        return;
    }
    const revision = ++ansysParseRevision;
    AppState.setStatus(`Parsing uploaded ANSYS CSV (${filename})...`, 'busy');
    try {
        const res = await fetch('/api/cad/ansys/parse', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ raw_content: content })
        });
        const dataset = await res.json();
        if (revision === ansysParseRevision && thermal3DSource === 'csv') {
            handleANSYSResult(dataset, filename);
        }
    } catch (e) {
        if (revision === ansysParseRevision && thermal3DSource === 'csv') {
            AppState.showAlert(`Error parsing ANSYS CSV: ${e.message}`, 'error');
            AppState.setStatus('Error parsing ANSYS CSV', 'error');
            setThermalStatus(document.getElementById('ansys-result-state'), 'error', 'Load failed');
        }
    } finally {
        const button = document.getElementById('ansys-parse-btn');
        if (button && revision === ansysParseRevision) button.disabled = false;
    }
}

function handleANSYSResult(dataset, sourceName = 'CSV dataset') {
    const metaCard = document.getElementById('ansys-meta-card');
    if (!dataset.is_valid) {
        metaCard.hidden = true;
        ansysRenderer.clearANSYSDataSet();
        AppState.showAlert(`ANSYS CSV Validation Rejection: ${dataset.error_message}`, 'error');
        AppState.setStatus('ANSYS CSV Validation Failed', 'error');
        setThermalStatus(document.getElementById('ansys-result-state'), 'error', 'Dataset rejected');
        document.getElementById('ansys-fit-btn').disabled = true;
        return;
    }

    AppState.hideAlert();
    metaCard.hidden = false;
    document.getElementById('ansys-meta-nodes').textContent = dataset.node_count.toLocaleString();
    document.getElementById('ansys-meta-range').textContent =
        `${dataset.min_temperature.toFixed(2)} °C to ${dataset.max_temperature.toFixed(2)} °C`;
    document.getElementById('ansys-meta-mean').textContent =
        `${dataset.mean_temperature.toFixed(2)} °C`;

    document.getElementById('legend-min-temp').textContent = `${dataset.min_temperature.toFixed(1)} °C`;
    document.getElementById('legend-mid-temp').textContent = `${((dataset.min_temperature + dataset.max_temperature) / 2).toFixed(1)} °C`;
    document.getElementById('legend-max-temp').textContent = `${dataset.max_temperature.toFixed(1)} °C`;

    ansysRenderer.setANSYSDataSet(dataset);
    document.getElementById('ansys-fit-btn').disabled = false;
    setThermal3DHeader('csv');
    setThermalStatus(document.getElementById('ansys-result-state'), 'ready', sourceName);
    AppState.setStatus(`ANSYS 3D Point Cloud Rendered: ${dataset.node_count} nodes`, 'ready');
}

function downloadBlob(content, filename, type) {
    const blob = new Blob([content], { type });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
}

