import { GameObject, Transform2D, Mesh2D, Shape2D, Camera2D, ParticleEmitter2D } from "bokken/gameObject";
import Canvas, { View, Label, Align, Timing } from "bokken/canvas";
import Input from "bokken/input";
import Log from "bokken/log";
import { type Scene, GameState } from "../../core/GameState.ts";
import { Game } from "../../core/Game.ts";
import { randomRange, hsvToHex, clamp, dist } from "../PhysicsUtils.ts";

const ParticleEase = {
    Linear: 0, EaseInQuad: 1, EaseOutQuad: 2,
    EaseInOutQuad: 3, EaseOutCubic: 4, EaseInCubic: 5,
} as const;

interface Ball {
    name: string;
    x: number;
    y: number;
    vx: number;
    vy: number;
    radius: number;
    mass: number;
    color: number;
    hue: number;
    trail: string[];
    trailTimer: number;
    trailIdx: number;
}

const GRAVITY = -15;
const FLOOR_Y = -8;
const WALL_LEFT = -14;
const WALL_RIGHT = 14;
const CEILING_Y = 10;
const RESTITUTION = 0.75;
const MAX_BALLS = 40;
const TRAIL_INTERVAL = 0.03;
const MAX_TRAIL = 12;

export class GravityScene implements Scene {
    private readonly game: Game;
    private camera!: GameObject;
    private balls: Ball[] = [];
    private ballIdx = 0;
    private spawnTimer = 0;
    private elapsed = 0;
    private floor!: GameObject;
    private wallL!: GameObject;
    private wallR!: GameObject;

    constructor(game: Game) {
        this.game = game;
    }

    enter(): void {
        // Camera
        this.camera = new GameObject("GravCam")
            .addComponent(Transform2D, { positionX: 0, positionY: 1 })
            .addComponent(Camera2D, { zoom: 28, isActive: true });

        // Floor
        this.floor = new GameObject("GravFloor")
            .addComponent(Transform2D, {
                positionX: 0, positionY: FLOOR_Y - 0.25,
                scaleX: 30, scaleY: 0.5, zOrder: -1,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Quad, color: 0x1A2A3AFF });

        // Walls
        this.wallL = new GameObject("GravWallL")
            .addComponent(Transform2D, {
                positionX: WALL_LEFT - 0.25, positionY: 1,
                scaleX: 0.5, scaleY: 20, zOrder: -1,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Quad, color: 0x1A2A3AFF });

        this.wallR = new GameObject("GravWallR")
            .addComponent(Transform2D, {
                positionX: WALL_RIGHT + 0.25, positionY: 1,
                scaleX: 0.5, scaleY: 20, zOrder: -1,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Quad, color: 0x1A2A3AFF });

        this.balls = [];
        this.ballIdx = 0;
        this.elapsed = 0;
        this.spawnTimer = 0;

        // Spawn initial batch
        for (let i = 0; i < 12; i++) {
            this.spawnBall();
        }

        this.renderHUD();
        Log.info("Gravity Demo — watch balls bounce! Press R to reset, Space to spawn more");
    }

    private spawnBall(): void {
        if (this.balls.length >= MAX_BALLS) {
            // Remove oldest
            const old = this.balls.shift()!;
            const obj = GameObject.find(old.name);
            if (obj) GameObject.destroy(obj);
            for (const tn of old.trail) {
                const t = GameObject.find(tn);
                if (t) GameObject.destroy(t);
            }
        }

        const idx = this.ballIdx++;
        const name = `GBall_${idx}`;
        const hue = (idx * 37) % 360;
        const radius = randomRange(0.25, 0.7);
        const color = hsvToHex(hue, 0.8, 1.0, 0.9);

        const x = randomRange(WALL_LEFT + 2, WALL_RIGHT - 2);
        const y = randomRange(4, 9);
        const vx = randomRange(-6, 6);
        const vy = randomRange(-2, 8);

        new GameObject(name)
            .addComponent(Transform2D, {
                positionX: x, positionY: y,
                scaleX: radius * 2, scaleY: radius * 2,
                zOrder: 5,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Circle, color });

        this.balls.push({
            name, x, y, vx, vy,
            radius, mass: radius * radius * Math.PI,
            color, hue, trail: [], trailTimer: 0, trailIdx: 0,
        });
    }

    private destroyAll(): void {
        for (const b of this.balls) {
            const obj = GameObject.find(b.name);
            if (obj) GameObject.destroy(obj);
            for (const tn of b.trail) {
                const t = GameObject.find(tn);
                if (t) GameObject.destroy(t);
            }
        }
        this.balls = [];

        // Destroy walls/floor/camera
        for (const n of ["GravFloor", "GravWallL", "GravWallR", "GravCam"]) {
            const o = GameObject.find(n);
            if (o) GameObject.destroy(o);
        }
    }

