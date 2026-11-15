import { Renderer3D } from './renderer3d.js';
import { AppState } from './app.js';

let renderer = null;
let currentModel = null;
let selectedUpload = null;
const MAX_STL_FILE_SIZE_BYTES = 150 * 1024 * 1024;

export function initGeometryView() {
    const canvas = document.getElementById('canvas-stl');
    if (!canvas) return;

    try {
        renderer = new Renderer3D('canvas-stl');
    } catch (error) {
        AppState.showAlert(`The model viewer could not start: ${error.message}`, 'error');
        AppState.setStatus('Viewer unavailable', 'error');
        return;
    }
    const fileSelect = document.getElementById('stl-file-select');
    const coordinateUnits = document.getElementById('stl-coordinate-units');
    const fileInput = document.getElementById('stl-file-input');
    const fileName = document.getElementById('stl-file-name');
    const dropzone = document.getElementById('stl-dropzone');
    const chooseBtn = document.getElementById('stl-choose-btn');
    const parseBtn = document.getElementById('stl-parse-btn');
    const fitBtn = document.getElementById('stl-fit-btn');
    const wireBtn = document.getElementById('stl-wire-btn');
    const modelBadge = document.getElementById('stl-model-badge');
    const viewPresetButtons = document.querySelectorAll('[data-stl-view]');

    fitBtn.disabled = true;
    wireBtn.disabled = true;
    fitBtn.addEventListener('click', () => {
        if (currentModel) renderer.fitToView(currentModel.bbox_min, currentModel.bbox_max, 1.25);
    });

    viewPresetButtons.forEach(button => {
        button.addEventListener('click', () => renderer.setViewPreset(button.dataset.stlView));
    });

    wireBtn.addEventListener('click', () => {
        if (!currentModel) return;
        renderer.wireframe = !renderer.wireframe;
        wireBtn.setAttribute('aria-pressed', String(renderer.wireframe));
        wireBtn.textContent = renderer.wireframe ? 'Shaded' : 'Wireframe';
        document.getElementById('stl-display-mode').textContent = renderer.wireframe ? 'Wireframe display' : 'Shaded surface';
        renderer.render();
    });

    coordinateUnits.addEventListener('change', () => {
        if (currentModel) updateSTLMetadata(currentModel, coordinateUnits.value);
    });

    chooseBtn.addEventListener('click', () => fileInput.click());
    fileInput.addEventListener('change', () => {
        const file = fileInput.files && fileInput.files[0];
        if (!file) return;
        selectUpload(file);
        fileInput.value = '';
    });

    dropzone.addEventListener('dragover', event => {
        event.preventDefault();
        dropzone.classList.add('dragging');
    });
    dropzone.addEventListener('dragleave', event => {
        if (!dropzone.contains(event.relatedTarget)) dropzone.classList.remove('dragging');
    });
    dropzone.addEventListener('drop', event => {
        event.preventDefault();
        dropzone.classList.remove('dragging');
        const file = event.dataTransfer && event.dataTransfer.files[0];
        if (!file) return;
        selectUpload(file);
    });

    parseBtn.addEventListener('click', loadSelectedSTL);
    fileSelect.addEventListener('change', () => {
        selectedUpload = null;
        fileInput.value = '';
        fileName.textContent = 'Drop a model here';
        fileName.title = '';
        coordinateUnits.value = fileSelect.selectedOptions[0]?.dataset.units || 'mm';
        loadSTLFilePath(fileSelect.value);
    });

    window.addEventListener('resize', () => {
        const view = document.getElementById('view-geometry');
        if (view && view.classList.contains('active')) renderer.resize();
    });

    const initialSample = fileSelect.querySelector('option[value]:not([value=""])');
    if (initialSample) {
        fileSelect.value = initialSample.value;
        coordinateUnits.value = initialSample.dataset.units || 'mm';
    }
    loadSTLFilePath(fileSelect.value);

    function selectUpload(file) {
        selectedUpload = file;
        fileName.textContent = file.name;
        fileName.title = file.name;
        fileSelect.value = '';
        setModelState('File selected', 'idle');
    }

    async function loadSelectedSTL() {
        if (selectedUpload) {
            const file = selectedUpload;
            if (!file.name.toLowerCase().endsWith('.stl')) {
                AppState.showAlert('Choose a file with the .stl extension.', 'warning');
                AppState.setStatus('Unsupported file type', 'error');
                setModelState('Unsupported file', 'error');
                return;
            }
            if (file.size > MAX_STL_FILE_SIZE_BYTES) {
                AppState.showAlert('This STL is larger than the 150 MiB limit. Choose a smaller file.', 'warning');
                AppState.setStatus('File exceeds the supported size limit', 'error');
                setModelState('File too large', 'error');
                return;
            }
            if (file.size === 0) {
                AppState.showAlert('This STL file is empty. Choose a model that contains triangle data.', 'error');
                AppState.setStatus('The selected file is empty', 'error');
                setModelState('Empty file', 'error');
                return;
            }

            setLoading(true);
            AppState.setStatus(`Loading ${file.name}`, 'busy');
            try {
                const bytes = await file.arrayBuffer();
                const response = await fetch('/api/cad/stl/parse', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/octet-stream' },
                    body: bytes
                });
                const model = await readSTLResponse(response);
                model.file_size_bytes = file.size;
                handleSTLResult(model);
            } catch (error) {
                AppState.showAlert(`Could not load ${file.name}: ${error.message}`, 'error');
                AppState.setStatus('Upload could not be read', 'error');
                setModelState('Import failed', 'error');
            } finally {
                setLoading(false);
            }
            return;
        }
        if (!fileSelect.value) {
            AppState.showAlert('Choose a sample model or browse for an STL file.', 'warning');
            AppState.setStatus('Choose a model to load', 'error');
            return;
        }
        loadSTLFilePath(fileSelect.value);
    }

    async function loadSTLFilePath(filepath) {
        setLoading(true);
        AppState.setStatus('Loading sample model', 'busy');
        try {
            const response = await fetch('/api/cad/stl/parse', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ filepath })
            });
            handleSTLResult(await readSTLResponse(response));
        } catch (error) {
            AppState.showAlert(`Could not load the selected model: ${error.message}`, 'error');
            AppState.setStatus('Model could not be loaded', 'error');
            setModelState('Import failed', 'error');
        } finally {
            setLoading(false);
        }
    }

    function setLoading(loading) {
        parseBtn.disabled = loading;
        parseBtn.setAttribute('aria-busy', String(loading));
        parseBtn.textContent = loading ? 'Importing…' : 'Load model';
        if (loading) setModelState('Validating', 'loading');
    }

    function setModelState(label, state) {
        modelBadge.textContent = label;
        modelBadge.className = `geometry-state is-${state}`;
    }
}

