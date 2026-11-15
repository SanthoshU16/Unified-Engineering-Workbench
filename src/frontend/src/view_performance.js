import { AppState } from './app.js';

let lastBenchmarkResult = null;
let visualizationMode = 'time';
const MIN_REPETITIONS = 1;
const MAX_REPETITIONS = 50;

export function initPerformanceView() {
    const runBtn = document.getElementById('bench-run-btn');
    const exportBtn = document.getElementById('bench-export-btn');
    const resetBtn = document.getElementById('bench-reset-btn');

    runBtn.addEventListener('click', runBenchmark);
    exportBtn.addEventListener('click', exportBenchmarkCSV);
    resetBtn.addEventListener('click', resetBenchmark);

    document.querySelectorAll('[data-viz-mode]').forEach(button => {
        button.addEventListener('click', () => setVisualizationMode(button.dataset.vizMode));
    });
    renderVisualization(null, visualizationMode);
}

async function runBenchmark() {
    const num_cases = document.getElementById('bench-cases').valueAsNumber;
    const mesh_size = document.getElementById('bench-mesh').valueAsNumber;
    const seed = document.getElementById('bench-seed').valueAsNumber;
    const repetitions = Number(document.getElementById('bench-reps').value);

    if (!Number.isInteger(num_cases) || num_cases < 10 || !Number.isInteger(mesh_size) || mesh_size < 5 || !Number.isInteger(seed)) {
        AppState.showAlert('Enter whole numbers: at least 10 cases, at least 5 mesh elements, and a valid random seed.', 'warning');
        setRunState('error', 'Check workload values');
        return;
    }
    if (!Number.isInteger(repetitions) || repetitions < MIN_REPETITIONS || repetitions > MAX_REPETITIONS) {
        AppState.showAlert(`Measured repetitions must be between ${MIN_REPETITIONS} and ${MAX_REPETITIONS}.`, 'warning');
        setRunState('error', 'Check repetitions');
        return;
    }

    AppState.setStatus(`Executing Multicore Benchmark (${num_cases} cases, mesh ${mesh_size}, ${repetitions} reps)...`, 'busy');

    const runBtn = document.getElementById('bench-run-btn');
    runBtn.disabled = true;
    document.getElementById('bench-reset-btn').disabled = true;
    runBtn.setAttribute('aria-busy', 'true');
    runBtn.querySelector('.button-label').textContent = 'Running benchmark…';
    setRunState('busy', 'Benchmark running');

    try {
        const res = await fetch('/api/benchmark/run', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ num_cases, mesh_size, seed, repetitions })
        });
        const data = await res.json();
        if (!res.ok) {
            throw new Error(data.error || data.message || `Server returned HTTP ${res.status}`);
        }
        if (!Array.isArray(data.measurements) || data.measurements.length === 0) {
            throw new Error('The benchmark returned no measurements.');
        }
        lastBenchmarkResult = data;
        updateBenchmarkResults(data);
        setRunState(data.verification_passed ? 'success' : 'error', data.verification_passed ? 'Run complete' : 'Check failed');
        AppState.setStatus(`Benchmark Completed: Verification ${data.verification_passed ? 'PASSED' : 'FAILED'} (Max Diff: ${data.serial_parallel_max_diff.toExponential(2)} °C)`, 'ready');
    } catch (e) {
        AppState.showAlert(`Benchmark error: ${e.message}`, 'error');
        AppState.setStatus('Benchmark failed', 'error');
        setRunState('error', 'Run failed');
    } finally {
        runBtn.disabled = false;
        document.getElementById('bench-reset-btn').disabled = false;
        runBtn.removeAttribute('aria-busy');
        runBtn.querySelector('.button-label').textContent = 'Run benchmark';
    }
}

