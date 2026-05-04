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

interface FluidParticle {
    name: string;
    x: number;
    y: number;
    vx: number;
    vy: number;
    density: number;
    pressure: number;
    hue: number;
}

const GRAVITY = -12;
const REST_DENSITY = 3.0;
const GAS_CONSTANT = 30;
const VISCOSITY = 5.0;
const SMOOTHING_RADIUS = 1.2;
const SMOOTHING_RADIUS_SQ = SMOOTHING_RADIUS * SMOOTHING_RADIUS;
const PARTICLE_RADIUS = 0.18;
const FLOOR_Y = -9;
const WALL_LEFT = -10;
const WALL_RIGHT = 10;
const CEILING_Y = 10;
const MAX_PARTICLES = 120;

export class FluidScene implements Scene {
    private readonly game: Game;
    private camera!: GameObject;
    private particles: FluidParticle[] = [];
    private pIdx = 0;
    private elapsed = 0;
    private objNames: string[] = [];
    private emitterActive = false;
    private emitterTimer = 0;

    constructor(game: Game) {
        this.game = game;
    }

    enter(): void {
        this.particles = [];
        this.objNames = [];
        this.pIdx = 0;
        this.elapsed = 0;
        this.emitterActive = true;
        this.emitterTimer = 0;

        this.camera = new GameObject("FluidCam")
            .addComponent(Transform2D, { positionX: 0, positionY: 0 })
            .addComponent(Camera2D, { zoom: 24, isActive: true });
        this.objNames.push("FluidCam");

        // Walls
        for (const [n, px, py, sx, sy] of [
            ["FlWallL", WALL_LEFT - 0.2, 0, 0.4, 22],
            ["FlWallR", WALL_RIGHT + 0.2, 0, 0.4, 22],
            ["FlFloor", 0, FLOOR_Y - 0.2, 22, 0.4],
        ] as [string, number, number, number, number][]) {
            new GameObject(n)
                .addComponent(Transform2D, {
                    positionX: px, positionY: py,
                    scaleX: sx, scaleY: sy, zOrder: -1,
                })
                .addComponent(Mesh2D, { shape: Shape2D.Quad, color: 0x112233CC });
            this.objNames.push(n);
        }

        // Obstacle in the center
        const obsName = "FlObs";
        new GameObject(obsName)
            .addComponent(Transform2D, {
                positionX: 0, positionY: -3,
                scaleX: 3, scaleY: 3, zOrder: 0,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Circle, color: 0x223344DD });
        this.objNames.push(obsName);

        // Spawn initial fluid block
        for (let row = 0; row < 8; row++) {
            for (let col = 0; col < 10; col++) {
                this.spawnParticle(
                    WALL_LEFT + 1 + col * 0.5 + (row % 2) * 0.25,
                    2 + row * 0.5,
                );
            }
        }

        this.renderHUD();
        Log.info("Fluid Simulation (SPH) — Space to pour, R to reset");
    }

    private spawnParticle(x: number, y: number): void {
        if (this.particles.length >= MAX_PARTICLES) return;
        const name = `FP_${this.pIdx++}`;
        const hue = 200 + (this.pIdx * 3) % 60;
        const color = hsvToHex(hue, 0.7, 0.95, 0.85);

        new GameObject(name)
            .addComponent(Transform2D, {
                positionX: x, positionY: y,
                scaleX: PARTICLE_RADIUS * 2.5, scaleY: PARTICLE_RADIUS * 2.5,
                zOrder: 5,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Circle, color });
        this.objNames.push(name);

        this.particles.push({
            name, x, y, vx: 0, vy: 0,
            density: 0, pressure: 0, hue,
        });
    }

    private destroyAll(): void {
        for (const n of this.objNames) {
            const o = GameObject.find(n);
            if (o) GameObject.destroy(o);
        }
        this.objNames = [];
        this.particles = [];
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

        // Pour particles
        if (Input.isKeyDown("Space")) {
            this.emitterTimer += dt;
            if (this.emitterTimer > 0.06) {
                this.emitterTimer = 0;
                this.spawnParticle(
                    randomRange(-2, 2),
                    8 + randomRange(-0.5, 0.5),
                );
            }
        }
    }