async function readSTLResponse(response) {
    const raw = await response.text();
    let model;
    try {
        model = JSON.parse(raw);
    } catch (_) {
        const details = raw.trim().slice(0, 180);
        throw new Error(details || `The server returned an unreadable response (${response.status}).`);
    }
    if (!response.ok && !Object.prototype.hasOwnProperty.call(model, 'is_valid')) {
        const error = new Error(model.error || `Request failed (${response.status}).`);
        error.serverMessage = model.error;
        throw error;
    }
    return model;
}

function handleSTLResult(model) {
    const metaCard = document.getElementById('stl-meta-card');
    if (!model || !model.is_valid) {
        const reason = model && model.error_message ? model.error_message : 'The file is not a supported STL model.';
        if (currentModel) {
            AppState.showAlert(`The selected file was not loaded. The previous model remains displayed. ${reason}`, 'error');
            document.getElementById('stl-model-badge').textContent = 'Previous model active';
            document.getElementById('stl-model-badge').className = 'geometry-state is-ready';
        } else {
            metaCard.style.display = 'none';
            AppState.showAlert(`Model not loaded: ${reason}`, 'error');
            document.getElementById('stl-model-badge').textContent = 'Validation failed';
            document.getElementById('stl-model-badge').className = 'geometry-state is-error';
        }
        AppState.setStatus('Model validation failed', 'error');
        return;
    }

    AppState.hideAlert();
    currentModel = model;
    metaCard.style.display = 'flex';
    document.getElementById('stl-fit-btn').disabled = false;
    document.getElementById('stl-wire-btn').disabled = false;
    document.getElementById('stl-model-badge').textContent = 'Validated';
    document.getElementById('stl-model-badge').className = 'geometry-state is-ready';

    updateSTLMetadata(model, document.getElementById('stl-coordinate-units').value);

    const wireBtn = document.getElementById('stl-wire-btn');
    wireBtn.disabled = model.wireframe_available === false;
    wireBtn.title = wireBtn.disabled ? 'Wireframe is unavailable for this model on this WebGL device.' : '';
    wireBtn.setAttribute('aria-pressed', 'false');
    wireBtn.textContent = 'Wireframe';
    document.getElementById('stl-display-mode').textContent = 'Shaded surface';
    renderer.wireframe = false;

    renderer.setSTLModel(model);
    AppState.setStatus(`Model loaded · ${model.triangle_count.toLocaleString()} triangles`, 'ready');
}

function updateSTLMetadata(model, coordinateUnit) {
    document.getElementById('stl-meta-name').textContent = model.solid_name || 'unnamed';
    document.getElementById('stl-meta-triangles').textContent = model.triangle_count.toLocaleString();
    const unitToMillimetres = { mm: 1, m: 1000, in: 25.4 }[coordinateUnit] || 1;
    const dims = model.dimensions;
    const dimensionText = `${(dims.x * unitToMillimetres).toFixed(1)} × ${(dims.y * unitToMillimetres).toFixed(1)} × ${(dims.z * unitToMillimetres).toFixed(1)} mm`;
    document.getElementById('stl-meta-dims').textContent = dimensionText;
    document.getElementById('stl-viewport-title').textContent = model.solid_name || 'Imported model';
    document.getElementById('stl-viewport-summary').textContent = `${model.triangle_count.toLocaleString()} surface triangles · ${dimensionText}`;
    document.getElementById('stl-viewport-facets').textContent = `${model.triangle_count.toLocaleString()} triangles`;
    document.getElementById('stl-meta-size').textContent =
        model.file_size_bytes >= 1024 * 1024
            ? `${(model.file_size_bytes / (1024 * 1024)).toFixed(2)} MiB`
            : `${(model.file_size_bytes / 1024).toFixed(1)} KiB`;
}
