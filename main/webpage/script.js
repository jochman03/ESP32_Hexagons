const canvas = document.getElementById("hexes");
const ctx = canvas.getContext("2d");
const colorPicker = document.getElementById("hexColor");

const modeSelector = document.getElementById("modeSelect");
const effectSpeedRow = document.getElementById("effect_speed");

let isDragging = false;
let lastHex = -1;

let g_radius = 40.0;
let start_point = point(300, 350);

let tab_color = [];
let tab_points = [start_point];
let tab_hexes = [];

function select_mode() {
    sendMode();
}

function fill_tab_points() {
    const tab_movement = [4, 5, 4, 5, 0, 1, 0, 1, 1, 0, 1];

    for (let i = 0; i < tab_movement.length; i++) {
        tab_points.push(point_transform(tab_points[i], tab_movement[i]));
    }

    const point_4 = point_transform(tab_points[3], 3);
    tab_points.splice(4, 0, point_4);

    const point_11 = point_transform(tab_points[10], 2);
    tab_points.splice(11, 0, point_11);

    const point_13 = point_transform(tab_points[12], 5);
    tab_points.splice(13, 0, point_13);
}

function point_transform(p, dir) {
    switch (dir) {
        case 0:
            return point(p.x + 3.0 / 2.0 * g_radius, p.y - g_radius * Math.sqrt(3) / 2.0);

        case 1:
            return point(p.x + 3.0 / 2.0 * g_radius, p.y + g_radius * Math.sqrt(3) / 2.0);

        case 2:
            return point(p.x, p.y + g_radius * Math.sqrt(3));

        case 3:
            return point(p.x - 3.0 / 2.0 * g_radius, p.y + g_radius * Math.sqrt(3) / 2.0);

        case 4:
            return point(p.x - 3.0 / 2.0 * g_radius, p.y - g_radius * Math.sqrt(3) / 2.0);

        case 5:
            return point(p.x, p.y - g_radius * Math.sqrt(3));

        default:
            return point(0, 0);
    }
}

function getHexLayoutBounds() {
    let minX = Infinity;
    let minY = Infinity;
    let maxX = -Infinity;
    let maxY = -Infinity;

    for (const p of tab_points) {
        minX = Math.min(minX, p.x - g_radius);
        maxX = Math.max(maxX, p.x + g_radius);

        minY = Math.min(minY, p.y - g_radius);
        maxY = Math.max(maxY, p.y + g_radius);
    }

    return {
        minX,
        minY,
        maxX,
        maxY,
        width: maxX - minX,
        height: maxY - minY
    };
}

function resize() {
    const parent = canvas.parentElement;
    const rect = parent.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;

    const w = Math.max(1, Math.floor(rect.width));
    const h = Math.max(1, Math.floor(rect.height));

    canvas.width = Math.floor(w * dpr);
    canvas.height = Math.floor(h * dpr);

    canvas.style.width = w + "px";
    canvas.style.height = h + "px";

    drawHexes();
}

function drawHexes() {
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    if (tab_points.length === 0 || tab_color.length === 0) {
        return;
    }

    const bounds = getHexLayoutBounds();

    if (bounds.width <= 0 || bounds.height <= 0) {
        return;
    }

    const dpr = window.devicePixelRatio || 1;
    const padding = 32 * dpr;

    const availableWidth = Math.max(1, canvas.width - padding * 2);
    const availableHeight = Math.max(1, canvas.height - padding * 2);

    const scale = Math.min(
        availableWidth / bounds.width,
        availableHeight / bounds.height
    );

    const offsetX = (canvas.width - bounds.width * scale) / 2 - bounds.minX * scale;
    const offsetY = (canvas.height - bounds.height * scale) / 2 - bounds.minY * scale;

    tab_hexes.length = 0;

    for (let i = 0; i < tab_points.length; i++) {
        const p = tab_points[i];

        const px = offsetX + p.x * scale;
        const py = offsetY + p.y * scale;
        const rr = g_radius * scale;

        const hex = createHex(point(px, py), rr);

        ctx.lineWidth = Math.max(1, 2 * dpr);
        ctx.fillStyle = tab_color[i % tab_color.length];
        ctx.strokeStyle = "#000000";

        ctx.fill(hex);
        ctx.stroke(hex);

        tab_hexes.push(hex);
    }
}

function deg_to_rad(deg) {
    return deg / 180 * Math.PI;
}

function point(x, y) {
    return { x, y };
}

function vector(p1, p2) {
    return { p1, p2 };
}

