import Log from "bokken/log";
import { type Scene } from "../core/GameState.ts";
import { Player } from "../entities/Player.ts";
import { Level } from "../entities/Level.ts";
import { HeadsUpDisplay } from "./HeadsUpDisplay.tsx";

export class PlayingScene implements Scene {
    private readonly player: Player;
    private readonly level: Level;
    private readonly hud: HeadsUpDisplay;

    constructor() {
        this.player = new Player();
        this.level = new Level();
        this.hud = new HeadsUpDisplay();
    }

    enter(): void {
        this.level.build();
        this.hud.render();
        Log.info("Arrow keys to move, Space to jump!");
    }

    update(dt: number): void {
        this.hud.update(dt);
        this.player.handleInput();
        this.level.checkCoinPickup(this.player);

        if (this.player.hasFallen()) {
            this.player.respawn();
        }
    }

    fixedUpdate(dt: number): void {
        this.player.applyGravity(dt);
        this.level.resolveCollisions(this.player);
    }
}
