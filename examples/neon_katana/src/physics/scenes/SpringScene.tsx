import { GameObject, Transform2D, Mesh2D, Shape2D, Camera2D, ParticleEmitter2D } from "bokken/gameObject";
import Canvas, { View, Label } from "bokken/canvas";
import Input from "bokken/input";
import Log from "bokken/log";
import { type Scene, GameState } from "../../core/GameState.ts";
import { Game } from "../../core/Game.ts";
import { hsvToHex, clamp } from "../PhysicsUtils.ts";

const ParticleEase = {
    Linear: 0, EaseInQuad: 1, EaseOutQuad: 2,
    EaseInOutQuad: 3, EaseOutCubic: 4, EaseInCubic: 5,
} as const;

interface SpringNode {
    name: string;
    x: number;
    y: number;
    vx: number;
    vy: number;
    anchorX: number;
    anchorY: number;
    radius: number;
    color: number;
    hue: number;
    stiffness: number;
    damping: number;
    linkName: string;
}

interface PendulumBob {
    name: string;
    angle: number;
    angularVel: number;
    length: number;
    radius: number;
    color: number;
    hue: number;
    pivotX: number;
    pivotY: number;
    rodName: string;
    trailNames: string[];
    trailIdx: number;
    trailTimer: number;
}

const GRAVITY = 15;
const NUM_SPRINGS = 7;
const NUM_PENDULUMS = 5;

export class SpringScene implements Scene {
    private readonly game: Game;
    private camera!: GameObject;
    private springs: SpringNode[] = [];
    private pendulums: PendulumBob[] = [];
    private elapsed = 0;
    private objNames: string[] = [];

    constructor(game: Game) {
        this.game = game;
    }