function resetBenchmark() {
    const resetBtn = document.getElementById('bench-reset-btn');
    if (resetBtn.disabled) return;

    document.getElementById('bench-cases').value = '2000';
    document.getElementById('bench-mesh').value = '100';
    document.getElementById('bench-seed').value = '42';
    document.getElementById('bench-reps').value = '5';
    lastBenchmarkResult = null;
    visualizationMode = 'time';

    document.getElementById('bench-kpi-runtime').textContent = '—';
    document.getElementById('bench-kpi-runtime-detail').textContent = 'Waiting for a benchmark';
    document.getElementById('bench-kpi-speedup').textContent = '—';
    document.getElementById('bench-kpi-speedup-detail').textContent = 'Relative to the serial run';
    document.getElementById('bench-kpi-workload').textContent = '—';
    document.getElementById('bench-kpi-workload-detail').textContent = 'Cases · mesh · repetitions';
    const checkKpi = document.getElementById('bench-kpi-check');
    checkKpi.textContent = '—';
    checkKpi.removeAttribute('data-state');
    document.getElementById('bench-kpi-check-detail').textContent = 'Serial compared with parallel';

    const badge = document.getElementById('bench-agreement-badge');
    badge.style.display = 'none';
    badge.removeAttribute('data-state');
    const assessmentNote = document.getElementById('bench-assessment-note');
    assessmentNote.style.display = 'none';
    assessmentNote.textContent = '';
    document.getElementById('bench-export-btn').hidden = true;
    document.getElementById('bench-table-summary').textContent = 'Repetition times and derived scaling metrics';

    const header = document.getElementById('bench-results-head');
    header.replaceChildren(...[
        'Worker threads', 'Rep 1 (ms)', 'Rep 2 (ms)', 'Rep 3 (ms)', 'Rep 4 (ms)',
        'Rep 5 (ms)', 'Median (ms)', 'Speedup', 'Efficiency'
    ].map(label => {
        const th = document.createElement('th');
        th.textContent = label;
        return th;
    }));
    document.getElementById('bench-results-tbody').innerHTML =
        '<tr><td colspan="9" class="empty-results">Run a benchmark to see repetition timings and scaling metrics.</td></tr>';

    document.getElementById('hw-ram').textContent = '—';
    document.getElementById('hw-bench-warmup').textContent = '—';
    document.getElementById('hw-launch').textContent = '—';
    setRunState('ready', 'Ready to run');
    document.querySelectorAll('[data-viz-mode]').forEach(button => {
        const selected = button.dataset.vizMode === visualizationMode;
        button.classList.toggle('active', selected);
        button.setAttribute('aria-pressed', String(selected));
    });
    AppState.hideAlert();
    renderVisualization(null, visualizationMode);
}

function setRunState(state, label) {
    const indicator = document.getElementById('bench-run-state');
    if (!indicator) return;
    indicator.dataset.state = state;
    indicator.lastElementChild.textContent = label;
}

