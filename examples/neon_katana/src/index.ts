import { GameState } from "./core/GameState.ts";
import { Game } from "./core/Game.ts";
import { MainMenuScene } from "./userInterface/MainMenuScene.tsx";
import { PlayingScene } from "./userInterface/PlayingScene.ts";
import { GravityScene } from "./physics/scenes/GravityScene.ts";
import { CollisionScene } from "./physics/scenes/CollisionScene.ts";
import { SpringScene } from "./physics/scenes/SpringScene.ts";
import { FluidScene } from "./physics/scenes/FluidScene.ts";
import { OrbitScene } from "./physics/scenes/OrbitScene.ts";

const game = new Game(GameState.MainMenu);

game.register(GameState.MainMenu, new MainMenuScene(game))
    .register(GameState.Playing, new PlayingScene())
    .register(GameState.PhysicsGravity, new GravityScene(game))
    .register(GameState.PhysicsCollision, new CollisionScene(game))
    .register(GameState.PhysicsSpring, new SpringScene(game))
    .register(GameState.PhysicsFluid, new FluidScene(game))
    .register(GameState.PhysicsOrbit, new OrbitScene(game));

export function onStart(): void {
    game.start();
}

export function onUpdate(deltaTime: number): void {
    game.update(deltaTime);
}

export function onFixedUpdate(deltaTime: number): void {
    game.fixedUpdate(deltaTime);
}