    enter(): void {
        this.springs = [];
        this.pendulums = [];
        this.objNames = [];
        this.elapsed = 0;

        this.camera = new GameObject("SpringCam")
            .addComponent(Transform2D, { positionX: 0, positionY: 0 })
            .addComponent(Camera2D, { zoom: 24, isActive: true });
        this.objNames.push("SpringCam");

        // Title dividers
        this.makeLabel("SprLabel", -6, 8.5, "Springs", 0xFFAA33FF);
        this.makeLabel("PenLabel", 6, 8.5, "Pendulums", 0x33CCFFFF);

        // Divider line
        const divName = "SprDiv";
        new GameObject(divName)
            .addComponent(Transform2D, {
                positionX: 0, positionY: 0,
                scaleX: 0.06, scaleY: 18, zOrder: 0,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Quad, color: 0x334455AA });
        this.objNames.push(divName);

        // Create springs on the left side
        for (let i = 0; i < NUM_SPRINGS; i++) {
            const anchorX = -12 + i * 2;
            const anchorY = 6;
            const hue = (i * 50 + 10) % 360;
            const color = hsvToHex(hue, 0.85, 1.0, 0.9);
            const radius = 0.3 + i * 0.05;
            const name = `SprNode_${i}`;
            const linkName = `SprLink_${i}`;

            // Anchor marker
            const aName = `SprAnc_${i}`;
            new GameObject(aName)
                .addComponent(Transform2D, {
                    positionX: anchorX, positionY: anchorY,
                    scaleX: 0.15, scaleY: 0.15, zOrder: 3,
                })
                .addComponent(Mesh2D, { shape: Shape2D.Circle, color: 0xFFFFFF55 });
            this.objNames.push(aName);

            // Spring link visual (rod)
            new GameObject(linkName)
                .addComponent(Transform2D, {
                    positionX: anchorX, positionY: anchorY,
                    scaleX: 0.04, scaleY: 1, zOrder: 1,
                })
                .addComponent(Mesh2D, { shape: Shape2D.Quad, color: hsvToHex(hue, 0.4, 0.6, 0.5) });
            this.objNames.push(linkName);

            // Ball
            new GameObject(name)
                .addComponent(Transform2D, {
                    positionX: anchorX, positionY: anchorY - 3 - i * 0.5,
                    scaleX: radius * 2, scaleY: radius * 2, zOrder: 5,
                })
                .addComponent(Mesh2D, { shape: Shape2D.Circle, color });
            this.objNames.push(name);

            this.springs.push({
                name, x: anchorX, y: anchorY - 3 - i * 0.5,
                vx: (i % 2 === 0 ? 3 : -3) + i, vy: i * 2,
                anchorX, anchorY, radius, color, hue,
                stiffness: 20 + i * 8,
                damping: 0.6 + i * 0.1,
                linkName,
            });
        }

        // Create pendulums on the right side
        for (let i = 0; i < NUM_PENDULUMS; i++) {
            const pivotX = 3 + i * 2.5;
            const pivotY = 6;
            const hue = (i * 70 + 180) % 360;
            const color = hsvToHex(hue, 0.8, 1.0, 0.9);
            const length = 3 + i * 0.8;
            const radius = 0.35;
            const name = `PenBob_${i}`;
            const rodName = `PenRod_${i}`;

            // Pivot point
            const pName = `PenPiv_${i}`;
            new GameObject(pName)
                .addComponent(Transform2D, {
                    positionX: pivotX, positionY: pivotY,
                    scaleX: 0.18, scaleY: 0.18, zOrder: 3,
                })
                .addComponent(Mesh2D, { shape: Shape2D.Circle, color: 0xFFFFFF55 });
            this.objNames.push(pName);

            // Rod
            new GameObject(rodName)
                .addComponent(Transform2D, {
                    positionX: pivotX, positionY: pivotY,
                    scaleX: 0.04, scaleY: length, zOrder: 1,
                })
                .addComponent(Mesh2D, { shape: Shape2D.Quad, color: hsvToHex(hue, 0.3, 0.5, 0.5) });
            this.objNames.push(rodName);

            // Bob
            new GameObject(name)
                .addComponent(Transform2D, {
                    positionX: pivotX, positionY: pivotY - length,
                    scaleX: radius * 2, scaleY: radius * 2, zOrder: 5,
                })
                .addComponent(Mesh2D, { shape: Shape2D.Circle, color });
            this.objNames.push(name);

            this.pendulums.push({
                name, angle: Math.PI / 4 + i * 0.3,
                angularVel: 0, length, radius, color, hue,
                pivotX, pivotY, rodName,
                trailNames: [], trailIdx: 0, trailTimer: 0,
            });
        }

        this.renderHUD();
        Log.info("Spring & Pendulum Demo — Space to perturb, R to reset");
    }

    private makeLabel(name: string, x: number, y: number, _text: string, color: number): void {
        // Use a small circle as a section marker since we can't render text in world space
        new GameObject(name)
            .addComponent(Transform2D, {
                positionX: x, positionY: y,
                scaleX: 0.3, scaleY: 0.3, zOrder: 10,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Circle, color });
        this.objNames.push(name);
    }

    private destroyAll(): void {
        for (const n of this.objNames) {
            const o = GameObject.find(n);
            if (o) GameObject.destroy(o);
        }
        for (const p of this.pendulums) {
            for (const tn of p.trailNames) {
                const o = GameObject.find(tn);
                if (o) GameObject.destroy(o);
            }
        }
        this.objNames = [];
        this.springs = [];
        this.pendulums = [];
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

        // Perturb on space
        if (Input.isKeyPressed("Space")) {
            for (const s of this.springs) {
                s.vx += (Math.random() - 0.5) * 15;
                s.vy += (Math.random() - 0.5) * 15;
            }
            for (const p of this.pendulums) {
                p.angularVel += (Math.random() - 0.5) * 5;
            }
        }

        // Pendulum trails
        for (const p of this.pendulums) {
            p.trailTimer += dt;
            if (p.trailTimer >= 0.04) {
                p.trailTimer = 0;
                const bobX = p.pivotX + Math.sin(p.angle) * p.length;
                const bobY = p.pivotY - Math.cos(p.angle) * p.length;
                const tn = `PTr_${p.name}_${p.trailIdx++}`;
                new GameObject(tn)
                    .addComponent(Transform2D, {
                        positionX: bobX, positionY: bobY,
                        scaleX: 0.12, scaleY: 0.12, zOrder: 2,
                    })
                    .addComponent(Mesh2D, { shape: Shape2D.Circle, color: hsvToHex(p.hue, 0.4, 0.5, 0.25) });
                p.trailNames.push(tn);
                if (p.trailNames.length > 20) {
                    const old = p.trailNames.shift()!;
                    const o = GameObject.find(old);
                    if (o) GameObject.destroy(o);
                }
            }
        }
    }

