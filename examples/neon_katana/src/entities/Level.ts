import { GameObject, Transform2D, Mesh2D, Shape2D, ParticleEmitter2D } from "bokken/gameObject";
import { LevelConfiguration } from "../constants.ts";
import { Bounds } from "../core/Bounds.ts";
import { Player } from "./Player.ts";

const ParticleEase = {
    Linear: 0,
    EaseInQuad: 1,
    EaseOutQuad: 2,
    EaseInOutQuad: 3,
    EaseOutCubic: 4,
    EaseInCubic: 5,
} as const;

// Seeded RNG (xorshift32).
function seededRandom(seed: number): () => number {
    let s = seed | 1;
    return () => {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (s >>> 0) / 0xFFFFFFFF;
    };
}

interface CrystalDef {
    name: string;
    x: number;
    y: number;
    alive: boolean;
}

interface AsteroidDef {
    name: string;
    x: number;
    y: number;
    size: number;
}

export class Level {
    // Stars — tracked by cell key.
    private readonly starCells: Map<string, string[]> = new Map();

    // Collectibles and obstacles.
    private readonly crystals: CrystalDef[] = [];
    private readonly asteroids: AsteroidDef[] = [];
    private crystalIndex = 0;
    private asteroidIndex = 0;

    // Ring-based spawning: we spawn content in concentric rings
    // around the player as they travel outward.
    private spawnedRings: Set<number> = new Set();
    private readonly ringStep = 5;

    private score = 0;
    private onScoreChanged: ((score: number) => void) | null = null;

    get currentScore(): number {
        return this.score;
    }

    setScoreCallback(callback: (score: number) => void): void {
        this.onScoreChanged = callback;
    }

    build(): void {
        this.score = 0;
        this.crystalIndex = 0;
        this.asteroidIndex = 0;
        this.spawnedRings.clear();

        // Starting crystals in a ring.
        for (let i = 0; i < 8; i++) {
            const angle = (i / 8) * Math.PI * 2;
            const dist = 5 + (i % 3) * 2;
            this.spawnCrystal(Math.cos(angle) * dist, Math.sin(angle) * dist);
        }
    }

    update(cameraX: number, cameraY: number): void {
        this.spawnRingsAround(cameraX, cameraY);
        this.cleanup(cameraX, cameraY);
    }

    checkCoinPickup(player: Player): void {
        for (const c of this.crystals) {
            if (!c.alive) continue;

            const obj = GameObject.find(c.name);
            if (!obj) continue;

            const ct = obj.getComponent(Transform2D);
            if (!ct) continue;

            const cb = new Bounds(
                ct.positionX, ct.positionY,
                LevelConfiguration.crystalSize, LevelConfiguration.crystalSize,
            );

            if (player.bounds.overlaps(cb)) {
                const sparkle = new GameObject(`Sparkle_${c.name}`)
                    .addComponent(Transform2D, { positionX: ct.positionX, positionY: ct.positionY })
                    .addComponent(ParticleEmitter2D, {
                        emitting: false,
                        colorStart: LevelConfiguration.crystalColor,
                        colorEnd: LevelConfiguration.crystalGlowColor,
                        speedMinimum: 2,
                        speedMaximum: 5,
                        sizeStart: 0.12,
                        sizeEnd: 0.01,
                        lifetimeMinimum: 0.2,
                        lifetimeMaximum: 0.5,
                        gravity: 0,
                        damping: 2.0,
                        spreadAngle: 360,
                        direction: 90,
                        alphaEase: ParticleEase.EaseOutCubic,
                        maximumParticles: 24,
                    });
                sparkle.getComponent(ParticleEmitter2D)!.burst(18);

                GameObject.destroy(obj);
                c.alive = false;
                this.score++;
                if (this.onScoreChanged) this.onScoreChanged(this.score);
            }
        }
    }

