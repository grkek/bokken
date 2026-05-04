import { GameState, type Scene } from "./GameState.ts";

export class Game {
    private scenes = new Map<GameState, Scene>();
    private current: GameState;

    constructor(initialState: GameState) {
        this.current = initialState;
    }

    register(state: GameState, scene: Scene): this {
        this.scenes.set(state, scene);
        return this;
    }

    start(): void {
        this.scenes.get(this.current)?.enter();
    }

    transition(to: GameState): void {
        this.current = to;
        this.scenes.get(this.current)?.enter();
    }

    get state(): GameState {
        return this.current;
    }

    update(deltaTime: number): void {
        this.scenes.get(this.current)?.update(deltaTime);
    }

    fixedUpdate(deltaTime: number): void {
        this.scenes.get(this.current)?.fixedUpdate(deltaTime);
    }
}
