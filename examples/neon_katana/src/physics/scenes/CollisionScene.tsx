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

interface PhysBox {
    name: string;
    x: number;
    y: number;
    w: number;
    h: number;
    vx: number;
    vy: number;
    mass: number;
    color: number;
    hue: number;
    isStatic: boolean;
    rotation: number;
    angularVel: number;
}

const GRAVITY = -18;
const FLOOR_Y = -9;
const WALL_LEFT = -16;
const WALL_RIGHT = 16;
const RESTITUTION = 0.5;
const FRICTION = 0.3;

export class CollisionScene implements Scene {
    private readonly game: Game;
    private camera!: GameObject;
    private boxes: PhysBox[] = [];
    private boxIdx = 0;
    private elapsed = 0;
    private objNames: string[] = [];

    constructor(game: Game) {
        this.game = game;
    }

    enter(): void {
        this.boxes = [];
        this.objNames = [];
        this.boxIdx = 0;
        this.elapsed = 0;

        this.camera = new GameObject("CollCam")
            .addComponent(Transform2D, { positionX: 0, positionY: 0 })
            .addComponent(Camera2D, { zoom: 22, isActive: true });
        this.objNames.push("CollCam");

        // Floor and walls
        this.spawnStatic(0, FLOOR_Y - 0.5, 34, 1.0, 0x1A2A3AFF);
        this.spawnStatic(WALL_LEFT - 0.5, 0, 1.0, 22, 0x1A2A3AFF);
        this.spawnStatic(WALL_RIGHT + 0.5, 0, 1.0, 22, 0x1A2A3AFF);

        // Newton's cradle area — suspended balls at top
        const cradleY = 5;
        const cradleSpacing = 1.2;
        const cradleStart = -3;
        for (let i = 0; i < 5; i++) {
            this.spawnBox(
                cradleStart + i * cradleSpacing, cradleY,
                0.8, 0.8,
                i === 0 ? -8 : 0, 0,
                2, (i * 60) % 360, false,
            );
        }

        // Domino chain on the right
        for (let i = 0; i < 10; i++) {
            this.spawnBox(
                5 + i * 1.0, FLOOR_Y + 1.2,
                0.3, 2.0,
                0, 0,
                1, (i * 30 + 180) % 360, false,
            );
        }

        // Pyramid of boxes on the left
        const pyramidBase = 5;
        const pyramidStartX = -12;
        for (let row = 0; row < pyramidBase; row++) {
            const count = pyramidBase - row;
            const startX = pyramidStartX - count * 0.6 + 0.6;
            for (let col = 0; col < count; col++) {
                this.spawnBox(
                    startX + col * 1.2,
                    FLOOR_Y + 0.5 + row * 1.0,
                    1.0, 0.9,
                    0, 0,
                    1, ((row * 5 + col * 20) + 60) % 360, false,
                );
            }
        }

        // Wrecking ball (big heavy circle)
        this.spawnBox(
            -16, 4, 2, 2,
            20, -5,
            15, 0, false,
        );

        this.renderHUD();
        Log.info("Collision Demo — Space to launch projectile, R to reset");
    }

    private spawnStatic(x: number, y: number, w: number, h: number, color: number): void {
        const name = `CStatic_${this.boxIdx++}`;
        new GameObject(name)
            .addComponent(Transform2D, {
                positionX: x, positionY: y,
                scaleX: w, scaleY: h, zOrder: -1,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Quad, color });
        this.objNames.push(name);
        this.boxes.push({
            name, x, y, w, h, vx: 0, vy: 0, mass: 999999,
            color, hue: 0, isStatic: true, rotation: 0, angularVel: 0,
        });
    }

    private spawnBox(
        x: number, y: number, w: number, h: number,
        vx: number, vy: number, mass: number, hue: number,
        isStatic: boolean,
    ): void {
        const name = `CBox_${this.boxIdx++}`;
        const color = hsvToHex(hue, 0.7, 0.9, 0.9);
        new GameObject(name)
            .addComponent(Transform2D, {
                positionX: x, positionY: y,
                scaleX: w, scaleY: h, zOrder: 5,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Quad, color });
        this.objNames.push(name);
        this.boxes.push({
            name, x, y, w, h, vx, vy, mass,
            color, hue, isStatic, rotation: 0, angularVel: 0,
        });
    }

    private destroyAll(): void {
        for (const n of this.objNames) {
            const o = GameObject.find(n);
            if (o) GameObject.destroy(o);
        }
        this.objNames = [];
        this.boxes = [];
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

        // Launch projectile
        if (Input.isKeyPressed("Space")) {
            this.spawnBox(
                WALL_LEFT + 2, 7,
                randomRange(0.6, 1.5), randomRange(0.6, 1.5),
                randomRange(10, 25), randomRange(-15, -5),
                randomRange(3, 10),
                Math.random() * 360,
                false,
            );
        }
    }