    fixedUpdate(dt: number): void {
        // Springs: Hooke's law F = -kx, with gravity
        for (const s of this.springs) {
            const dx = s.x - s.anchorX;
            const dy = s.y - s.anchorY;
            const fx = -s.stiffness * dx - s.damping * s.vx;
            const fy = -s.stiffness * dy - s.damping * s.vy + s.radius * GRAVITY;

            s.vx += fx * dt;
            s.vy += fy * dt;
            s.x += s.vx * dt;
            s.y += s.vy * dt;

            // Sync visuals
            const obj = GameObject.find(s.name);
            if (obj) {
                const t = obj.getComponent(Transform2D)!;
                t.positionX = s.x;
                t.positionY = s.y;

                // Stretch
                const speed = Math.sqrt(s.vx * s.vx + s.vy * s.vy);
                const stretch = clamp(1 + speed * 0.02, 1, 1.5);
                t.scaleX = s.radius * 2 / stretch;
                t.scaleY = s.radius * 2 * stretch;
            }

            // Update link
            const link = GameObject.find(s.linkName);
            if (link) {
                const lt = link.getComponent(Transform2D)!;
                const midX = (s.anchorX + s.x) / 2;
                const midY = (s.anchorY + s.y) / 2;
                lt.positionX = midX;
                lt.positionY = midY;
                const linkLen = Math.sqrt(dx * dx + dy * dy);
                lt.scaleY = linkLen;
                lt.rotation = Math.atan2(dx, dy) * (180 / Math.PI) * -1;
            }
        }

        // Pendulums: angular acceleration = -(g/L) * sin(theta)
        for (const p of this.pendulums) {
            const angularAccel = -(GRAVITY / p.length) * Math.sin(p.angle);
            p.angularVel += angularAccel * dt;
            p.angularVel *= 0.999; // Tiny damping
            p.angle += p.angularVel * dt;

            const bobX = p.pivotX + Math.sin(p.angle) * p.length;
            const bobY = p.pivotY - Math.cos(p.angle) * p.length;

            // Bob
            const obj = GameObject.find(p.name);
            if (obj) {
                const t = obj.getComponent(Transform2D)!;
                t.positionX = bobX;
                t.positionY = bobY;
            }

            // Rod
            const rod = GameObject.find(p.rodName);
            if (rod) {
                const rt = rod.getComponent(Transform2D)!;
                rt.positionX = (p.pivotX + bobX) / 2;
                rt.positionY = (p.pivotY + bobY) / 2;
                rt.scaleY = p.length;
                const dx = bobX - p.pivotX;
                const dy = bobY - p.pivotY;
                rt.rotation = Math.atan2(dx, dy) * (180 / Math.PI) * -1;
            }
        }
    }

    private renderHUD(): void {
        Canvas.render(
            <View style={{
                position: "Absolute", top: 12, left: 12,
                backgroundColor: 0x00000088, padding: 8, borderRadius: 4,
            }}>
                <Label style={{ fontSize: 16, color: 0xFFAA33FF }}>
                    {"Springs & Pendulums"}
                </Label>
                <Label style={{ fontSize: 11, color: 0xAABBCC88, marginTop: 4 }}>
                    {"Space: perturb | R: reset | Esc: menu"}
                </Label>
            </View>
        );
    }
}