function updateBenchmarkResults(data) {
    // 1. Agreement Badge
    const badge = document.getElementById('bench-agreement-badge');
    const assessmentNote = document.getElementById('bench-assessment-note');
    badge.style.display = 'inline-block';
    const maxDiff = data.serial_parallel_max_diff.toExponential(2);
    badge.dataset.state = data.verification_passed ? 'success' : 'error';
    if (data.verification_passed) {
        badge.textContent = `Verified · ΔT max ${maxDiff} °C`;
    } else {
        badge.textContent = `Verification failed · ΔT max ${maxDiff} °C`;
    }
    assessmentNote.style.display = data.speedup_assessable ? 'none' : 'block';
    assessmentNote.textContent = data.speedup_assessable ? ''
        : `Only ${data.logical_cpus} logical CPU is available. The ${data.verification_workers}-worker path passed its correctness check, but speedup and efficiency cannot be assessed.`;

    const formatInteger = value => new Intl.NumberFormat().format(value);
    const fastest = data.measurements.reduce((best, current) => current.median_time_ms < best.median_time_ms ? current : best);
    document.getElementById('bench-kpi-runtime').textContent = `${fastest.median_time_ms.toFixed(2)} ms`;
    document.getElementById('bench-kpi-runtime-detail').textContent = `Fastest result · ${fastest.workers} worker${fastest.workers === 1 ? '' : 's'}`;
    const parallelResults = data.measurements.filter(measurement => measurement.workers > 1);
    const peakSpeedup = parallelResults.reduce((best, current) => current.speedup > best.speedup ? current : best, parallelResults[0]);
    document.getElementById('bench-kpi-speedup').textContent = data.speedup_assessable && peakSpeedup
        ? `${peakSpeedup.speedup.toFixed(2)}×`
        : 'N/A';
    document.getElementById('bench-kpi-speedup-detail').textContent = data.speedup_assessable && peakSpeedup
        ? `At ${peakSpeedup.workers} workers · ${((peakSpeedup.efficiency || 0) * 100).toFixed(1)}% efficiency`
        : 'Not assessable on this CPU configuration';
    document.getElementById('bench-kpi-workload').textContent = `${formatInteger(data.num_cases)} cases`;
    document.getElementById('bench-kpi-workload-detail').textContent = `Mesh N = ${formatInteger(data.mesh_size)} · ${data.repetitions} repetitions`;
    const checkKpi = document.getElementById('bench-kpi-check');
    checkKpi.textContent = data.verification_passed ? 'PASS' : 'FAIL';
    checkKpi.dataset.state = data.verification_passed ? 'success' : 'error';
    document.getElementById('bench-kpi-check-detail').textContent = `Max difference ${maxDiff} °C · limit 1e-8 °C`;
    document.getElementById('bench-table-summary').textContent = `${data.repetitions} measured runs per worker · median shown in amber`;

    // 2. Hardware display
    const hwCpu = document.getElementById('hw-cpu');
    const hwCores = document.getElementById('hw-cores');
    const hwCompiler = document.getElementById('hw-compiler');
    const hwFlags = document.getElementById('hw-flags');
    if (hwCpu) hwCpu.textContent = data.cpu_model || 'Unavailable';
    if (hwCores) hwCores.textContent = `${data.physical_cpus} Physical / ${data.logical_cpus} Logical`;
    if (hwCompiler) hwCompiler.textContent = data.compiler_info || 'Unavailable';
    if (hwFlags) hwFlags.textContent = data.optimization_flags || 'Unavailable';
    const hwLaunch = document.getElementById('hw-launch');
    if (hwLaunch) hwLaunch.textContent = Number.isFinite(data.launch_to_ready_ms) ? `${data.launch_to_ready_ms.toFixed(1)} ms` : 'Unavailable';
    const hwRam = document.getElementById('hw-ram');
    if (hwRam) {
        hwRam.textContent = Number.isFinite(data.total_memory_bytes) && data.total_memory_bytes > 0
            ? `${(data.total_memory_bytes / (1024 ** 3)).toFixed(1)} GB`
            : 'Unavailable';
    }
    const warmup = document.getElementById('hw-bench-warmup');
    if (warmup) {
        warmup.textContent = data.warm_up_per_worker
            ? '1 untimed pass per worker count'
            : 'Disabled';
    }

    // 3. Results Table
    const repetitions = Math.max(...data.measurements.map(m => m.run_times_ms.length));
    const header = document.getElementById('bench-results-head');
    header.replaceChildren();
    const headers = ['Worker Threads'];
    for (let rep = 1; rep <= repetitions; rep++) headers.push(`Rep ${rep} (ms)`);
    headers.push('Median Time (ms)', 'Speedup (T1/Tp)', 'Efficiency');
    headers.forEach(label => {
        const th = document.createElement('th');
        th.textContent = label;
        header.appendChild(th);
    });

    const tbody = document.getElementById('bench-results-tbody');
    tbody.innerHTML = '';
    data.measurements.forEach(m => {
        const tr = document.createElement('tr');
        const addCell = (value, style = '') => {
            const td = document.createElement('td');
            td.textContent = value;
            if (style) td.style.cssText = style;
            tr.appendChild(td);
        };
        addCell(`${m.workers} Worker${m.workers > 1 ? 's' : ''}`, 'font-weight:bold; color:#f1f5f9;');
        for (let rep = 0; rep < repetitions; rep++) {
            const time = m.run_times_ms[rep];
            addCell(Number.isFinite(time) ? time.toFixed(2) : '-');
        }
        addCell(m.median_time_ms.toFixed(2), 'font-weight:bold; color:#d8a36b;');
        addCell(data.speedup_assessable ? `${m.speedup.toFixed(2)}x` : 'N/A', 'font-weight:bold; color:#e0b564;');
        addCell(data.speedup_assessable ? `${(m.efficiency * 100).toFixed(1)}%` : 'N/A', 'color:#99b184;');
        tbody.appendChild(tr);
    });

    renderVisualization(data, visualizationMode);

    // Show export button
    document.getElementById('bench-export-btn').hidden = false;
}