    fixedUpdate(dt: number): void {
        const n = this.particles.length;
        if (n === 0) return;

        // Compute density
        for (let i = 0; i < n; i++) {
            const pi = this.particles[i];
            pi.density = 0;
            for (let j = 0; j < n; j++) {
                const pj = this.particles[j];
                const dx = pj.x - pi.x;
                const dy = pj.y - pi.y;
                const r2 = dx * dx + dy * dy;
                if (r2 < SMOOTHING_RADIUS_SQ) {
                    const r = Math.sqrt(r2);
                    const q = 1 - r / SMOOTHING_RADIUS;
                    pi.density += q * q;
                }
            }
            pi.pressure = GAS_CONSTANT * (pi.density - REST_DENSITY);
        }

        // Compute forces and integrate
        for (let i = 0; i < n; i++) {
            const pi = this.particles[i];
            let fx = 0, fy = GRAVITY;

            for (let j = 0; j < n; j++) {
                if (i === j) continue;
                const pj = this.particles[j];
                const dx = pj.x - pi.x;
                const dy = pj.y - pi.y;
                const r2 = dx * dx + dy * dy;
                if (r2 < SMOOTHING_RADIUS_SQ && r2 > 0.001) {
                    const r = Math.sqrt(r2);
                    const q = 1 - r / SMOOTHING_RADIUS;
                    const nx = dx / r;
                    const ny = dy / r;

                    // Pressure force
                    const pressureForce = -0.5 * (pi.pressure + pj.pressure) * q / (pi.density || 1);
                    fx += nx * pressureForce;
                    fy += ny * pressureForce;

                    // Viscosity
                    const dvx = pj.vx - pi.vx;
                    const dvy = pj.vy - pi.vy;
                    fx += VISCOSITY * dvx * q / (pi.density || 1);
                    fy += VISCOSITY * dvy * q / (pi.density || 1);
                }
            }

            // Obstacle collision (circle at 0, -3 r=1.5)
            const obsDx = pi.x - 0;
            const obsDy = pi.y - (-3);
            const obsDist = Math.sqrt(obsDx * obsDx + obsDy * obsDy);
            if (obsDist < 1.8) {
                const on = obsDist || 0.01;
                const onx = obsDx / on;
                const ony = obsDy / on;
                const overlap = 1.8 - obsDist;
                pi.x += onx * overlap;
                pi.y += ony * overlap;
                const dot = pi.vx * onx + pi.vy * ony;
                if (dot < 0) {
                    pi.vx -= dot * onx * 1.5;
                    pi.vy -= dot * ony * 1.5;
                }
            }

            pi.vx += fx * dt;
            pi.vy += fy * dt;

            // Damping
            pi.vx *= 0.998;
            pi.vy *= 0.998;

            pi.x += pi.vx * dt;
            pi.y += pi.vy * dt;

            // Boundary
            if (pi.y < FLOOR_Y + PARTICLE_RADIUS) {
                pi.y = FLOOR_Y + PARTICLE_RADIUS;
                pi.vy *= -0.3;
                pi.vx *= 0.95;
            }
            if (pi.x < WALL_LEFT + PARTICLE_RADIUS) {
                pi.x = WALL_LEFT + PARTICLE_RADIUS;
                pi.vx *= -0.3;
            }
            if (pi.x > WALL_RIGHT - PARTICLE_RADIUS) {
                pi.x = WALL_RIGHT - PARTICLE_RADIUS;
                pi.vx *= -0.3;
            }
            if (pi.y > CEILING_Y - PARTICLE_RADIUS) {
                pi.y = CEILING_Y - PARTICLE_RADIUS;
                pi.vy *= -0.3;
            }

            // Sync transform
            const obj = GameObject.find(pi.name);
            if (obj) {
                const t = obj.getComponent(Transform2D)!;
                t.positionX = pi.x;
                t.positionY = pi.y;

                // Color based on velocity (blue -> cyan -> white as speed increases)
                const speed = Math.sqrt(pi.vx * pi.vx + pi.vy * pi.vy);
                const speedHue = clamp(200 - speed * 8, 160, 220);
                const brightness = clamp(0.7 + speed * 0.03, 0.7, 1.0);
                const color = hsvToHex(speedHue, clamp(0.8 - speed * 0.02, 0.2, 0.8), brightness, 0.85);
                const mesh = obj.getComponent(Mesh2D);
                if (mesh) mesh.color = color;
            }
        }
    }

    private renderHUD(): void {
        Canvas.render(
            <View style={{
                position: "Absolute", top: 12, left: 12,
                backgroundColor: 0x00000088, padding: 8, borderRadius: 4,
            }}>
                <Label style={{ fontSize: 16, color: 0x33CCFFFF }}>
                    {"Fluid Simulation (SPH)"}
                </Label>
                <Label style={{ fontSize: 11, color: 0xAABBCC88, marginTop: 4 }}>
                    {"Hold Space: pour | R: reset | Esc: menu"}
                </Label>
                <Label style={{ fontSize: 10, color: 0x66778888, marginTop: 2 }}>
                    {`Particles: ${this.particles.length}/${MAX_PARTICLES}`}
                </Label>
            </View>
        );
    }
}
