import { GameObject, Transform2D, Mesh2D, Shape2D, Camera2D, ParticleEmitter2D } from "bokken/gameObject";
import Canvas, { View, Label } from "bokken/canvas";
import Input from "bokken/input";
import Log from "bokken/log";
import { type Scene, GameState } from "../../core/GameState.ts";
import { Game } from "../../core/Game.ts";
import { hsvToHex, clamp, randomRange } from "../PhysicsUtils.ts";

const ParticleEase = {
    Linear: 0, EaseInQuad: 1, EaseOutQuad: 2,
    EaseInOutQuad: 3, EaseOutCubic: 4, EaseInCubic: 5,
} as const;

interface CelestialBody {
    name: string;
    x: number;
    y: number;
    vx: number;
    vy: number;
    mass: number;
    radius: number;
    color: number;
    hue: number;
    isStar: boolean;
    trailNames: string[];
    trailIdx: number;
    trailTimer: number;
    glowName: string;
}

const G = 80; // Gravitational constant
const MAX_BODIES = 30;
const TRAIL_LEN = 40;

export class OrbitScene implements Scene {
    private readonly game: Game;
    private camera!: GameObject;
    private bodies: CelestialBody[] = [];
    private bodyIdx = 0;
    private elapsed = 0;
    private objNames: string[] = [];

    constructor(game: Game) {
        this.game = game;
    }

    enter(): void {
        this.bodies = [];
        this.objNames = [];
        this.bodyIdx = 0;
        this.elapsed = 0;

        this.camera = new GameObject("OrbitCam")
            .addComponent(Transform2D, { positionX: 0, positionY: 0 })
            .addComponent(Camera2D, { zoom: 16, isActive: true });
        this.objNames.push("OrbitCam");

        // Central star
        this.spawnBody(0, 0, 0, 0, 200, 1.5, 40, true);

        // Planets in orbits
        const orbits = [
            { dist: 5, speed: 4.0, mass: 2, radius: 0.35, hue: 200 },
            { dist: 8, speed: 3.2, mass: 3, radius: 0.4, hue: 120 },
            { dist: 12, speed: 2.5, mass: 1.5, radius: 0.3, hue: 280 },
            { dist: 16, speed: 2.0, mass: 5, radius: 0.5, hue: 20 },
            { dist: 20, speed: 1.6, mass: 0.8, radius: 0.25, hue: 320 },
        ];

        for (const orbit of orbits) {
            const angle = Math.random() * Math.PI * 2;
            const x = Math.cos(angle) * orbit.dist;
            const y = Math.sin(angle) * orbit.dist;
            // Tangential velocity for circular orbit
            const vx = -Math.sin(angle) * orbit.speed;
            const vy = Math.cos(angle) * orbit.speed;
            this.spawnBody(x, y, vx, vy, orbit.mass, orbit.radius, orbit.hue, false);
        }

        // Some moons
        for (let i = 0; i < 3; i++) {
            const parentIdx = 1 + i; // Orbit around first few planets
            if (parentIdx >= this.bodies.length) continue;
            const parent = this.bodies[parentIdx];
            const moonDist = 1.5;
            const moonAngle = Math.random() * Math.PI * 2;
            const mx = parent.x + Math.cos(moonAngle) * moonDist;
            const my = parent.y + Math.sin(moonAngle) * moonDist;
            const moonSpeed = 3;
            const mvx = parent.vx - Math.sin(moonAngle) * moonSpeed;
            const mvy = parent.vy + Math.cos(moonAngle) * moonSpeed;
            this.spawnBody(mx, my, mvx, mvy, 0.2, 0.12, (i * 90 + 60) % 360, false);
        }

        this.renderHUD();
        Log.info("Orbital Mechanics — Space to spawn asteroid, R to reset");
    }