function setVisualizationMode(mode) {
    const allowedModes = ['time', 'speedup', 'efficiency', 'consistency'];
    if (!allowedModes.includes(mode)) return;
    visualizationMode = mode;
    document.querySelectorAll('[data-viz-mode]').forEach(button => {
        const selected = button.dataset.vizMode === mode;
        button.classList.toggle('active', selected);
        button.setAttribute('aria-pressed', String(selected));
    });
    renderVisualization(lastBenchmarkResult, mode);
}

function renderVisualization(data, mode) {
    const title = document.getElementById('benchmark-results-title');
    const description = document.getElementById('bench-visualization-description');
    const stage = document.getElementById('bench-visualization-stage');
    const copy = {
        time: ['Execution time', 'Compare median solve time as worker count increases.'],
        speedup: ['Speedup vs. ideal', 'Measured speedup compared with the maximum possible linear gain.'],
        efficiency: ['Parallel efficiency', 'How much of the added worker capacity is being used.'],
        consistency: ['Run consistency', 'Individual repetitions, timing range, and the median for each worker count.']
    }[mode];
    title.textContent = copy[0];
    description.textContent = copy[1];

    if (!data) {
        stage.innerHTML = `<div class="chart-empty-state" role="status">
          <span class="chart-empty-mark" aria-hidden="true"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5"><path d="M4 19V5m0 14h16M7 15l4-4 3 2 5-6"></path><circle cx="7" cy="15" r="1"></circle><circle cx="11" cy="11" r="1"></circle><circle cx="14" cy="13" r="1"></circle><circle cx="19" cy="7" r="1"></circle></svg></span>
          <strong>No benchmark results yet</strong>
          <span>Configure the workload above and run a benchmark. Switch views here to explore timing, scaling, and repeatability.</span>
        </div>`;
        return;
    }
    if ((mode === 'speedup' || mode === 'efficiency') && !data.speedup_assessable) {
        const heading = mode === 'speedup' ? 'Speedup not assessable' : 'Efficiency not assessable';
        const reason = data.logical_cpus === 1
            ? 'Only one logical CPU is available. The parallel path was checked for correctness, but no parallel run was timed.'
            : 'No parallel worker configuration was timed, so this metric cannot be assessed.';
        stage.innerHTML = `<div class="chart-mode-empty"><span class="chart-empty-mark" aria-hidden="true">i</span><strong>${heading}</strong><span>${reason}</span></div>`;
        return;
    }

    const renderers = {
        time: renderExecutionTime,
        speedup: renderSpeedup,
        efficiency: renderEfficiency,
        consistency: renderConsistency
    };
    stage.innerHTML = renderers[mode](data);
}

function chartSvg(width, height, label, children) {
    return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${width} ${height}" role="img" aria-label="${label}" focusable="false">${children}</svg>`;
}

function niceMaximum(value) {
    const raw = Math.max(value * 1.12, 1e-9);
    const magnitude = 10 ** Math.floor(Math.log10(raw));
    const fraction = raw / magnitude;
    const step = fraction <= 1 ? 1 : fraction <= 2 ? 2 : fraction <= 5 ? 5 : 10;
    return step * magnitude;
}

