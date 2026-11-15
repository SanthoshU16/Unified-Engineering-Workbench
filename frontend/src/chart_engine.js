// Lightweight, zero-dependency interactive 2D Canvas Charting Engine
// Supports multi-series line charts, log-log scaling, and bar charts with hover tooltips

export class CanvasChart {
    constructor(canvasId, options = {}) {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) throw new Error(`Canvas #${canvasId} not found`);
        this.ctx = this.canvas.getContext('2d');
        this.options = Object.assign({
            title: '',
            xLabel: 'X',
            yLabel: 'Y',
            logX: false,
            logY: false,
            showLegend: true,
            padding: { top: 35, right: 30, bottom: 45, left: 65 }
        }, options);

        this.series = [];
        this.hoverPoint = null;
        this.setupEventHandlers();
        if (typeof ResizeObserver !== 'undefined') {
            this.resizeObserver = new ResizeObserver(() => this.resize());
            this.resizeObserver.observe(this.canvas.parentElement);
        }
        this.resize();
    }

    resize() {
        const rect = this.canvas.parentElement.getBoundingClientRect();
        const dpr = window.devicePixelRatio || 1;
        if (rect.width < 1 || rect.height < 1) return;

        this.width = rect.width;
        this.height = rect.height;
        this.canvas.width = Math.round(this.width * dpr);
        this.canvas.height = Math.round(this.height * dpr);
        // Keep the bitmap responsive to its parent instead of freezing the
        // first measured pixel size into inline styles.
        this.canvas.style.width = '100%';
        this.canvas.style.height = '100%';
        this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        this.render();
    }

    setSeries(seriesArray) {
        this.series = seriesArray;
        this.render();
    }

    setupEventHandlers() {
        this.canvas.addEventListener('mousemove', (e) => {
            const rect = this.canvas.getBoundingClientRect();
            const mouseX = e.clientX - rect.left;
            const mouseY = e.clientY - rect.top;
            this.handleMouseMove(mouseX, mouseY);
        });
        this.canvas.addEventListener('mouseleave', () => {
            this.hoverPoint = null;
            this.render();
        });
    }

    handleMouseMove(mx, my) {
        const pad = this.currentPadding || this.options.padding;
        const plotW = this.width - pad.left - pad.right;
        const plotH = this.height - pad.top - pad.bottom;
        if (mx < pad.left || mx > pad.left + plotW || my < pad.top || my > pad.top + plotH) {
            if (this.hoverPoint) {
                this.hoverPoint = null;
                this.render();
            }
            return;
        }

        let closest = null;
        let minDist = 15;

        for (const s of this.series) {
            for (const pt of s.data) {
                const px = this.xToScreen(pt.x);
                const py = this.yToScreen(pt.y);
                const dist = Math.hypot(mx - px, my - py);
                if (dist < minDist) {
                    minDist = dist;
                    closest = { ...pt, x: pt.x, y: pt.y, px, py, label: s.label || '', color: s.color || '#a78bfa' };
                }
            }
        }

        this.hoverPoint = closest;
        this.render();
    }

    computeBounds() {
        let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
        for (const s of this.series) {
            for (const pt of s.data) {
                if (pt.x < minX) minX = pt.x;
                if (pt.x > maxX) maxX = pt.x;
                if (pt.y < minY) minY = pt.y;
                if (pt.y > maxY) maxY = pt.y;
            }
        }
        if (minX === Infinity) { minX = 0; maxX = 1; minY = 0; maxY = 1; }
        if (minX === maxX) { minX -= 1; maxX += 1; }
        if (minY === maxY) { minY -= 1; maxY += 1; }

        if (!this.options.logY && minY > 0 && minY < maxY * 0.2) minY = 0;
        this.bounds = { minX, maxX, minY, maxY };
    }

    xToScreen(x) {
        const pad = this.currentPadding || this.options.padding;
        const w = this.width - pad.left - pad.right;
        if (this.options.logX) {
            const lx = Math.log10(Math.max(1e-12, x));
            const lmin = Math.log10(Math.max(1e-12, this.bounds.minX));
            const lmax = Math.log10(Math.max(1e-12, this.bounds.maxX));
            return pad.left + ((lx - lmin) / (lmax - lmin)) * w;
        }
        return pad.left + ((x - this.bounds.minX) / (this.bounds.maxX - this.bounds.minX)) * w;
    }

    yToScreen(y) {
        const pad = this.currentPadding || this.options.padding;
        const h = this.height - pad.top - pad.bottom;
        if (this.options.logY) {
            const ly = Math.log10(Math.max(1e-12, y));
            const lmin = Math.log10(Math.max(1e-12, this.bounds.minY));
            const lmax = Math.log10(Math.max(1e-12, this.bounds.maxY));
            return pad.top + h - ((ly - lmin) / (lmax - lmin)) * h;
        }
        return pad.top + h - ((y - this.bounds.minY) / (this.bounds.maxY - this.bounds.minY)) * h;
    }

    render() {
        if (!this.width || !this.height) return;
        const ctx = this.ctx;
        const pad = this.updatePadding();
        ctx.clearRect(0, 0, this.width, this.height);

        this.computeBounds();
        const plotW = this.width - pad.left - pad.right;
        const plotH = this.height - pad.top - pad.bottom;

        // Background
        ctx.fillStyle = '#171622';
        ctx.fillRect(0, 0, this.width, this.height);
        ctx.fillStyle = '#111018';
        ctx.fillRect(pad.left, pad.top, plotW, plotH);

        // Title
        if (this.options.title) {
            ctx.fillStyle = '#f3f0f8';
            ctx.font = 'bold 13px -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, sans-serif';
            ctx.textAlign = 'left';
            ctx.fillText(this.options.title, pad.left, 22, Math.max(1, this.width - pad.left - pad.right));
        }

        // Gridlines & Ticks
        this.renderAxesAndGrid(ctx, pad, plotW, plotH);

        // Render Series
        for (const s of this.series) {
            if (!s.data || s.data.length === 0) continue;
            ctx.save();
            ctx.strokeStyle = s.color || '#a78bfa';
            ctx.lineWidth = s.lineWidth || 2;
            if (s.dash) ctx.setLineDash(s.dash);

            ctx.beginPath();
            let first = true;
            for (const pt of s.data) {
                const px = this.xToScreen(pt.x);
                const py = this.yToScreen(pt.y);
                if (first) {
                    ctx.moveTo(px, py);
                    first = false;
                } else {
                    ctx.lineTo(px, py);
                }
            }
            ctx.stroke();
            ctx.restore();

            // Points
            if (s.showPoints !== false) {
                ctx.fillStyle = s.color || '#a78bfa';
                for (let i = 0; i < s.data.length; ++i) {
                    const pt = s.data[i];
                    const px = this.xToScreen(pt.x);
                    const py = this.yToScreen(pt.y);
                    ctx.beginPath();
                    ctx.arc(px, py, s.pointRadius || 3.5, 0, Math.PI * 2);
                    ctx.fill();

                    if (s.pointLabels?.[i]) {
                        const label = s.pointLabels[i];
                        ctx.font = '600 9px -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, sans-serif';
                        const labelWidth = ctx.measureText(label).width;
                        ctx.fillStyle = 'rgba(18, 16, 26, 0.96)';
                        ctx.fillRect(px - labelWidth / 2 - 5, py - 21, labelWidth + 10, 14);
                        ctx.fillStyle = '#e6e0f0';
                        ctx.textAlign = 'center';
                        ctx.fillText(label, px, py - 11);
                        ctx.fillStyle = s.color || '#a78bfa';
                    }
                }
            }

            if (s.segmentLabels?.length) {
                ctx.save();
                ctx.font = '600 9px -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, sans-serif';
                ctx.textAlign = 'center';
                for (const annotation of s.segmentLabels) {
                    const from = s.data[annotation.fromIndex];
                    const to = s.data[annotation.toIndex];
                    if (!from || !to) continue;
                    const x = (this.xToScreen(from.x) + this.xToScreen(to.x)) / 2;
                    const y = (this.yToScreen(from.y) + this.yToScreen(to.y)) / 2 - 15;
                    const width = ctx.measureText(annotation.text).width + 14;
                    ctx.fillStyle = 'rgba(25, 22, 36, 0.97)';
                    ctx.strokeStyle = 'rgba(167, 139, 250, 0.4)';
                    ctx.lineWidth = 1;
                    ctx.fillRect(x - width / 2, y - 10, width, 19);
                    ctx.strokeRect(x - width / 2, y - 10, width, 19);
                    ctx.fillStyle = '#d1c2fa';
                    ctx.fillText(annotation.text, x, y + 3);
                }
                ctx.restore();
            }
        }

        // Legend
        if (this.options.showLegend && this.series.length > 0) {
            this.renderLegend(ctx, pad, plotW);
        }

        // Tooltip
        if (this.hoverPoint) {
            this.renderTooltip(ctx, this.hoverPoint);
        }
    }

    updatePadding() {
        const padding = { ...this.options.padding };
        const ctx = this.ctx;
        const availableWidth = Math.max(1, this.width - padding.left - padding.right);
        ctx.font = '11px -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, sans-serif';
        const legendWidths = this.series.map(series => Math.min(availableWidth, ctx.measureText(series.label || '').width + 18));
        const legendWidth = legendWidths.reduce((total, width) => total + width + 8, 0);
        ctx.font = 'bold 13px -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, sans-serif';
        const titleWidth = this.options.title ? ctx.measureText(this.options.title).width : 0;
        this.compactLegend = this.options.showLegend && this.series.length > 0 &&
            (this.width < 520 || titleWidth + legendWidth + 24 > availableWidth);

        if (this.compactLegend) {
            let rows = 1;
            let rowWidth = 0;
            for (const itemWidth of [...legendWidths].reverse()) {
                if (rowWidth > 0 && rowWidth + itemWidth > availableWidth) {
                    rows += 1;
                    rowWidth = 0;
                }
                rowWidth += itemWidth + 8;
            }
            padding.top = Math.max(padding.top, 50 + (rows - 1) * 14);
        }

        this.currentPadding = padding;
        return padding;
    }

    renderAxesAndGrid(ctx, pad, plotW, plotH) {
        ctx.strokeStyle = '#353244';
        ctx.lineWidth = 1;
        ctx.fillStyle = '#a8a2b7';
        ctx.font = '10px -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, sans-serif';

        // Y-axis grid & labels (5 divisions)
        const yTicks = 5;
        for (let i = 0; i <= yTicks; ++i) {
            const frac = i / yTicks;
            const py = pad.top + plotH - frac * plotH;
            ctx.beginPath();
            ctx.moveTo(pad.left, py);
            ctx.lineTo(pad.left + plotW, py);
            ctx.stroke();

            let val;
            if (this.options.logY) {
                const lmin = Math.log10(Math.max(1e-12, this.bounds.minY));
                const lmax = Math.log10(Math.max(1e-12, this.bounds.maxY));
                val = Math.pow(10, lmin + frac * (lmax - lmin));
            } else {
                val = this.bounds.minY + frac * (this.bounds.maxY - this.bounds.minY);
            }

            ctx.textAlign = 'right';
            ctx.fillText(this.formatNumber(val), pad.left - 8, py + 3);
        }

        // X-axis grid & labels (5 divisions)
        const xTicks = 5;
        for (let i = 0; i <= xTicks; ++i) {
            const frac = i / xTicks;
            const px = pad.left + frac * plotW;
            ctx.beginPath();
            ctx.moveTo(px, pad.top);
            ctx.lineTo(px, pad.top + plotH);
            ctx.stroke();

            let val;
            if (this.options.logX) {
                const lmin = Math.log10(Math.max(1e-12, this.bounds.minX));
                const lmax = Math.log10(Math.max(1e-12, this.bounds.maxX));
                val = Math.pow(10, lmin + frac * (lmax - lmin));
            } else {
                val = this.bounds.minX + frac * (this.bounds.maxX - this.bounds.minX);
            }

            ctx.textAlign = 'center';
            ctx.fillText(this.formatNumber(val), px, pad.top + plotH + 16);
        }

        // Axis Titles
        ctx.fillStyle = '#d2ccdf';
        ctx.font = '11px -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, sans-serif';
        // X Label
        ctx.textAlign = 'center';
        ctx.fillText(this.options.xLabel, pad.left + plotW / 2, pad.top + plotH + 34);

        // Y Label
        ctx.save();
        ctx.translate(16, pad.top + plotH / 2);
        ctx.rotate(-Math.PI / 2);
        ctx.textAlign = 'center';
        ctx.fillText(this.options.yLabel, 0, 0);
        ctx.restore();

        // Plot Border
        ctx.strokeStyle = '#554f69';
        ctx.strokeRect(pad.left, pad.top, plotW, plotH);
    }

    renderLegend(ctx, pad, plotW) {
        if (this.compactLegend) {
            const right = pad.left + plotW;
            const availableWidth = Math.max(1, plotW);
            let lx = pad.left;
            let ly = 40;
            ctx.font = '10px -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, sans-serif';
            ctx.textAlign = 'left';

            for (let i = this.series.length - 1; i >= 0; --i) {
                const series = this.series[i];
                const measuredWidth = ctx.measureText(series.label || '').width;
                const itemWidth = Math.min(availableWidth, measuredWidth + 18);
                if (lx > pad.left && lx + itemWidth > right) {
                    lx = pad.left;
                    ly += 14;
                }

                ctx.fillStyle = series.color || '#a78bfa';
                ctx.fillRect(lx, ly - 7, 9, 7);
                ctx.fillStyle = '#d2ccdf';
                ctx.fillText(series.label || '', lx + 13, ly, Math.max(1, Math.min(availableWidth - 13, right - lx - 13)));
                lx += itemWidth + 8;
            }
            return;
        }

        let lx = pad.left + plotW - 10;
        const ly = 20;
        ctx.font = '11px -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, sans-serif';
        ctx.textAlign = 'right';

        for (let i = this.series.length - 1; i >= 0; --i) {
            const s = this.series[i];
            const textWidth = ctx.measureText(s.label).width;
            ctx.fillStyle = '#d2ccdf';
            ctx.fillText(s.label, lx, ly);

            ctx.fillStyle = s.color || '#a78bfa';
            ctx.fillRect(lx - textWidth - 14, ly - 8, 10, 8);
            lx -= (textWidth + 30);
        }
    }

    renderTooltip(ctx, p) {
        ctx.save();
        // Highlight circle
        ctx.strokeStyle = '#ffffff';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.arc(p.px, p.py, 6, 0, Math.PI * 2);
        ctx.stroke();

        ctx.font = '11px -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, sans-serif';
        const lines = Number.isInteger(p.N)
            ? [`Mesh N=${p.N} · h=${this.formatNumber(p.x)} m`, `Midpoint error=${this.formatNumber(p.y)} °C`, `Observed order p=${p.order > 0 ? p.order.toFixed(3) : 'Base mesh'}`]
            : [`${p.label ? p.label + ': ' : ''}x=${this.formatNumber(p.x)}, y=${this.formatNumber(p.y)}`];
        const tw = Math.max(...lines.map(line => ctx.measureText(line).width));

        let tx = p.px + 10;
        const boxHeight = lines.length * 15 + 10;
        let ty = p.py - boxHeight - 7;
        if (tx + tw + 16 > this.width) tx = p.px - tw - 20;
        if (ty < 10) ty = p.py + 15;

        ctx.fillStyle = 'rgba(27, 26, 38, 0.97)';
        ctx.strokeStyle = '#615a75';
        ctx.lineWidth = 1;
        ctx.fillRect(tx, ty, tw + 16, boxHeight);
        ctx.strokeRect(tx, ty, tw + 16, boxHeight);

        ctx.fillStyle = '#f3f0f8';
        ctx.textAlign = 'left';
        lines.forEach((line, index) => ctx.fillText(line, tx + 8, ty + 15 + index * 15));
        ctx.restore();
    }

    formatNumber(n) {
        if (Math.abs(n) < 1e-4 && n !== 0) return n.toExponential(2);
        if (Math.abs(n) >= 1e4) return n.toExponential(2);
        return parseFloat(n.toPrecision(4)).toString();
    }
}

