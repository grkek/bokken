import { GameObject, Transform2D, Mesh2D, Shape2D } from "bokken/gameObject";
import Log from "bokken/log";
import { LevelConfiguration } from "../constants.ts";
import { Bounds } from "../core/Bounds.ts";
import { Player } from "./Player.ts";

interface PlatformDefinition {
    name: string;
    x: number;
    y: number;
    width: number;
    height: number;
    isGround?: boolean;
}

interface CoinDefintion {
    name: string;
    x: number;
    y: number;
}

export class Level {
    private readonly platformDefintions: PlatformDefinition[];
    private readonly coinDefinitions: CoinDefintion[];

    constructor() {
        this.platformDefintions = [
            { name: "Ground", x: 0, y: -4.0, width: 12, height: 0.5, isGround: true },
            { name: "PlatformLeft", x: -3, y: -1.5, width: 3, height: 0.4 },
            { name: "PlatformRight", x: 3, y: 0.0, width: 3, height: 0.4 },
            { name: "PlatformTop", x: 0, y: 1.5, width: 2, height: 0.4 },
        ];

        this.coinDefinitions = [
            { name: "Coin", x: 3, y: 1.5 },
        ];
    }

    build(): void {
        for (const p of this.platformDefintions) {
            new GameObject(p.name)
                .addComponent(Transform2D, {
                    positionX: p.x,
                    positionY: p.y,
                    scaleX: p.width,
                    scaleY: p.height,
                })
                .addComponent(Mesh2D, {
                    shape: Shape2D.Quad,
                    color: p.isGround ? LevelConfiguration.groundColor : LevelConfiguration.platformColor,
                });
        }

        for (const c of this.coinDefinitions) {
            new GameObject(c.name)
                .addComponent(Transform2D, {
                    positionX: c.x,
                    positionY: c.y,
                    scaleX: LevelConfiguration.coinSize,
                    scaleY: LevelConfiguration.coinSize,
                })
                .addComponent(Mesh2D, { shape: Shape2D.Quad, color: LevelConfiguration.coinColor });
        }
    }

    getPlatformBounds(): Bounds[] {
        const result: Bounds[] = [];

        for (const p of this.platformDefintions) {
            const obj = GameObject.find(p.name);
            if (!obj) continue;
            const t = obj.getComponent(Transform2D);
            if (!t) continue;
            result.push(new Bounds(t.positionX, t.positionY, t.scaleX, t.scaleY));
        }

        return result;
    }

    checkCoinPickup(player: Player): void {
        for (const c of this.coinDefinitions) {
            const obj = GameObject.find(c.name);
            if (!obj) continue;

            const ct = obj.getComponent(Transform2D);
            if (!ct) continue;

            const coinBounds = new Bounds(ct.positionX, ct.positionY, LevelConfiguration.coinSize, LevelConfiguration.coinSize);

            if (player.bounds.overlaps(coinBounds)) {
                GameObject.destroy(obj);
                Log.info("Coin collected!");
            }
        }
    }

    resolveCollisions(player: Player): void {
        const platforms = this.getPlatformBounds();
        const pb = player.bounds;
        const rb = player.rigidbody;

        player.isGrounded = false;

        for (const plat of platforms) {
            if (!pb.overlaps(plat)) continue;

            if (rb.velocityY < 0 && pb.y > plat.y) {
                player.transform.positionY = plat.top + (pb.height / 2);
                rb.velocityY = 0;
                player.isGrounded = true;
            } else if (rb.velocityY > 0 && pb.y < plat.y) {
                rb.velocityY = 0;
            }
        }
    }
}
