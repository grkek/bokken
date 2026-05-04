export enum GameState {
    MainMenu = "MainMenu",
    Playing = "Playing",
    PhysicsGravity = "PhysicsGravity",
    PhysicsCollision = "PhysicsCollision",
    PhysicsSpring = "PhysicsSpring",
    PhysicsFluid = "PhysicsFluid",
    PhysicsOrbit = "PhysicsOrbit",
}

export interface Scene {
    enter(): void;
    update(deltaTime: number): void;
    fixedUpdate(deltaTime: number): void;
}