function rotate_vector(vec, ang) {
    const p1 = vec.p1;
    const p2 = vec.p2;

    const p = point(p2.x - p1.x, p2.y - p1.y);

    const rad = deg_to_rad(ang);
    const c = Math.cos(rad);
    const s = Math.sin(rad);

    const new_x = p.x * c - p.y * s;
    const new_y = p.x * s + p.y * c;

    const rotated_p2 = point(p1.x + new_x, p1.y + new_y);

    return vector(p1, rotated_p2);
}

function createHex(p, radius) {
    const path = new Path2D();

    path.moveTo(p.x + radius, p.y);

    for (let i = 1; i < 6; i++) {
        const angle = i * 60 * Math.PI / 180;
        const x = p.x + radius * Math.cos(angle);
        const y = p.y + radius * Math.sin(angle);

        path.lineTo(x, y);
    }

    path.closePath();

    return path;
}

async function loadColors() {
    const r = await fetch("/getColors.json");
    const j = await r.json();

    const colors = j.colors;

    tab_color = colors.map(([r, g, b]) => `rgb(${r},${g},${b})`);

    tab_points = [start_point];
    fill_tab_points();

    resize();
}

function getHexIndexAt(e) {
    const rect = canvas.getBoundingClientRect();

    const x = (e.clientX - rect.left) * (canvas.width / rect.width);
    const y = (e.clientY - rect.top) * (canvas.height / rect.height);

    for (let i = tab_hexes.length - 1; i >= 0; i--) {
        if (ctx.isPointInPath(tab_hexes[i], x, y)) {
            return i;
        }
    }

    return -1;
}

canvas.addEventListener("pointerdown", (e) => {
    isDragging = true;
    lastHex = -1;

    canvas.setPointerCapture?.(e.pointerId);

    const i = getHexIndexAt(e);

    if (i !== -1) {
        lastHex = i;
        triggerHex(i);
    }
});

canvas.addEventListener("pointermove", (e) => {
    if (!isDragging) {
        return;
    }

    const i = getHexIndexAt(e);

    if (i !== -1 && i !== lastHex) {
        lastHex = i;
        triggerHex(i);
    }
});

canvas.addEventListener("pointerup", (e) => {
    if (isDragging) {
        sendColor();
    }

    canvas.releasePointerCapture?.(e.pointerId);

    isDragging = false;
    lastHex = -1;
});

canvas.addEventListener("pointercancel", () => {
    isDragging = false;
    lastHex = -1;
});

canvas.addEventListener("pointerleave", () => {
    isDragging = false;
    lastHex = -1;
});

function hexToRgb(hex) {
    return {
        r: parseInt(hex.slice(1, 3), 16),
        g: parseInt(hex.slice(3, 5), 16),
        b: parseInt(hex.slice(5, 7), 16)
    };
}

function triggerHex(i) {
    const { r, g, b } = hexToRgb(colorPicker.value);

    tab_color[i] = `rgb(${r},${g},${b})`;

    drawHexes();
}

function rgbconvert(s) {
    const m = s.match(/rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)/i);

    if (!m) {
        return [0, 0, 0];
    }

    return [Number(m[1]), Number(m[2]), Number(m[3])];
}

async function sendColor() {
    const colors = tab_color.map(rgbconvert);

    const res = await fetch("/setColors", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ colors })
    });

    if (!res.ok) {
        console.error("ESP error:", await res.text());
    } else {
        console.log("OK");
    }
}

async function sendSpeed() {
    const value = document.getElementById("rangeSpeed").value;

    const res = await fetch("/setSpeed", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ value })
    });

    if (!res.ok) {
        console.error("ESP error:", await res.text());
    } else {
        console.log("OK");
    }
}

async function sendMode() {
    const value = modeSelector.value;

    const res = await fetch("/setMode", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ value })
    });

    if (!res.ok) {
        console.error("ESP error:", await res.text());
    } else {
        console.log("OK");
    }
}

async function loadStatus() {
    const res = await fetch("/getStatus.json");
    const data = await res.json();

    const hexEnabled = document.getElementById("hexEnabled");

    modeSelector.value = data.mode;
    document.getElementById("rangeSpeed").value = data.speed;

    if (data.enabled == "1" || data.enabled == 1 || data.enabled === true) {
        hexEnabled.checked = true;
    } else {
        hexEnabled.checked = false;
    }

    select_mode();
}

async function setHexEnabled() {
    const hexEnabled = document.getElementById("hexEnabled");

    const payload = {
        enabled: hexEnabled.checked ? 1 : 0
    };

    const res = await fetch("/setHexEnabled", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify(payload)
    });

    if (!res.ok) {
        console.error("ESP error:", await res.text());
    }
}

window.addEventListener("resize", resize);

window.addEventListener("orientationchange", () => {
    setTimeout(resize, 200);
});

loadColors();
loadStatus();