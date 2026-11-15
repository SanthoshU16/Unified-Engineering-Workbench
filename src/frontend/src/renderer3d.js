// Zero-dependency WebGL 3D Engine for CAD (STL) and ANSYS Point Clouds
// Features: OrbitControls, Shaded Facets, Wireframe, Point Cloud Colormap, Raycasting Inspection

export class Renderer3D {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) throw new Error(`Canvas #${canvasId} not found`);
        this.transparentBackground = canvasId === 'canvas-ansys' || canvasId === 'canvas-stl';
        this.gl = this.canvas.getContext('webgl', { antialias: true, alpha: this.transparentBackground });
        if (!this.gl) throw new Error('WebGL not supported');
        this.pointOverlay = canvasId === 'canvas-ansys'
            ? document.getElementById('canvas-ansys-overlay')
            : null;
        this.pointOverlayContext = this.pointOverlay?.getContext('2d') || null;
        this.usePointOverlay = false;
        this.pointVisibilityChecked = false;

        this.camera = {
            rotX: 25 * Math.PI / 180,
            rotY: -45 * Math.PI / 180,
            distance: 0.25,
            target: [0, 0, 0],
            fov: 45 * Math.PI / 180
        };

        this.mode = 'stl'; // 'stl' or 'points'
        this.stlData = null;
        this.stlInterleavedBuffer = null;
        this.pointsData = null;
        this.thermalRod = null;
        this.thermalRodBuffers = null;
        this.wireframe = false;
        this.hoveredNode = null;
        this.selectedNode = null;
        this.pickingGrid = null;
        this.tooltipEl = null;

        this.initShaders();
        this.setupControls();
        if (typeof ResizeObserver !== 'undefined') {
            this.resizeObserver = new ResizeObserver(() => this.resize());
            this.resizeObserver.observe(this.canvas.parentElement);
        }
        this.resize();
    }

    setTooltipElement(el) {
        this.tooltipEl = el;
    }

    initShaders() {
        const gl = this.gl;

        // --- STL Shader (Position + Normal) ---
        const vsSTL = `
            attribute vec3 aPosition;
            attribute vec3 aNormal;
            uniform mat4 uMVP;
            uniform mat4 uModel;
            varying vec3 vNormal;
            varying vec3 vWorldPos;
            void main() {
                vNormal = aNormal;
                vWorldPos = (uModel * vec4(aPosition, 1.0)).xyz;
                gl_Position = uMVP * vec4(aPosition, 1.0);
            }
        `;
        const fsSTL = `
            precision mediump float;
            varying vec3 vNormal;
            varying vec3 vWorldPos;
            uniform vec3 uColor;
            uniform vec3 uLightDir;
            void main() {
                vec3 n = normalize(vNormal);
                float diff = max(dot(n, uLightDir), 0.0);
            float ambient = 0.48;
            vec3 lit = uColor * (ambient + diff * 0.58);
                gl_FragColor = vec4(lit, 1.0);
            }
        `;
        this.stlProgram = this.createProgram(vsSTL, fsSTL);

        // --- Point Cloud Shader (Position + Color) ---
        const vsPoints = `
            attribute vec3 aPosition;
            attribute vec3 aColor;
            uniform mat4 uMVP;
            uniform float uPointSize;
            varying vec3 vColor;
            void main() {
                vColor = aColor;
                gl_Position = uMVP * vec4(aPosition, 1.0);
                gl_PointSize = uPointSize;
            }
        `;
        const fsPoints = `
            precision mediump float;
            varying vec3 vColor;
            void main() {
                // Circle point shape
                vec2 coord = gl_PointCoord - vec2(0.5);
                if (length(coord) > 0.5) discard;
                gl_FragColor = vec4(vColor, 1.0);
            }
        `;
        this.pointsProgram = this.createProgram(vsPoints, fsPoints);

        const vsThermalRod = `
            attribute vec3 aPosition;
            attribute vec3 aNormal;
            attribute vec3 aColor;
            uniform mat4 uMVP;
            varying vec3 vNormal;
            varying vec3 vColor;
            void main() {
                vNormal = aNormal;
                vColor = aColor;
                gl_Position = uMVP * vec4(aPosition, 1.0);
            }
        `;
        const fsThermalRod = `
            precision mediump float;
            varying vec3 vNormal;
            varying vec3 vColor;
            void main() {
                vec3 n = normalize(vNormal);
                vec3 light = normalize(vec3(-0.35, 0.70, 0.85));
                float diffuse = max(dot(n, light), 0.0);
                float rim = pow(1.0 - max(dot(n, vec3(0.0, 0.0, 1.0)), 0.0), 2.0);
                vec3 shaded = vColor * (0.48 + diffuse * 0.58) + vec3(0.09, 0.08, 0.055) * rim;
                gl_FragColor = vec4(shaded, 1.0);
            }
        `;
        this.thermalRodProgram = this.createProgram(vsThermalRod, fsThermalRod);

        gl.enable(gl.DEPTH_TEST);
        gl.clearColor(0.067, 0.063, 0.094, this.transparentBackground ? 0.0 : 1.0);
    }

    createProgram(vsSrc, fsSrc) {
        const gl = this.gl;
        const vs = gl.createShader(gl.VERTEX_SHADER);
        gl.shaderSource(vs, vsSrc);
        gl.compileShader(vs);
        if (!gl.getShaderParameter(vs, gl.COMPILE_STATUS)) {
            const message = gl.getShaderInfoLog(vs) || 'Vertex shader compilation failed.';
            gl.deleteShader(vs);
            throw new Error(message);
        }

        const fs = gl.createShader(gl.FRAGMENT_SHADER);
        gl.shaderSource(fs, fsSrc);
        gl.compileShader(fs);
        if (!gl.getShaderParameter(fs, gl.COMPILE_STATUS)) {
            const message = gl.getShaderInfoLog(fs) || 'Fragment shader compilation failed.';
            gl.deleteShader(vs);
            gl.deleteShader(fs);
            throw new Error(message);
        }

        const prog = gl.createProgram();
        gl.attachShader(prog, vs);
        gl.attachShader(prog, fs);
        gl.linkProgram(prog);
        if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
            const message = gl.getProgramInfoLog(prog) || 'WebGL program could not be linked.';
            gl.deleteProgram(prog);
            gl.deleteShader(vs);
            gl.deleteShader(fs);
            throw new Error(message);
        }
        gl.deleteShader(vs);
        gl.deleteShader(fs);
        return prog;
    }

    setSTLModel(model) {
        this.mode = 'stl';
        this.stlModel = model;
        const gl = this.gl;
        this.wireframe = false;

        const positions = [];
        const normals = [];
        const wirePositions = [];

        for (const tri of model.triangles) {
            // tri: [nx, ny, nz, v1x, v1y, v1z, v2x, v2y, v2z, v3x, v3y, v3z]
            const nx = tri[0], ny = tri[1], nz = tri[2];
            for (let v = 0; v < 3; ++v) {
                positions.push(tri[3 + v * 3], tri[4 + v * 3], tri[5 + v * 3]);
                normals.push(nx, ny, nz);
            }
            const a = tri.slice(3, 6), b = tri.slice(6, 9), c = tri.slice(9, 12);
            wirePositions.push(...a, ...b, ...b, ...c, ...c, ...a);
        }

        if (this.stlPosBuffer) gl.deleteBuffer(this.stlPosBuffer);
        if (this.stlNormBuffer) gl.deleteBuffer(this.stlNormBuffer);
        if (this.stlWireBuffer) gl.deleteBuffer(this.stlWireBuffer);
        if (this.stlWireIndexBuffer) gl.deleteBuffer(this.stlWireIndexBuffer);
        if (this.stlInterleavedBuffer) gl.deleteBuffer(this.stlInterleavedBuffer);
        this.stlInterleavedBuffer = null;
        this.stlWireIndexBuffer = null;
        this.stlWireIndexCount = 0;
        this.stlVertexCount = positions.length / 3;
        this.stlWireVertexCount = wirePositions.length / 3;

        this.stlPosBuffer = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, this.stlPosBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.STATIC_DRAW);

        this.stlNormBuffer = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, this.stlNormBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(normals), gl.STATIC_DRAW);

        this.stlWireBuffer = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, this.stlWireBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(wirePositions), gl.STATIC_DRAW);

        this.fitToView(model.bbox_min, model.bbox_max, 1.25);
        this.render();
    }

    setSTLInterleavedModel(vertexData, wireIndices, triangleCount, bboxMin, bboxMax) {
        const gl = this.gl;
        this.mode = 'stl';
        this.wireframe = false;
        this.stlModel = null;

        if (this.stlPosBuffer) gl.deleteBuffer(this.stlPosBuffer);
        if (this.stlNormBuffer) gl.deleteBuffer(this.stlNormBuffer);
        if (this.stlWireBuffer) gl.deleteBuffer(this.stlWireBuffer);
        if (this.stlWireIndexBuffer) gl.deleteBuffer(this.stlWireIndexBuffer);
        if (this.stlInterleavedBuffer) gl.deleteBuffer(this.stlInterleavedBuffer);
        this.stlPosBuffer = null;
        this.stlNormBuffer = null;
        this.stlWireBuffer = null;
        this.stlWireVertexCount = 0;
        this.stlWireIndexBuffer = null;
        this.stlWireIndexCount = 0;

        this.stlInterleavedBuffer = gl.createBuffer();
        if (!this.stlInterleavedBuffer) throw new Error('The browser could not allocate a graphics buffer for this STL.');
        let glError = gl.getError();
        while (glError !== gl.NO_ERROR) glError = gl.getError();
        gl.bindBuffer(gl.ARRAY_BUFFER, this.stlInterleavedBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, vertexData, gl.STATIC_DRAW);
        const vertexUploadError = gl.getError();
        if (vertexUploadError !== gl.NO_ERROR) {
            gl.deleteBuffer(this.stlInterleavedBuffer);
            this.stlInterleavedBuffer = null;
            throw new Error('The model is too large for this browser graphics device. Reduce the STL triangle count and try again.');
        }

        if (wireIndices) {
            this.stlWireIndexBuffer = gl.createBuffer();
            if (this.stlWireIndexBuffer) {
                gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.stlWireIndexBuffer);
                gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, wireIndices, gl.STATIC_DRAW);
                if (gl.getError() === gl.NO_ERROR) {
                    this.stlWireIndexCount = wireIndices.length;
                } else {
                    gl.deleteBuffer(this.stlWireIndexBuffer);
                    this.stlWireIndexBuffer = null;
                }
            }
        }

        this.stlVertexCount = triangleCount * 3;
        this.fitToView(bboxMin, bboxMax);
        this.render();
        return Boolean(this.stlWireIndexBuffer);
    }

    supportsLargeWireframe() {
        return true;
    }

    setANSYSDataSet(dataset) {
        this.clearThermalRodBuffers();
        if (this.pointsPosBuffer) this.gl.deleteBuffer(this.pointsPosBuffer);
        if (this.pointsColBuffer) this.gl.deleteBuffer(this.pointsColBuffer);
        this.mode = 'points';
        this.usePointOverlay = false;
        this.pointVisibilityChecked = false;
        if (this.pointOverlay) this.pointOverlay.style.display = 'none';
        this.ansysDataSet = dataset;
        this.selectedNode = null;
        this.hoveredNode = null;
        this.pickingGrid = null;
        if (this.tooltipEl) this.tooltipEl.style.display = 'none';
        const gl = this.gl;

        const positions = [];
        const colors = [];
        const minT = dataset.min_temperature;
        const maxT = dataset.max_temperature;
        const rangeT = (maxT > minT) ? (maxT - minT) : 1.0;

        for (const n of dataset.nodes) {
            // n: [node_id, x, y, z, temp]
            positions.push(n[1], n[2], n[3]);
            const normT = Math.max(0.0, Math.min(1.0, (n[4] - minT) / rangeT));
            const rgb = this.thermalColor(normT);
            colors.push(rgb[0], rgb[1], rgb[2]);
        }

        this.pointsCount = dataset.nodes.length;

        this.pointsPosBuffer = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, this.pointsPosBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.STATIC_DRAW);

        this.pointsColBuffer = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, this.pointsColBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(colors), gl.STATIC_DRAW);

        this.fitToView(dataset.bbox_min, dataset.bbox_max);
        this.render();
    }

    setThermalRod(result) {
        if (!result?.success || !result.nodes?.length || !result.config?.L) return;
        const gl = this.gl;
        this.clearThermalRodBuffers();
        if (this.pointsPosBuffer) gl.deleteBuffer(this.pointsPosBuffer);
        if (this.pointsColBuffer) gl.deleteBuffer(this.pointsColBuffer);
        this.pointsPosBuffer = null;
        this.pointsColBuffer = null;
        this.pointsCount = 0;
        this.mode = 'thermalRod';
        this.usePointOverlay = false;
        if (this.pointOverlay) this.pointOverlay.style.display = 'none';
        this.ansysDataSet = null;
        this.thermalRod = result;
        this.hoveredNode = null;
        this.selectedNode = null;
        this.pickingGrid = null;
        if (this.tooltipEl) this.tooltipEl.style.display = 'none';

        // This is a schematic solid for a 1D model. Its radius is enlarged
        // relative to length so the axial temperature field reads clearly.
        const length = result.config.L;
        const radius = length * 0.035;
        const positions = [];
        const normals = [];
        const colors = [];
        const meshLinePositions = [];
        const meshLineNormals = [];
        let minT = Infinity;
        let maxT = -Infinity;
        for (const node of result.nodes) {
            minT = Math.min(minT, node.T_fea);
            maxT = Math.max(maxT, node.T_fea);
        }
        const tempRange = Math.max(1e-12, maxT - minT);
        const radialSegments = 32;
        const colorAt = temperature => this.thermalColor((temperature - minT) / tempRange);

        for (let i = 0; i < result.nodes.length - 1; ++i) {
            const left = result.nodes[i];
            const right = result.nodes[i + 1];
            const c0 = colorAt(left.T_fea);
            const c1 = colorAt(right.T_fea);
            for (let j = 0; j < radialSegments; ++j) {
                const a0 = (j / radialSegments) * Math.PI * 2;
                const a1 = ((j + 1) / radialSegments) * Math.PI * 2;
                const p00 = [left.x,  Math.cos(a0) * radius, Math.sin(a0) * radius];
                const p01 = [left.x,  Math.cos(a1) * radius, Math.sin(a1) * radius];
                const p10 = [right.x, Math.cos(a0) * radius, Math.sin(a0) * radius];
                const p11 = [right.x, Math.cos(a1) * radius, Math.sin(a1) * radius];
                const n0 = [0, Math.cos(a0), Math.sin(a0)];
                const n1 = [0, Math.cos(a1), Math.sin(a1)];
                this.pushThermalRodVertex(positions, normals, colors, p00, n0, c0);
                this.pushThermalRodVertex(positions, normals, colors, p10, n0, c1);
                this.pushThermalRodVertex(positions, normals, colors, p11, n1, c1);
                this.pushThermalRodVertex(positions, normals, colors, p00, n0, c0);
                this.pushThermalRodVertex(positions, normals, colors, p11, n1, c1);
                this.pushThermalRodVertex(positions, normals, colors, p01, n1, c0);
            }
        }

        // A fine circumferential ring at each FEA node communicates the mesh
        // without the bead-like appearance of oversized point markers.
        const meshLineRadius = radius * 1.006;
        for (const node of result.nodes) {
            for (let j = 0; j < radialSegments; ++j) {
                const a0 = (j / radialSegments) * Math.PI * 2;
                const a1 = ((j + 1) / radialSegments) * Math.PI * 2;
                const normal0 = [0, Math.cos(a0), Math.sin(a0)];
                const normal1 = [0, Math.cos(a1), Math.sin(a1)];
                meshLinePositions.push(node.x, Math.cos(a0) * meshLineRadius, Math.sin(a0) * meshLineRadius);
                meshLinePositions.push(node.x, Math.cos(a1) * meshLineRadius, Math.sin(a1) * meshLineRadius);
                meshLineNormals.push(...normal0, ...normal1);
            }
        }

        const firstNode = result.nodes[0];
        const lastNode = result.nodes[result.nodes.length - 1];
        const firstColor = colorAt(firstNode.T_fea);
        const lastColor = colorAt(lastNode.T_fea);
        for (let j = 0; j < radialSegments; ++j) {
            const a0 = (j / radialSegments) * Math.PI * 2;
            const a1 = ((j + 1) / radialSegments) * Math.PI * 2;
            const firstP0 = [firstNode.x, Math.cos(a0) * radius, Math.sin(a0) * radius];
            const firstP1 = [firstNode.x, Math.cos(a1) * radius, Math.sin(a1) * radius];
            const lastP0 = [lastNode.x, Math.cos(a0) * radius, Math.sin(a0) * radius];
            const lastP1 = [lastNode.x, Math.cos(a1) * radius, Math.sin(a1) * radius];
            this.pushThermalRodVertex(positions, normals, colors, [firstNode.x, 0, 0], [-1, 0, 0], firstColor);
            this.pushThermalRodVertex(positions, normals, colors, firstP1, [-1, 0, 0], firstColor);
            this.pushThermalRodVertex(positions, normals, colors, firstP0, [-1, 0, 0], firstColor);
            this.pushThermalRodVertex(positions, normals, colors, [lastNode.x, 0, 0], [1, 0, 0], lastColor);
            this.pushThermalRodVertex(positions, normals, colors, lastP0, [1, 0, 0], lastColor);
            this.pushThermalRodVertex(positions, normals, colors, lastP1, [1, 0, 0], lastColor);
        }

        const positionBuffer = gl.createBuffer();
        const normalBuffer = gl.createBuffer();
        const colorBuffer = gl.createBuffer();
        const meshLinePositionBuffer = gl.createBuffer();
        const meshLineNormalBuffer = gl.createBuffer();
        if (!positionBuffer || !normalBuffer || !colorBuffer || !meshLinePositionBuffer || !meshLineNormalBuffer) {
            [positionBuffer, normalBuffer, colorBuffer, meshLinePositionBuffer, meshLineNormalBuffer].forEach(buffer => buffer && gl.deleteBuffer(buffer));
            throw new Error('The browser could not allocate graphics buffers for the rod visualization.');
        }
        gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.STATIC_DRAW);
        gl.bindBuffer(gl.ARRAY_BUFFER, normalBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(normals), gl.STATIC_DRAW);
        gl.bindBuffer(gl.ARRAY_BUFFER, colorBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(colors), gl.STATIC_DRAW);
        gl.bindBuffer(gl.ARRAY_BUFFER, meshLinePositionBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(meshLinePositions), gl.STATIC_DRAW);
        gl.bindBuffer(gl.ARRAY_BUFFER, meshLineNormalBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(meshLineNormals), gl.STATIC_DRAW);

        this.thermalRodBuffers = {
            position: positionBuffer,
            normal: normalBuffer,
            color: colorBuffer,
            meshLinePosition: meshLinePositionBuffer,
            meshLineNormal: meshLineNormalBuffer,
            vertexCount: positions.length / 3,
            meshLineVertexCount: meshLinePositions.length / 3
        };
        this.thermalRodBounds = {
            min: { x: 0, y: -radius, z: -radius },
            max: { x: length, y: radius, z: radius }
        };
        this.fitToView(this.thermalRodBounds.min, this.thermalRodBounds.max);
        this.render();
    }

    pushThermalRodVertex(positions, normals, colors, point, normal, color) {
        positions.push(...point);
        normals.push(...normal);
        colors.push(...color);
    }

    clearThermalRodBuffers() {
        if (!this.thermalRodBuffers) return;
        Object.values(this.thermalRodBuffers).forEach(buffer => {
            if (buffer && typeof buffer !== 'number') this.gl.deleteBuffer(buffer);
        });
        this.thermalRodBuffers = null;
        this.thermalRod = null;
        this.thermalRodBounds = null;
    }

    clearANSYSDataSet() {
        this.clearThermalRodBuffers();
        if (this.pointsPosBuffer) this.gl.deleteBuffer(this.pointsPosBuffer);
        if (this.pointsColBuffer) this.gl.deleteBuffer(this.pointsColBuffer);
        this.pointsPosBuffer = null;
        this.pointsColBuffer = null;
        this.pointsCount = 0;
        this.ansysDataSet = null;
        this.usePointOverlay = false;
        this.pointVisibilityChecked = false;
        if (this.pointOverlay) this.pointOverlay.style.display = 'none';
        this.hoveredNode = null;
        this.selectedNode = null;
        this.pickingGrid = null;
        if (this.tooltipEl) this.tooltipEl.style.display = 'none';
        this.mode = 'points';
        this.render();
    }

    thermalColor(t) {
        // Warm thermal scale: mineral green -> brass -> amber -> terracotta.
        const stops = [
            [0.00, [0.34, 0.43, 0.32]],
            [0.35, [0.56, 0.61, 0.36]],
            [0.68, [0.83, 0.64, 0.31]],
            [1.00, [0.79, 0.36, 0.24]]
        ];
        const upper = stops.findIndex(([stop]) => stop >= t);
        const right = stops[Math.max(1, upper)];
        const left = stops[Math.max(0, upper - 1)];
        const fraction = (t - left[0]) / Math.max(1e-9, right[0] - left[0]);
        return left[1].map((channel, index) => channel + (right[1][index] - channel) * fraction);
    }

    setViewPreset(preset) {
        const presets = {
            iso: [27 * Math.PI / 180, -42 * Math.PI / 180],
            front: [0, 0],
            top: [Math.PI / 2 - 0.02, 0],
            right: [0, Math.PI / 2]
        };
        const view = presets[preset];
        if (!view) return;
        this.camera.rotX = view[0];
        this.camera.rotY = view[1];
        this.render();
    }

    fitToView(bboxMin, bboxMax, padding = 1.6) {
        if (!bboxMin || !bboxMax) return;
        const cx = (bboxMin.x + bboxMax.x) * 0.5;
        const cy = (bboxMin.y + bboxMax.y) * 0.5;
        const cz = (bboxMin.z + bboxMax.z) * 0.5;
        this.camera.target = [cx, cy, cz];

        const dx = bboxMax.x - bboxMin.x;
        const dy = bboxMax.y - bboxMin.y;
        const dz = bboxMax.z - bboxMin.z;
        const diag = Math.sqrt(dx * dx + dy * dy + dz * dz);
        this.camera.distance = Math.max(0.02, diag * padding);
        this.render();
    }

    setupControls() {
        let isDragging = false;
        let isPanning = false;
        let lastX = 0, lastY = 0;

        this.canvas.style.touchAction = 'none';
        this.canvas.addEventListener('pointerdown', (e) => {
            lastX = e.clientX;
            lastY = e.clientY;
            if (e.button === 2 || e.shiftKey) {
                isPanning = true;
            } else if (e.button === 0) {
                isDragging = true;
            }
            if (isDragging || isPanning) this.canvas.setPointerCapture(e.pointerId);
        });

        window.addEventListener('pointermove', (e) => {
            const dx = e.clientX - lastX;
            const dy = e.clientY - lastY;
            lastX = e.clientX;
            lastY = e.clientY;

            if (isDragging) {
                this.camera.rotY += dx * 0.008;
                this.camera.rotX += dy * 0.008;
                this.camera.rotX = Math.max(-Math.PI / 2 + 0.01, Math.min(Math.PI / 2 - 0.01, this.camera.rotX));
                this.render();
            } else if (isPanning) {
                const panSpeed = this.camera.distance * 0.0015;
                const cosY = Math.cos(this.camera.rotY);
                const sinY = Math.sin(this.camera.rotY);
                this.camera.target[0] -= (dx * cosY) * panSpeed;
                this.camera.target[1] += dy * panSpeed;
                this.camera.target[2] -= (-dx * sinY) * panSpeed;
                this.render();
            } else if (this.mode === 'points' && this.ansysDataSet) {
                this.inspectNodeUnderMouse(e);
            }
        });

        window.addEventListener('pointerup', () => {
            isDragging = false;
            isPanning = false;
        });

        this.canvas.addEventListener('click', (e) => {
            if (this.mode === 'points' && this.ansysDataSet) this.selectNodeUnderMouse(e);
        });

        this.canvas.addEventListener('wheel', (e) => {
            e.preventDefault();
            const factor = e.deltaY > 0 ? 1.1 : 0.9;
            this.camera.distance *= factor;
            this.camera.distance = Math.max(0.005, Math.min(100.0, this.camera.distance));
            this.render();
        }, { passive: false });

        this.canvas.addEventListener('contextmenu', (e) => e.preventDefault());
    }

    inspectNodeUnderMouse(e) {
        if (!this.ansysDataSet || !this.tooltipEl) return;
        const rect = this.canvas.getBoundingClientRect();
        const mx = e.clientX - rect.left;
        const my = e.clientY - rect.top;

        if (mx < 0 || mx > rect.width || my < 0 || my > rect.height) {
            this.hoveredNode = null;
            this.showNodeTooltip(this.selectedNode);
            return;
        }

        this.hoveredNode = this.findNodeAtScreenPosition(mx, my, rect.width, rect.height);
        this.showNodeTooltip(this.hoveredNode || this.selectedNode);
    }

    selectNodeUnderMouse(e) {
        if (!this.ansysDataSet || !this.tooltipEl) return;
        const rect = this.canvas.getBoundingClientRect();
        const mx = e.clientX - rect.left;
        const my = e.clientY - rect.top;
        this.selectedNode = this.findNodeAtScreenPosition(mx, my, rect.width, rect.height);
        this.hoveredNode = null;
        this.showNodeTooltip(this.selectedNode);
    }

    findNodeAtScreenPosition(mx, my, width, height) {
        if (mx < 0 || mx > width || my < 0 || my > height) return null;
        const bucketSize = 24;
        if (!this.pickingGrid || this.pickingGrid.width !== width || this.pickingGrid.height !== height) {
            const bins = new Map();
            const mvp = this.computeMVPMatrix();
            for (const n of this.ansysDataSet.nodes) {
                const screen = this.projectPoint(n[1], n[2], n[3], mvp, width, height);
                if (screen.z < -1 || screen.z > 1 || screen.x < 0 || screen.x > width || screen.y < 0 || screen.y > height) continue;
                const key = `${Math.floor(screen.x / bucketSize)}:${Math.floor(screen.y / bucketSize)}`;
                if (!bins.has(key)) bins.set(key, []);
                bins.get(key).push({ node: n, x: screen.x, y: screen.y });
            }
            this.pickingGrid = { width, height, bucketSize, bins };
        }

        let closestNode = null;
        let minDist = 18.0; // pixel threshold
        const minCellX = Math.floor((mx - minDist) / bucketSize);
        const maxCellX = Math.floor((mx + minDist) / bucketSize);
        const minCellY = Math.floor((my - minDist) / bucketSize);
        const maxCellY = Math.floor((my + minDist) / bucketSize);
        for (let cellX = minCellX; cellX <= maxCellX; cellX++) {
            for (let cellY = minCellY; cellY <= maxCellY; cellY++) {
                const candidates = this.pickingGrid.bins.get(`${cellX}:${cellY}`) || [];
                for (const candidate of candidates) {
                    const dist = Math.hypot(mx - candidate.x, my - candidate.y);
                    if (dist < minDist) {
                        minDist = dist;
                        const n = candidate.node;
                        closestNode = { id: n[0], x: n[1], y: n[2], z: n[3], temp: n[4] };
                    }
                }
            }
        }
        return closestNode;
    }

    showNodeTooltip(node) {
        if (!this.tooltipEl) return;
        if (!node || !this.ansysDataSet) {
            this.tooltipEl.style.display = 'none';
            return;
        }

        const rect = this.canvas.getBoundingClientRect();
        const screen = this.projectPoint(node.x, node.y, node.z, this.computeMVPMatrix(), rect.width, rect.height);
        if (screen.z < -1 || screen.z > 1) {
            this.tooltipEl.style.display = 'none';
            return;
        }
        this.tooltipEl.style.display = 'block';
        this.tooltipEl.style.left = `${rect.left + screen.x + 15}px`;
        this.tooltipEl.style.top = `${rect.top + screen.y - 20}px`;
        this.tooltipEl.innerHTML = `
            <div style="font-weight:bold; color:#d8a36b;">Node ID: ${node.id}</div>
            <div>X: ${node.x.toFixed(4)} m</div>
            <div>Y: ${node.y.toFixed(4)} m</div>
            <div>Z: ${node.z.toFixed(4)} m</div>
            <div style="font-weight:bold; color:#f59e0b; margin-top:2px;">Temp: ${node.temp.toFixed(2)} °C</div>
        `;
    }

    projectPoint(x, y, z, m, width, height) {
        const px = m[0]*x + m[4]*y + m[8]*z + m[12];
        const py = m[1]*x + m[5]*y + m[9]*z + m[13];
        const pz = m[2]*x + m[6]*y + m[10]*z + m[14];
        const pw = m[3]*x + m[7]*y + m[11]*z + m[15];
        if (pw === 0) return { x: 0, y: 0, z: -1 };
        const ndcX = px / pw;
        const ndcY = py / pw;
        const ndcZ = pz / pw;
        return {
            x: (ndcX * 0.5 + 0.5) * width,
            y: (-ndcY * 0.5 + 0.5) * height,
            z: ndcZ
        };
    }

    resize() {
        const rect = this.canvas.parentElement.getBoundingClientRect();
        if (rect.width < 1 || rect.height < 1) return;
        const dpr = window.devicePixelRatio || 1;
        this.width = rect.width;
        this.height = rect.height;
        this.canvas.width = Math.round(this.width * dpr);
        this.canvas.height = Math.round(this.height * dpr);
        if (this.pointOverlay) {
            this.pointOverlay.width = this.canvas.width;
            this.pointOverlay.height = this.canvas.height;
            this.pointOverlay.style.width = `${this.width}px`;
            this.pointOverlay.style.height = `${this.height}px`;
            this.pointOverlayContext?.setTransform(dpr, 0, 0, dpr, 0, 0);
        }
        this.gl.viewport(0, 0, this.canvas.width, this.canvas.height);
        this.render();
    }

    pointCloudIsVisible(mvp) {
        if (!this.ansysDataSet?.nodes?.length) return false;
        const gl = this.gl;
        const pixel = new Uint8Array(4);
        const nodes = this.ansysDataSet.nodes;
        const checks = Math.min(12, nodes.length);
        const dpr = window.devicePixelRatio || 1;
        gl.finish();
        for (let i = 0; i < checks; ++i) {
            const node = nodes[Math.floor((i + 0.5) * nodes.length / checks)];
            const screen = this.projectPoint(node[1], node[2], node[3], mvp, this.width, this.height);
            if (screen.z < -1 || screen.z > 1 || screen.x < 0 || screen.x >= this.width || screen.y < 0 || screen.y >= this.height) continue;
            const px = Math.floor(screen.x * dpr);
            const py = this.canvas.height - 1 - Math.floor(screen.y * dpr);
            gl.readPixels(px, py, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
            // The clear color is near-black. A visible thermal point should differ
            // strongly from it even after antialiasing and color quantization.
            if (pixel[0] > 35 || pixel[1] > 38 || pixel[2] > 32) return true;
        }
        return false;
    }

    renderPointCloudOverlay(mvp) {
        if (!this.pointOverlay || !this.pointOverlayContext || !this.ansysDataSet) return;
        const ctx = this.pointOverlayContext;
        const width = this.width;
        const height = this.height;
        const depthBands = 12;
        const tempBands = 24;
        const paths = Array.from({ length: depthBands * tempBands }, () => new Path2D());
        const minT = this.ansysDataSet.min_temperature;
        const tempRange = Math.max(1e-12, this.ansysDataSet.max_temperature - minT);
        const nodes = this.ansysDataSet.nodes;
        const dpr = window.devicePixelRatio || 1;
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        ctx.clearRect(0, 0, width, height);

        for (const node of nodes) {
            const screen = this.projectPoint(node[1], node[2], node[3], mvp, width, height);
            if (screen.z < -1 || screen.z > 1 || screen.x < 0 || screen.x >= width || screen.y < 0 || screen.y >= height) continue;
            const depthBand = Math.min(depthBands - 1, Math.floor(((screen.z + 1) / 2) * depthBands));
            const normalizedTemp = Math.max(0, Math.min(1, (node[4] - minT) / tempRange));
            const tempBand = Math.min(tempBands - 1, Math.floor(normalizedTemp * tempBands));
            paths[depthBand * tempBands + tempBand].moveTo(screen.x + 3, screen.y);
            paths[depthBand * tempBands + tempBand].arc(screen.x, screen.y, 3, 0, Math.PI * 2);
        }

        // Draw distant nodes first so nearer ones remain visually in front.
        for (let depthBand = depthBands - 1; depthBand >= 0; --depthBand) {
            for (let tempBand = 0; tempBand < tempBands; ++tempBand) {
                const normalizedTemp = (tempBand + 0.5) / tempBands;
                const color = this.thermalColor(normalizedTemp).map(channel => Math.round(channel * 255));
                ctx.fillStyle = `rgb(${color[0]} ${color[1]} ${color[2]})`;
                ctx.fill(paths[depthBand * tempBands + tempBand]);
            }
        }
        this.pointOverlay.style.display = 'block';
    }

    computeMVPMatrix() {
        const aspect = this.canvas.width / this.canvas.height;
        const near = Math.max(0.00001, this.camera.distance * 0.0005);
        const far = Math.max(1000, this.camera.distance * 24);
        const pMat = this.perspectiveMatrix(this.camera.fov, aspect, near, far);

        // Camera position
        const cosX = Math.cos(this.camera.rotX), sinX = Math.sin(this.camera.rotX);
        const cosY = Math.cos(this.camera.rotY), sinY = Math.sin(this.camera.rotY);
        const eye = [
            this.camera.target[0] + this.camera.distance * sinY * cosX,
            this.camera.target[1] + this.camera.distance * sinX,
            this.camera.target[2] + this.camera.distance * cosY * cosX
        ];

        const vMat = this.lookAtMatrix(eye, this.camera.target, [0, 1, 0]);
        return this.multiplyMatrices(pMat, vMat);
    }

    render() {
        this.pickingGrid = null;
        const gl = this.gl;
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.viewport(0, 0, this.canvas.width, this.canvas.height);
        gl.depthMask(true);
        gl.depthFunc(gl.LESS);
        gl.enable(gl.DEPTH_TEST);
        gl.disable(gl.CULL_FACE);
        gl.disable(gl.BLEND);
        gl.disable(gl.SCISSOR_TEST);
        gl.colorMask(true, true, true, true);
        gl.clearDepth(1.0);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

        const mvp = this.computeMVPMatrix();
        if (this.pointOverlayContext) {
            this.pointOverlayContext.setTransform(window.devicePixelRatio || 1, 0, 0, window.devicePixelRatio || 1, 0, 0);
            this.pointOverlayContext.clearRect(0, 0, this.width, this.height);
        }

        if (this.mode === 'stl' && (this.stlPosBuffer || this.stlInterleavedBuffer)) {
            gl.useProgram(this.stlProgram);
            const uMVP = gl.getUniformLocation(this.stlProgram, 'uMVP');
            const uModel = gl.getUniformLocation(this.stlProgram, 'uModel');
            const uColor = gl.getUniformLocation(this.stlProgram, 'uColor');
            const uLightDir = gl.getUniformLocation(this.stlProgram, 'uLightDir');

            gl.uniformMatrix4fv(uMVP, false, mvp);
            gl.uniformMatrix4fv(uModel, false, this.identityMatrix());
            gl.uniform3f(uColor, 0.74, 0.68, 0.58); // Neutral warm display material; STL contains no material data.

            // Light from camera orientation
            const lx = Math.sin(this.camera.rotY);
            const ly = 0.8;
            const lz = Math.cos(this.camera.rotY);
            const llen = Math.hypot(lx, ly, lz);
            gl.uniform3f(uLightDir, lx / llen, ly / llen, lz / llen);

            const aPos = gl.getAttribLocation(this.stlProgram, 'aPosition');
            const aNorm = gl.getAttribLocation(this.stlProgram, 'aNormal');
            if (this.stlInterleavedBuffer) {
                gl.bindBuffer(gl.ARRAY_BUFFER, this.stlInterleavedBuffer);
                gl.enableVertexAttribArray(aPos);
                gl.vertexAttribPointer(aPos, 3, gl.FLOAT, false, 24, 0);
                gl.enableVertexAttribArray(aNorm);
                gl.vertexAttribPointer(aNorm, 3, gl.FLOAT, false, 24, 12);
            } else {
                gl.bindBuffer(gl.ARRAY_BUFFER, this.stlPosBuffer);
                gl.enableVertexAttribArray(aPos);
                gl.vertexAttribPointer(aPos, 3, gl.FLOAT, false, 0, 0);
                gl.bindBuffer(gl.ARRAY_BUFFER, this.stlNormBuffer);
                gl.enableVertexAttribArray(aNorm);
                gl.vertexAttribPointer(aNorm, 3, gl.FLOAT, false, 0, 0);
            }

            if (this.wireframe && this.stlWireIndexBuffer) {
                gl.bindBuffer(gl.ARRAY_BUFFER, this.stlInterleavedBuffer);
                gl.disableVertexAttribArray(aNorm);
                gl.vertexAttrib3f(aNorm, 0, 0, 1);
                gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.stlWireIndexBuffer);
                const maxTrianglesPerIndexChunk = 21_845;
                for (let triangleStart = 0; triangleStart < this.stlVertexCount / 3; triangleStart += maxTrianglesPerIndexChunk) {
                    const chunkTriangles = Math.min(maxTrianglesPerIndexChunk, this.stlVertexCount / 3 - triangleStart);
                    const chunkIndexStart = triangleStart * 6;
                    gl.vertexAttribPointer(aPos, 3, gl.FLOAT, false, 24, triangleStart * 3 * 24);
                    gl.drawElements(gl.LINES, chunkTriangles * 6, gl.UNSIGNED_SHORT, chunkIndexStart * 2);
                }
            } else if (this.wireframe && this.stlWireBuffer) {
                gl.bindBuffer(gl.ARRAY_BUFFER, this.stlWireBuffer);
                gl.vertexAttribPointer(aPos, 3, gl.FLOAT, false, 0, 0);
                gl.disableVertexAttribArray(aNorm);
                gl.vertexAttrib3f(aNorm, 0, 0, 1);
                gl.drawArrays(gl.LINES, 0, this.stlWireVertexCount);
            } else {
                gl.drawArrays(gl.TRIANGLES, 0, this.stlVertexCount);
            }
        } else if (this.mode === 'thermalRod' && this.thermalRodBuffers) {
            gl.useProgram(this.thermalRodProgram);
            const uMVP = gl.getUniformLocation(this.thermalRodProgram, 'uMVP');
            gl.uniformMatrix4fv(uMVP, false, mvp);
            const aPos = gl.getAttribLocation(this.thermalRodProgram, 'aPosition');
            const aNormal = gl.getAttribLocation(this.thermalRodProgram, 'aNormal');
            const aColor = gl.getAttribLocation(this.thermalRodProgram, 'aColor');
            gl.bindBuffer(gl.ARRAY_BUFFER, this.thermalRodBuffers.position);
            gl.enableVertexAttribArray(aPos);
            gl.vertexAttribPointer(aPos, 3, gl.FLOAT, false, 0, 0);
            gl.bindBuffer(gl.ARRAY_BUFFER, this.thermalRodBuffers.normal);
            gl.enableVertexAttribArray(aNormal);
            gl.vertexAttribPointer(aNormal, 3, gl.FLOAT, false, 0, 0);
            gl.bindBuffer(gl.ARRAY_BUFFER, this.thermalRodBuffers.color);
            gl.enableVertexAttribArray(aColor);
            gl.vertexAttribPointer(aColor, 3, gl.FLOAT, false, 0, 0);
            gl.drawArrays(gl.TRIANGLES, 0, this.thermalRodBuffers.vertexCount);

            // Thin circumferential lines mark each finite-element boundary.
            gl.useProgram(this.stlProgram);
            gl.uniformMatrix4fv(gl.getUniformLocation(this.stlProgram, 'uMVP'), false, mvp);
            gl.uniformMatrix4fv(gl.getUniformLocation(this.stlProgram, 'uModel'), false, this.identityMatrix());
            gl.uniform3f(gl.getUniformLocation(this.stlProgram, 'uColor'), 0.16, 0.13, 0.10);
            gl.uniform3f(gl.getUniformLocation(this.stlProgram, 'uLightDir'), 0.0, 0.7, 0.7);
            const linePos = gl.getAttribLocation(this.stlProgram, 'aPosition');
            const lineNormal = gl.getAttribLocation(this.stlProgram, 'aNormal');
            gl.bindBuffer(gl.ARRAY_BUFFER, this.thermalRodBuffers.meshLinePosition);
            gl.enableVertexAttribArray(linePos);
            gl.vertexAttribPointer(linePos, 3, gl.FLOAT, false, 0, 0);
            gl.bindBuffer(gl.ARRAY_BUFFER, this.thermalRodBuffers.meshLineNormal);
            gl.enableVertexAttribArray(lineNormal);
            gl.vertexAttribPointer(lineNormal, 3, gl.FLOAT, false, 0, 0);
            gl.drawArrays(gl.LINES, 0, this.thermalRodBuffers.meshLineVertexCount);
        } else if (this.mode === 'points' && this.pointsPosBuffer) {
            if (this.usePointOverlay) {
                this.renderPointCloudOverlay(mvp);
            } else {
                gl.useProgram(this.pointsProgram);
                const uMVP = gl.getUniformLocation(this.pointsProgram, 'uMVP');
                const uPointSize = gl.getUniformLocation(this.pointsProgram, 'uPointSize');
                gl.uniformMatrix4fv(uMVP, false, mvp);
                gl.uniform1f(uPointSize, 7.0 * (window.devicePixelRatio || 1));

                const aPos = gl.getAttribLocation(this.pointsProgram, 'aPosition');
                gl.bindBuffer(gl.ARRAY_BUFFER, this.pointsPosBuffer);
                gl.enableVertexAttribArray(aPos);
                gl.vertexAttribPointer(aPos, 3, gl.FLOAT, false, 0, 0);

                const aCol = gl.getAttribLocation(this.pointsProgram, 'aColor');
                gl.bindBuffer(gl.ARRAY_BUFFER, this.pointsColBuffer);
                gl.enableVertexAttribArray(aCol);
                gl.vertexAttribPointer(aCol, 3, gl.FLOAT, false, 0, 0);

                gl.drawArrays(gl.POINTS, 0, this.pointsCount);
                if (!this.pointVisibilityChecked && this.pointOverlayContext) {
                    this.pointVisibilityChecked = true;
                    if (!this.pointCloudIsVisible(mvp)) {
                        this.usePointOverlay = true;
                        this.renderPointCloudOverlay(mvp);
                    }
                }
            }
        }

        if (this.mode === 'points') this.showNodeTooltip(this.hoveredNode || this.selectedNode);
    }

    // --- Matrix Math Helpers ---

    identityMatrix() {
        return new Float32Array([
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        ]);
    }

    perspectiveMatrix(fov, aspect, near, far) {
        const f = 1.0 / Math.tan(fov / 2);
        const rangeInv = 1.0 / (near - far);
        return new Float32Array([
            f / aspect, 0, 0, 0,
            0, f, 0, 0,
            0, 0, (near + far) * rangeInv, -1,
            0, 0, near * far * rangeInv * 2, 0
        ]);
    }

    lookAtMatrix(eye, target, up) {
        let z0 = eye[0] - target[0], z1 = eye[1] - target[1], z2 = eye[2] - target[2];
        let len = Math.hypot(z0, z1, z2);
        z0 /= len; z1 /= len; z2 /= len;

        let x0 = up[1] * z2 - up[2] * z1, x1 = up[2] * z0 - up[0] * z2, x2 = up[0] * z1 - up[1] * z0;
        len = Math.hypot(x0, x1, x2);
        x0 /= len; x1 /= len; x2 /= len;

        let y0 = z1 * x2 - z2 * x1, y1 = z2 * x0 - z0 * x2, y2 = z0 * x1 - z1 * x0;

        return new Float32Array([
            x0, y0, z0, 0,
            x1, y1, z1, 0,
            x2, y2, z2, 0,
            -(x0*eye[0] + x1*eye[1] + x2*eye[2]),
            -(y0*eye[0] + y1*eye[1] + y2*eye[2]),
            -(z0*eye[0] + z1*eye[1] + z2*eye[2]),
            1
        ]);
    }

    multiplyMatrices(a, b) {
        const out = new Float32Array(16);
        for (let i = 0; i < 4; ++i) {
            for (let j = 0; j < 4; ++j) {
                out[j * 4 + i] =
                    a[i] * b[j * 4] +
                    a[i + 4] * b[j * 4 + 1] +
                    a[i + 8] * b[j * 4 + 2] +
                    a[i + 12] * b[j * 4 + 3];
            }
        }
        return out;
    }
}