function renderExecutionTime(data) {
    const measurements = [...data.measurements].sort((a, b) => a.workers - b.workers);
    const allTimes = measurements.map(m => m.median_time_ms);
    const axisMax = niceMaximum(Math.max(...allTimes));
    const width = 980;
    const left = 142;
    const right = 170;
    const top = 54;
    const rowHeight = 78;
    const plotWidth = width - left - right;
    const height = top + measurements.length * rowHeight + 64;
    const scaleX = time => left + (time / axisMax) * plotWidth;
    const colors = ['#d8a36b', '#9cbb9d', '#87a8c5', '#c38eb3'];
    const parts = [];

    for (let i = 0; i <= 4; i++) {
        const value = axisMax * i / 4;
        const x = scaleX(value);
        parts.push(`<line x1="${x}" y1="${top - 10}" x2="${x}" y2="${height - 43}" stroke="#344037" stroke-width="1"/>`);
        parts.push(`<text x="${x}" y="${top - 19}" fill="#8c9990" font-size="11" text-anchor="middle" font-family="system-ui,sans-serif">${formatAxis(value)}</text>`);
    }
    parts.push(`<text x="${left + plotWidth / 2}" y="${height - 12}" fill="#9aa69c" font-size="12" text-anchor="middle" font-family="system-ui,sans-serif">Median solve time (ms) · lower is faster</text>`);

    measurements.forEach((measurement, index) => {
        const centerY = top + index * rowHeight + 18;
        const barWidth = Math.max(0, scaleX(measurement.median_time_ms) - left);
        const color = colors[index % colors.length];
        parts.push(`<line x1="12" y1="${centerY + 32}" x2="${width - 12}" y2="${centerY + 32}" stroke="#2a342d" stroke-width="1"/>`);
        parts.push(`<text x="16" y="${centerY + 5}" fill="#d8dfd5" font-size="13" font-weight="600" font-family="system-ui,sans-serif">${measurement.workers} worker${measurement.workers === 1 ? '' : 's'}</text>`);
        parts.push(`<rect x="${left}" y="${centerY - 13}" width="${plotWidth}" height="28" rx="7" fill="#202922"/>`);
        parts.push(`<rect x="${left}" y="${centerY - 13}" width="${barWidth}" height="28" rx="7" fill="${color}" opacity=".92"><title>${measurement.workers} worker${measurement.workers === 1 ? '' : 's'}: median ${measurement.median_time_ms.toFixed(2)} ms</title></rect>`);
        parts.push(`<text x="${width - 18}" y="${centerY - 1}" fill="#f0f1e8" font-size="13" font-weight="650" text-anchor="end" font-family="ui-monospace,monospace">${measurement.median_time_ms.toFixed(2)} ms</text>`);
        parts.push(`<text x="${width - 18}" y="${centerY + 17}" fill="#9aa69c" font-size="10" text-anchor="end" font-family="system-ui,sans-serif">${data.speedup_assessable ? `${measurement.speedup.toFixed(2)}× speedup` : 'speedup N/A'}</text>`);
    });
    return chartSvg(width, height, 'Horizontal bars compare median solve time across worker counts. Shorter bars are faster.', parts.join(''));
}

