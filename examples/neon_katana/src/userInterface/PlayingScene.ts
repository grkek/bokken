import { GameObject, Transform2D, Camera2D } from "bokken/gameObject";
import Log from "bokken/log";
import { type Scene } from "../core/GameState.ts";
import { Player } from "../entities/Player.ts";
import { Level } from "../entities/Level.ts";
import { HeadsUpDisplay } from "./HeadsUpDisplay.tsx";

export class PlayingScene implements Scene {
    private readonly player: Player;
    private readonly level: Level;
    private readonly hud: HeadsUpDisplay;
    private readonly camera: GameObject;

    constructor() {
        this.player = new Player();
        this.level = new Level();
        this.hud = new HeadsUpDisplay();

        this.camera = new GameObject("Camera")
            .addComponent(Transform2D)
            .addComponent(Camera2D, { zoom: 32, isActive: true });

        this.level.setScoreCallback((score) => this.hud.setScore(score));
    }

    enter(): void {
        this.level.build();
        this.hud.render();
        Log.info("WASD / Arrows to fly, Space to boost!");
    }

    update(dt: number): void {
        this.hud.update(dt);
        this.player.handleInput(dt);

        const px = this.player.transform.positionX;
        const py = this.player.transform.positionY;

        // Camera follows player.
        const camTransform = this.camera.getComponent(Transform2D)!;
        camTransform.positionX = px;
        camTransform.positionY = py;

        this.level.update(px, py);
        this.level.checkCoinPickup(this.player);
    }

    fixedUpdate(dt: number): void {
        this.player.applyGravity(dt);
        this.level.resolveCollisions(this.player);
    }
}