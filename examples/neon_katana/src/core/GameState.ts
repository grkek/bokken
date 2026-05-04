export enum GameState {
    MainMenu = "MainMenu",
    Playing = "Playing",
}

export interface Scene {
    enter(): void;
    update(deltaTime: number): void;
    fixedUpdate(deltaTime: number): void;
}