function renderSpeedup(data) {
    const measurements = [...data.measurements].sort((a, b) => a.workers - b.workers);
    const width = 900;
    const height = 420;
    const left = 76;
    const right = 34;
    const top = 70;
    const bottom = 72;
    const plotWidth = width - left - right;
    const plotHeight = height - top - bottom;
    const maxSpeedup = Math.max(4, Math.ceil(Math.max(...measurements.map(m => m.speedup))));
    const groupWidth = plotWidth / measurements.length;
    const barWidth = Math.min(48, groupWidth * .24);
    const gap = Math.max(10, groupWidth * .07);
    const y = value => top + plotHeight - (value / maxSpeedup) * plotHeight;
    const parts = [];

    for (let tick = 0; tick <= maxSpeedup; tick++) {
        const py = y(tick);
        parts.push(`<line x1="${left}" y1="${py}" x2="${width - right}" y2="${py}" stroke="#344037" stroke-width="1"/>`);
        parts.push(`<text x="${left - 10}" y="${py + 4}" fill="#89958c" font-size="11" text-anchor="end" font-family="system-ui,sans-serif">${tick}×</text>`);
    }
    parts.push(`<rect x="${left + 8}" y="22" width="11" height="11" rx="2" fill="#d8a36b"/><text x="${left + 25}" y="32" fill="#c6d0c5" font-size="11" font-family="system-ui,sans-serif">Measured speedup</text>`);
    parts.push(`<rect x="${left + 166}" y="22" width="11" height="11" rx="2" fill="#829689"/><text x="${left + 183}" y="32" fill="#c6d0c5" font-size="11" font-family="system-ui,sans-serif">Ideal linear speedup</text>`);

    measurements.forEach((measurement, index) => {
        const center = left + groupWidth * (index + .5);
        const measuredX = center - gap / 2 - barWidth;
        const idealX = center + gap / 2;
        const measuredTop = y(measurement.speedup);
        const idealTop = y(measurement.workers);
        parts.push(`<rect x="${measuredX}" y="${measuredTop}" width="${barWidth}" height="${top + plotHeight - measuredTop}" rx="5" fill="#d8a36b"><title>${measurement.workers} workers: measured speedup ${measurement.speedup.toFixed(2)}×</title></rect>`);
        parts.push(`<rect x="${idealX}" y="${idealTop}" width="${barWidth}" height="${top + plotHeight - idealTop}" rx="5" fill="#829689"><title>${measurement.workers} workers: ideal speedup ${measurement.workers.toFixed(2)}×</title></rect>`);
        parts.push(`<text x="${measuredX + barWidth / 2}" y="${measuredTop - 8}" fill="#edbc83" font-size="10" text-anchor="middle" font-family="ui-monospace,monospace">${measurement.speedup.toFixed(2)}×</text>`);
        parts.push(`<text x="${idealX + barWidth / 2}" y="${idealTop - 8}" fill="#a4b5a8" font-size="10" text-anchor="middle" font-family="ui-monospace,monospace">${measurement.workers.toFixed(2)}×</text>`);
        parts.push(`<text x="${center}" y="${top + plotHeight + 27}" fill="#c4cec3" font-size="11" text-anchor="middle" font-family="system-ui,sans-serif">${measurement.workers} worker${measurement.workers === 1 ? '' : 's'}</text>`);
    });
    parts.push(`<text x="${left + plotWidth / 2}" y="${height - 17}" fill="#9aa69c" font-size="11" text-anchor="middle" font-family="system-ui,sans-serif">More workers →</text>`);
    return chartSvg(width, height, 'Grouped columns compare measured speedup with ideal linear speedup for each worker count.', parts.join(''));
}