    fixedUpdate(dt: number): void {
        const dynamics = this.boxes.filter(b => !b.isStatic);
        const statics = this.boxes.filter(b => b.isStatic);

        for (const b of dynamics) {
            // Gravity
            b.vy += GRAVITY * dt;

            // Update position
            b.x += b.vx * dt;
            b.y += b.vy * dt;
            b.rotation += b.angularVel * dt;

            // Collide with statics (simple AABB)
            for (const s of statics) {
                this.resolveAABB(b, s);
            }

            // Collide with other dynamics
            for (const other of dynamics) {
                if (other === b) continue;
                this.resolveAABB(b, other);
            }

            // World bounds
            if (b.y - b.h / 2 < FLOOR_Y) {
                b.y = FLOOR_Y + b.h / 2;
                b.vy = Math.abs(b.vy) * RESTITUTION;
                if (Math.abs(b.vy) < 0.5) b.vy = 0;
                b.vx *= (1 - FRICTION);
                b.angularVel += b.vx * 2;
                b.angularVel *= 0.95;
            }
            if (b.x - b.w / 2 < WALL_LEFT) {
                b.x = WALL_LEFT + b.w / 2;
                b.vx = Math.abs(b.vx) * RESTITUTION;
            }
            if (b.x + b.w / 2 > WALL_RIGHT) {
                b.x = WALL_RIGHT - b.w / 2;
                b.vx = -Math.abs(b.vx) * RESTITUTION;
            }

            // Sync
            const obj = GameObject.find(b.name);
            if (obj) {
                const t = obj.getComponent(Transform2D)!;
                t.positionX = b.x;
                t.positionY = b.y;

                // Squash on impact
                const speed = Math.sqrt(b.vx * b.vx + b.vy * b.vy);
                const squash = clamp(1 - speed * 0.003, 0.85, 1);
                t.scaleX = b.w / squash;
                t.scaleY = b.h * squash;
            }
        }
    }

    private resolveAABB(a: PhysBox, b: PhysBox): void {
        // AABB overlap check
        const aLeft = a.x - a.w / 2;
        const aRight = a.x + a.w / 2;
        const aTop = a.y + a.h / 2;
        const aBottom = a.y - a.h / 2;
        const bLeft = b.x - b.w / 2;
        const bRight = b.x + b.w / 2;
        const bTop = b.y + b.h / 2;
        const bBottom = b.y - b.h / 2;

        if (aRight <= bLeft || aLeft >= bRight || aTop <= bBottom || aBottom >= bTop) return;

        // Find minimum overlap
        const overlapX = Math.min(aRight - bLeft, bRight - aLeft);
        const overlapY = Math.min(aTop - bBottom, bTop - aBottom);

        if (overlapX < overlapY) {
            // Resolve horizontally
            const sign = a.x < b.x ? -1 : 1;
            if (!a.isStatic) a.x += sign * overlapX * 0.5;
            if (!b.isStatic) b.x -= sign * overlapX * 0.5;

            // Exchange velocity
            if (!b.isStatic) {
                const totalMass = a.mass + b.mass;
                const newVxA = (a.vx * (a.mass - b.mass) + 2 * b.mass * b.vx) / totalMass;
                const newVxB = (b.vx * (b.mass - a.mass) + 2 * a.mass * a.vx) / totalMass;
                a.vx = newVxA * RESTITUTION;
                b.vx = newVxB * RESTITUTION;
            } else {
                a.vx = -a.vx * RESTITUTION;
            }
        } else {
            // Resolve vertically
            const sign = a.y < b.y ? -1 : 1;
            if (!a.isStatic) a.y += sign * overlapY * 0.5;
            if (!b.isStatic) b.y -= sign * overlapY * 0.5;

            if (!b.isStatic) {
                const totalMass = a.mass + b.mass;
                const newVyA = (a.vy * (a.mass - b.mass) + 2 * b.mass * b.vy) / totalMass;
                const newVyB = (b.vy * (b.mass - a.mass) + 2 * a.mass * a.vy) / totalMass;
                a.vy = newVyA * RESTITUTION;
                b.vy = newVyB * RESTITUTION;
            } else {
                a.vy = -a.vy * RESTITUTION;
            }

            // Angular velocity from impact
            a.angularVel += (a.vx - (b.isStatic ? 0 : b.vx)) * 0.5;
            if (!b.isStatic) {
                b.angularVel -= (a.vx - b.vx) * 0.5;
            }
        }
    }

    private renderHUD(): void {
        Canvas.render(
            <View style={{
                position: "Absolute", top: 12, left: 12,
                backgroundColor: 0x00000088, padding: 8, borderRadius: 4,
            }}>
                <Label style={{ fontSize: 16, color: 0xFF5555FF }}>
                    {"Rigid Body Collisions"}
                </Label>
                <Label style={{ fontSize: 11, color: 0xAABBCC88, marginTop: 4 }}>
                    {"Space: launch box | R: reset | Esc: menu"}
                </Label>
            </View>
        );
    }
}
