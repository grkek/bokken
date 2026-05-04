import { GameState } from "./core/GameState.ts";
import { Game } from "./core/Game.ts";
import { MainMenuScene } from "./userInterface/MainMenuScene.tsx";
import { PlayingScene } from "./userInterface/PlayingScene.ts";

const game = new Game(GameState.MainMenu);

game.register(GameState.MainMenu, new MainMenuScene(game))
    .register(GameState.Playing, new PlayingScene());

export function onStart(): void {
    game.start();
}

export function onUpdate(deltaTime: number): void {
    game.update(deltaTime);
}

export function onFixedUpdate(deltaTime: number): void {
    game.fixedUpdate(deltaTime);
}