    private spawnBody(
        x: number, y: number, vx: number, vy: number,
        mass: number, radius: number, hue: number, isStar: boolean,
    ): void {
        if (this.bodies.length >= MAX_BODIES) return;
        const name = `OBody_${this.bodyIdx}`;
        const glowName = `OGlow_${this.bodyIdx}`;
        this.bodyIdx++;

        const color = isStar
            ? hsvToHex(40, 0.9, 1.0, 1.0)
            : hsvToHex(hue, 0.75, 0.95, 0.9);

        // Glow (larger, translucent circle behind)
        if (isStar || radius > 0.3) {
            const glowColor = isStar
                ? hsvToHex(30, 0.8, 1.0, 0.15)
                : hsvToHex(hue, 0.5, 0.8, 0.1);
            new GameObject(glowName)
                .addComponent(Transform2D, {
                    positionX: x, positionY: y,
                    scaleX: radius * (isStar ? 6 : 4),
                    scaleY: radius * (isStar ? 6 : 4),
                    zOrder: isStar ? 1 : 3,
                })
                .addComponent(Mesh2D, { shape: Shape2D.Circle, color: glowColor });
            this.objNames.push(glowName);
        }

        new GameObject(name)
            .addComponent(Transform2D, {
                positionX: x, positionY: y,
                scaleX: radius * 2, scaleY: radius * 2,
                zOrder: isStar ? 2 : 5,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Circle, color });
        this.objNames.push(name);

        // Star emitter for particle glow
        if (isStar) {
            const emName = `OStarEm_${this.bodyIdx}`;
            new GameObject(emName)
                .addComponent(Transform2D, { positionX: x, positionY: y })
                .addComponent(ParticleEmitter2D, {
                    emitting: true,
                    emitRate: 20,
                    colorStart: 0xFFCC4488,
                    colorEnd: 0xFF440000,
                    speedMinimum: 0.3,
                    speedMaximum: 1.5,
                    direction: 90,
                    spreadAngle: 360,
                    sizeStart: 0.3,
                    sizeEnd: 0.01,
                    lifetimeMinimum: 0.5,
                    lifetimeMaximum: 1.5,
                    gravity: 0,
                    damping: 1.0,
                    alphaEase: ParticleEase.EaseOutCubic,
                    maximumParticles: 50,
                });
            this.objNames.push(emName);
        }

        this.bodies.push({
            name, x, y, vx, vy, mass, radius, color, hue,
            isStar, glowName,
            trailNames: [], trailIdx: 0, trailTimer: 0,
        });
    }

    private destroyAll(): void {
        for (const n of this.objNames) {
            const o = GameObject.find(n);
            if (o) GameObject.destroy(o);
        }
        for (const b of this.bodies) {
            for (const tn of b.trailNames) {
                const o = GameObject.find(tn);
                if (o) GameObject.destroy(o);
            }
        }
        this.objNames = [];
        this.bodies = [];
    }

