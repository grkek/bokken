// Shared physics math utilities used across demos.

export function lerp(a: number, b: number, t: number): number {
    return a + (b - a) * t;
}

export function clamp(v: number, min: number, max: number): number {
    return v < min ? min : v > max ? max : v;
}

export function dist(x1: number, y1: number, x2: number, y2: number): number {
    const dx = x2 - x1;
    const dy = y2 - y1;
    return Math.sqrt(dx * dx + dy * dy);
}

export function normalize(x: number, y: number): [number, number] {
    const len = Math.sqrt(x * x + y * y) || 1;
    return [x / len, y / len];
}

export function randomRange(min: number, max: number): number {
    return min + Math.random() * (max - min);
}

export function hsvToHex(h: number, s: number, v: number, a: number = 1.0): number {
    h = ((h % 360) + 360) % 360;
    const c = v * s;
    const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
    const m = v - c;
    let r = 0, g = 0, b = 0;
    if (h < 60) { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else { r = c; b = x; }
    const ri = Math.floor((r + m) * 255);
    const gi = Math.floor((g + m) * 255);
    const bi = Math.floor((b + m) * 255);
    const ai = Math.floor(a * 255);
    return ((ri << 24) | (gi << 16) | (bi << 8) | ai) >>> 0;
}