function renderEfficiency(data) {
    const measurements = [...data.measurements].sort((a, b) => a.workers - b.workers);
    const width = 980;
    const left = 142;
    const right = 212;
    const top = 58;
    const rowHeight = 78;
    const plotWidth = width - left - right;
    const height = top + measurements.length * rowHeight + 64;
    const colors = ['#a8c99f', '#87b9a0', '#d4b36f', '#c38eb3'];
    const maxEfficiency = Math.max(100, ...measurements.map(m => m.efficiency * 100));
    const axisMax = Math.max(100, Math.ceil(maxEfficiency / 25) * 25);
    const x = percent => left + plotWidth * (percent / axisMax);
    const parts = [];

    for (let tick = 0; tick <= 4; tick++) {
        const value = axisMax * tick / 4;
        const px = x(value);
        parts.push(`<line x1="${px}" y1="${top - 12}" x2="${px}" y2="${height - 43}" stroke="#344037" stroke-width="1"/>`);
        parts.push(`<text x="${px}" y="${top - 21}" fill="#8c9990" font-size="11" text-anchor="middle" font-family="system-ui,sans-serif">${formatAxis(value)}%</text>`);
    }
    const idealX = x(100);
    parts.push(`<line x1="${idealX}" y1="${top - 12}" x2="${idealX}" y2="${height - 43}" stroke="#e4ebdf" stroke-width="1.5" stroke-dasharray="5 5"/>`);
    parts.push(`<text x="${idealX + 6}" y="${top - 4}" fill="#d9e3d8" font-size="9" font-family="system-ui,sans-serif">Ideal 100%</text>`);
    parts.push(`<text x="${left + plotWidth / 2}" y="${height - 12}" fill="#9aa69c" font-size="12" text-anchor="middle" font-family="system-ui,sans-serif">Worker efficiency · 100% means ideal use</text>`);

    measurements.forEach((measurement, index) => {
        const centerY = top + index * rowHeight + 18;
        const efficiency = Math.max(0, measurement.efficiency * 100);
        const gapToIdeal = 100 - efficiency;
        const color = colors[index % colors.length];
        parts.push(`<line x1="12" y1="${centerY + 32}" x2="${width - 12}" y2="${centerY + 32}" stroke="#2a342d" stroke-width="1"/>`);
        parts.push(`<text x="16" y="${centerY + 5}" fill="#d8dfd5" font-size="13" font-weight="600" font-family="system-ui,sans-serif">${measurement.workers} worker${measurement.workers === 1 ? '' : 's'}</text>`);
        parts.push(`<rect x="${left}" y="${centerY - 10}" width="${plotWidth}" height="20" rx="7" fill="#202922"/>`);
        parts.push(`<rect x="${left}" y="${centerY - 10}" width="${plotWidth * efficiency / axisMax}" height="20" rx="7" fill="${color}"><title>${measurement.workers} workers: ${efficiency.toFixed(1)}% efficiency</title></rect>`);
        parts.push(`<text x="${width - 18}" y="${centerY - 1}" fill="#f0f1e8" font-size="13" font-weight="650" text-anchor="end" font-family="ui-monospace,monospace">${efficiency.toFixed(1)}%</text>`);
        const efficiencyNote = Math.abs(gapToIdeal) < .05
            ? 'ideal use'
            : gapToIdeal > 0 ? `${gapToIdeal.toFixed(1)} pts below ideal` : `${Math.abs(gapToIdeal).toFixed(1)} pts above ideal · timing variation`;
        parts.push(`<text x="${width - 18}" y="${centerY + 17}" fill="#9aa69c" font-size="10" text-anchor="end" font-family="system-ui,sans-serif">${efficiencyNote}</text>`);
    });
    return chartSvg(width, height, 'Horizontal progress bars show parallel efficiency from zero to one hundred percent.', parts.join(''));
}

function renderConsistency(data) {
    const measurements = data.measurements.filter(m => m.run_times_ms.length > 0);
    const width = 980;
    const left = 126;
    const right = 184;
    const top = 54;
    const rowHeight = 78;
    const plotWidth = width - left - right;
    const height = top + measurements.length * rowHeight + 64;
    const allTimes = measurements.flatMap(m => m.run_times_ms).filter(Number.isFinite);
    const axisMax = niceMaximum(Math.max(...allTimes));
    const scaleX = time => left + (time / axisMax) * plotWidth;
    const colors = ['#d8a36b', '#9cbb9d', '#87a8c5', '#c38eb3'];
    const parts = [];

    for (let tick = 0; tick <= 4; tick++) {
        const value = axisMax * tick / 4;
        const px = scaleX(value);
        parts.push(`<line x1="${px}" y1="${top - 11}" x2="${px}" y2="${height - 43}" stroke="#344037" stroke-width="1"/>`);
        parts.push(`<text x="${px}" y="${top - 20}" fill="#8c9990" font-size="11" text-anchor="middle" font-family="system-ui,sans-serif">${formatAxis(value)}</text>`);
    }
    parts.push(`<text x="${left + plotWidth / 2}" y="${height - 12}" fill="#9aa69c" font-size="12" text-anchor="middle" font-family="system-ui,sans-serif">Measured solve time (ms)</text>`);
    measurements.forEach((measurement, index) => {
        const centerY = top + index * rowHeight + 18;
        const times = measurement.run_times_ms.filter(Number.isFinite);
        const min = Math.min(...times);
        const max = Math.max(...times);
        const median = measurement.median_time_ms;
        const color = colors[index % colors.length];
        parts.push(`<line x1="12" y1="${centerY + 32}" x2="${width - 12}" y2="${centerY + 32}" stroke="#2a342d" stroke-width="1"/>`);
        parts.push(`<text x="16" y="${centerY + 5}" fill="#d8dfd5" font-size="13" font-weight="600" font-family="system-ui,sans-serif">${measurement.workers} worker${measurement.workers === 1 ? '' : 's'}</text>`);
        parts.push(`<line x1="${scaleX(min)}" y1="${centerY}" x2="${scaleX(max)}" y2="${centerY}" stroke="${color}" stroke-width="4" stroke-linecap="round" opacity=".65"/>`);
        parts.push(`<line x1="${scaleX(min)}" y1="${centerY - 8}" x2="${scaleX(min)}" y2="${centerY + 8}" stroke="${color}" stroke-width="1.5"/>`);
        parts.push(`<line x1="${scaleX(max)}" y1="${centerY - 8}" x2="${scaleX(max)}" y2="${centerY + 8}" stroke="${color}" stroke-width="1.5"/>`);
        times.forEach((time, repIndex) => {
            const jitter = times.length < 2 ? 0 : ((repIndex / (times.length - 1)) - .5) * 17;
            parts.push(`<circle cx="${scaleX(time)}" cy="${centerY + jitter}" r="4.5" fill="${color}" stroke="#172019" stroke-width="1.5"><title>Run ${repIndex + 1}: ${time.toFixed(2)} ms</title></circle>`);
        });
        parts.push(`<line x1="${scaleX(median)}" y1="${centerY - 16}" x2="${scaleX(median)}" y2="${centerY + 16}" stroke="#f0f1e8" stroke-width="2.5"><title>Median ${median.toFixed(2)} ms</title></line>`);
        parts.push(`<text x="${width - 14}" y="${centerY + 4}" fill="#e6b77f" font-size="11" text-anchor="end" font-family="ui-monospace,monospace">Median ${median.toFixed(2)} ms</text>`);
    });
    return chartSvg(width, height, 'Each dot is one measured repetition. Horizontal range marks minimum to maximum, and the white vertical marker is the median.', parts.join(''));
}