    resolveCollisions(player: Player): void {
        const pb = player.bounds;
        const rb = player.rigidbody;

        for (const a of this.asteroids) {
            const obj = GameObject.find(a.name);
            if (!obj) continue;

            const at = obj.getComponent(Transform2D);
            if (!at) continue;

            const ab = new Bounds(at.positionX, at.positionY, a.size, a.size);
            if (!pb.overlaps(ab)) continue;

            const dx = pb.x - ab.x;
            const dy = pb.y - ab.y;
            const dist = Math.sqrt(dx * dx + dy * dy) || 0.01;
            const nx = dx / dist;
            const ny = dy / dist;

            const overlap = (a.size / 2 + Math.max(pb.width, pb.height) / 2) - dist;
            if (overlap > 0) {
                player.transform.positionX += nx * overlap * 1.1;
                player.transform.positionY += ny * overlap * 1.1;
            }

            const dot = rb.velocityX * nx + rb.velocityY * ny;
            if (dot < 0) {
                rb.velocityX = rb.velocityX - 2 * dot * nx * 0.7;
                rb.velocityY = rb.velocityY - 2 * dot * ny * 0.7;
            }
        }
    }

    // Spawns crystals and asteroids in rings around the origin
    // as the player ventures further out.
    private spawnRingsAround(camX: number, camY: number): void {
        const playerDist = Math.sqrt(camX * camX + camY * camY);
        const maxRing = Math.floor((playerDist + 30) / this.ringStep);
        const minRing = Math.max(0, Math.floor((playerDist - 10) / this.ringStep));

        for (let ring = minRing; ring <= maxRing; ring++) {
            if (this.spawnedRings.has(ring)) continue;
            this.spawnedRings.add(ring);

            const ringDist = ring * this.ringStep;
            if (ringDist < 3) continue; // Skip center.

            const rand = seededRandom(ring * 48271 + 12345);
            const circumference = Math.PI * 2 * ringDist;
            const crystalCount = Math.max(1, Math.floor(circumference * 0.12));
            const asteroidCount = Math.max(0, Math.floor(circumference * 0.04));

            for (let i = 0; i < crystalCount; i++) {
                const angle = rand() * Math.PI * 2;
                const r = ringDist + (rand() - 0.5) * this.ringStep;
                this.spawnCrystal(Math.cos(angle) * r, Math.sin(angle) * r);
            }
        }
    }

    private spawnCrystal(x: number, y: number): void {
        const name = `Cr_${this.crystalIndex++}`;
        new GameObject(name)
            .addComponent(Transform2D, {
                positionX: x,
                positionY: y,
                scaleX: LevelConfiguration.crystalSize,
                scaleY: LevelConfiguration.crystalSize,
                zOrder: 5,
            })
            .addComponent(Mesh2D, {
                shape: Shape2D.Circle,
                color: LevelConfiguration.crystalColor,
            });
        this.crystals.push({ name, x, y, alive: true });
    }

    private cleanup(camX: number, camY: number): void {
        const r2 = LevelConfiguration.crystalCleanRadius * LevelConfiguration.crystalCleanRadius;

        for (let i = this.crystals.length - 1; i >= 0; i--) {
            const c = this.crystals[i];
            const dx = c.x - camX;
            const dy = c.y - camY;
            if (dx * dx + dy * dy > r2) {
                if (c.alive) {
                    const obj = GameObject.find(c.name);
                    if (obj) GameObject.destroy(obj);
                }
                this.crystals.splice(i, 1);
            }
        }

        const ar2 = r2 * 1.5;
        for (let i = this.asteroids.length - 1; i >= 0; i--) {
            const a = this.asteroids[i];
            const dx = a.x - camX;
            const dy = a.y - camY;
            if (dx * dx + dy * dy > ar2) {
                const obj = GameObject.find(a.name);
                if (obj) GameObject.destroy(obj);
                this.asteroids.splice(i, 1);
            }
        }
    }
}