    update(dt: number): void {
        this.elapsed += dt;

        if (Input.isKeyPressed("Escape")) {
            this.destroyAll();
            this.game.transition(GameState.MainMenu);
            return;
        }
        if (Input.isKeyPressed("KeyR")) {
            this.destroyAll();
            this.enter();
            return;
        }

        // Spawn random asteroid
        if (Input.isKeyPressed("Space")) {
            const angle = Math.random() * Math.PI * 2;
            const dist = randomRange(6, 18);
            const x = Math.cos(angle) * dist;
            const y = Math.sin(angle) * dist;
            const speed = randomRange(1.5, 4);
            const vx = -Math.sin(angle) * speed + randomRange(-1, 1);
            const vy = Math.cos(angle) * speed + randomRange(-1, 1);
            this.spawnBody(x, y, vx, vy, randomRange(0.3, 2), randomRange(0.1, 0.3), Math.random() * 360, false);
        }

        // Trails
        for (const b of this.bodies) {
            if (b.isStar) continue;
            b.trailTimer += dt;
            if (b.trailTimer >= 0.06) {
                b.trailTimer = 0;
                const tn = `OTr_${b.name}_${b.trailIdx++}`;
                const trailColor = hsvToHex(b.hue, 0.3, 0.5, 0.2);
                new GameObject(tn)
                    .addComponent(Transform2D, {
                        positionX: b.x, positionY: b.y,
                        scaleX: b.radius * 0.6,
                        scaleY: b.radius * 0.6,
                        zOrder: 1,
                    })
                    .addComponent(Mesh2D, { shape: Shape2D.Circle, color: trailColor });
                b.trailNames.push(tn);

                if (b.trailNames.length > TRAIL_LEN) {
                    const old = b.trailNames.shift()!;
                    const o = GameObject.find(old);
                    if (o) GameObject.destroy(o);
                }
            }

            // Fade trails
            for (let i = 0; i < b.trailNames.length; i++) {
                const tObj = GameObject.find(b.trailNames[i]);
                if (!tObj) continue;
                const tt = tObj.getComponent(Transform2D);
                if (!tt) continue;
                const progress = i / b.trailNames.length;
                tt.scaleX = b.radius * 0.6 * progress;
                tt.scaleY = b.radius * 0.6 * progress;
            }
        }

        // Pulsating star glow
        const star = this.bodies.find(b => b.isStar);
        if (star) {
            const glow = GameObject.find(star.glowName);
            if (glow) {
                const gt = glow.getComponent(Transform2D)!;
                const pulse = 1 + Math.sin(this.elapsed * 2) * 0.15;
                gt.scaleX = star.radius * 6 * pulse;
                gt.scaleY = star.radius * 6 * pulse;
            }

            // Update star emitter position
            const emObj = GameObject.find(`OStarEm_${1}`);
            if (emObj) {
                const et = emObj.getComponent(Transform2D)!;
                et.positionX = star.x;
                et.positionY = star.y;
            }
        }
    }

    fixedUpdate(dt: number): void {
        const n = this.bodies.length;

        // N-body gravity
        for (let i = 0; i < n; i++) {
            const bi = this.bodies[i];
            let fx = 0, fy = 0;

            for (let j = 0; j < n; j++) {
                if (i === j) continue;
                const bj = this.bodies[j];
                const dx = bj.x - bi.x;
                const dy = bj.y - bi.y;
                const distSq = dx * dx + dy * dy;
                const dist = Math.sqrt(distSq) || 0.1;

                // Softened gravity to avoid singularities
                const softened = distSq + 0.5;
                const force = G * bi.mass * bj.mass / softened;
                fx += (dx / dist) * force / bi.mass;
                fy += (dy / dist) * force / bi.mass;

                // Collision merge
                const minDist = bi.radius + bj.radius;
                if (dist < minDist && !bi.isStar && bj.isStar) {
                    // Absorbed by star — respawn
                    const angle = Math.random() * Math.PI * 2;
                    const spawnDist = randomRange(8, 20);
                    bi.x = Math.cos(angle) * spawnDist;
                    bi.y = Math.sin(angle) * spawnDist;
                    const speed = randomRange(2, 4);
                    bi.vx = -Math.sin(angle) * speed;
                    bi.vy = Math.cos(angle) * speed;
                    fx = 0; fy = 0;
                    break;
                }
            }

            if (!bi.isStar) {
                bi.vx += fx * dt;
                bi.vy += fy * dt;
                bi.x += bi.vx * dt;
                bi.y += bi.vy * dt;
            }

            // Sync transform
            const obj = GameObject.find(bi.name);
            if (obj) {
                const t = obj.getComponent(Transform2D)!;
                t.positionX = bi.x;
                t.positionY = bi.y;
            }

            // Sync glow
            const glow = GameObject.find(bi.glowName);
            if (glow) {
                const gt = glow.getComponent(Transform2D)!;
                gt.positionX = bi.x;
                gt.positionY = bi.y;
            }
        }
    }

    private renderHUD(): void {
        Canvas.render(
            <View style={{
                position: "Absolute", top: 12, left: 12,
                backgroundColor: 0x00000088, padding: 8, borderRadius: 4,
            }}>
                <Label style={{ fontSize: 16, color: 0xFFCC44FF }}>
                    {"Orbital Mechanics (N-Body)"}
                </Label>
                <Label style={{ fontSize: 11, color: 0xAABBCC88, marginTop: 4 }}>
                    {"Space: spawn body | R: reset | Esc: menu"}
                </Label>
            </View>
        );
    }
}