    update(dt: number): void {
        this.elapsed += dt;

        // Auto-spawn periodically
        this.spawnTimer += dt;
        if (this.spawnTimer > 1.5) {
            this.spawnTimer = 0;
            this.spawnBall();
        }

        // Manual spawn
        if (Input.isKeyPressed("Space")) {
            for (let i = 0; i < 5; i++) this.spawnBall();
        }

        // Reset
        if (Input.isKeyPressed("KeyR")) {
            this.destroyAll();
            this.enter();
            return;
        }

        // Back to menu
        if (Input.isKeyPressed("Escape")) {
            this.destroyAll();
            this.game.transition(GameState.MainMenu);
            return;
        }

        // Update trail visuals
        for (const b of this.balls) {
            b.trailTimer += dt;
            if (b.trailTimer >= TRAIL_INTERVAL) {
                b.trailTimer = 0;
                const tn = `GBTr_${b.name}_${b.trailIdx++}`;
                const trailSize = b.radius * 0.6;
                const trailColor = hsvToHex(b.hue, 0.5, 0.6, 0.3);
                new GameObject(tn)
                    .addComponent(Transform2D, {
                        positionX: b.x, positionY: b.y,
                        scaleX: trailSize, scaleY: trailSize,
                        zOrder: 2,
                    })
                    .addComponent(Mesh2D, { shape: Shape2D.Circle, color: trailColor });
                b.trail.push(tn);

                if (b.trail.length > MAX_TRAIL) {
                    const old = b.trail.shift()!;
                    const o = GameObject.find(old);
                    if (o) GameObject.destroy(o);
                }
            }

            // Fade and shrink trails
            for (let i = 0; i < b.trail.length; i++) {
                const tObj = GameObject.find(b.trail[i]);
                if (!tObj) continue;
                const tt = tObj.getComponent(Transform2D);
                if (!tt) continue;
                const progress = i / b.trail.length;
                const scale = b.radius * 0.6 * progress;
                tt.scaleX = scale;
                tt.scaleY = scale;
            }
        }
    }

    fixedUpdate(dt: number): void {
        for (const b of this.balls) {
            // Gravity
            b.vy += GRAVITY * dt;

            // Update position
            b.x += b.vx * dt;
            b.y += b.vy * dt;

            // Floor bounce
            if (b.y - b.radius < FLOOR_Y) {
                b.y = FLOOR_Y + b.radius;
                b.vy = Math.abs(b.vy) * RESTITUTION;
                if (Math.abs(b.vy) < 0.5) b.vy = 0;
                b.vx *= 0.98; // Friction
            }

            // Ceiling
            if (b.y + b.radius > CEILING_Y) {
                b.y = CEILING_Y - b.radius;
                b.vy = -Math.abs(b.vy) * RESTITUTION;
            }

            // Wall bounce
            if (b.x - b.radius < WALL_LEFT) {
                b.x = WALL_LEFT + b.radius;
                b.vx = Math.abs(b.vx) * RESTITUTION;
            }
            if (b.x + b.radius > WALL_RIGHT) {
                b.x = WALL_RIGHT - b.radius;
                b.vx = -Math.abs(b.vx) * RESTITUTION;
            }

            // Ball-ball collisions
            for (const other of this.balls) {
                if (other === b) continue;
                const dx = other.x - b.x;
                const dy = other.y - b.y;
                const d = Math.sqrt(dx * dx + dy * dy) || 0.001;
                const minDist = b.radius + other.radius;
                if (d < minDist) {
                    // Separate
                    const nx = dx / d;
                    const ny = dy / d;
                    const overlap = minDist - d;
                    const totalMass = b.mass + other.mass;
                    b.x -= nx * overlap * (other.mass / totalMass);
                    b.y -= ny * overlap * (other.mass / totalMass);
                    other.x += nx * overlap * (b.mass / totalMass);
                    other.y += ny * overlap * (b.mass / totalMass);

                    // Elastic collision
                    const dvx = b.vx - other.vx;
                    const dvy = b.vy - other.vy;
                    const dot = dvx * nx + dvy * ny;
                    if (dot > 0) {
                        const impulse = (2 * dot) / totalMass;
                        b.vx -= impulse * other.mass * nx * RESTITUTION;
                        b.vy -= impulse * other.mass * ny * RESTITUTION;
                        other.vx += impulse * b.mass * nx * RESTITUTION;
                        other.vy += impulse * b.mass * ny * RESTITUTION;
                    }
                }
            }

            // Sync transform
            const obj = GameObject.find(b.name);
            if (obj) {
                const t = obj.getComponent(Transform2D)!;
                t.positionX = b.x;
                t.positionY = b.y;

                // Squash & stretch based on velocity
                const speed = Math.sqrt(b.vx * b.vx + b.vy * b.vy);
                const stretch = clamp(1 + speed * 0.015, 1, 1.4);
                const angle = Math.atan2(b.vy, b.vx) * (180 / Math.PI);
                t.rotation = angle - 90;
                t.scaleX = b.radius * 2 / stretch;
                t.scaleY = b.radius * 2 * stretch;
            }
        }
    }

    private renderHUD(): void {
        Canvas.render(
            <View style={{
                position: "Absolute", top: 12, left: 12,
                backgroundColor: 0x00000088, padding: 8, borderRadius: 4,
            }}>
                <Label style={{ fontSize: 16, color: 0xFF8844FF }}>
                    {"Gravity + Bounce"}
                </Label>
                <Label style={{ fontSize: 11, color: 0xAABBCC88, marginTop: 4 }}>
                    {"Space: spawn | R: reset | Esc: menu"}
                </Label>
            </View>
        );
    }
}