function formatAxis(value) {
    if (!Number.isFinite(value)) return '—';
    if (value === 0) return '0';
    const absolute = Math.abs(value);
    if (absolute >= 10000 || absolute < 0.01) return value.toExponential(1);
    return String(Number(value.toPrecision(3)));
}

function exportBenchmarkCSV() {
    if (!lastBenchmarkResult) return;
    const res = lastBenchmarkResult;
    const repetitions = Math.max(...res.measurements.map(m => m.run_times_ms.length));
    const columns = [
        'cpu_model', 'physical_cpus', 'logical_cpus', 'compiler', 'optimization_flags', 'launch_to_ready_ms',
        'total_memory_bytes', 'num_cases', 'mesh_size', 'seed', 'repetitions', 'warm_up_per_worker',
        'serial_parallel_max_diff_C', 'verification_passed', 'speedup_assessable',
        'verification_workers', 'workers',
        ...Array.from({ length: repetitions }, (_, index) => `rep${index + 1}_ms`),
        'median_ms', 'speedup', 'efficiency'
    ];
    const csvCell = value => `"${String(value ?? '').replace(/"/g, '""')}"`;
    const metadata = [
        res.cpu_model, res.physical_cpus, res.logical_cpus, res.compiler_info,
        res.optimization_flags, res.launch_to_ready_ms, res.total_memory_bytes, res.num_cases,
        res.mesh_size, res.seed, res.repetitions, res.warm_up_per_worker ? 'YES' : 'NO',
        res.serial_parallel_max_diff, res.verification_passed ? 'YES' : 'NO',
        res.speedup_assessable ? 'YES' : 'NO', res.verification_workers
    ];
    let csv = `${columns.join(',')}\n`;

    res.measurements.forEach(m => {
        const row = [...metadata, m.workers];
        for (let rep = 0; rep < repetitions; rep++) row.push(m.run_times_ms[rep] ?? '');
        row.push(m.median_time_ms, res.speedup_assessable ? m.speedup : '', res.speedup_assessable ? m.efficiency : '');
        csv += `${row.map(csvCell).join(',')}\n`;
    });

    const blob = new Blob(['\uFEFF', csv], { type: 'text/csv;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `benchmark_multicore_${res.num_cases}cases.csv`;
    a.hidden = true;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    window.setTimeout(() => URL.revokeObjectURL(url), 1000);
}

