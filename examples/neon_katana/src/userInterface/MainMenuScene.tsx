import Canvas, { View, Label, Align } from "bokken/canvas";
import Input from "bokken/input";
import { type Scene, GameState } from "../core/GameState.ts";
import { Game } from "../core/Game.ts";

export class MainMenuScene implements Scene {
    constructor(private readonly game: Game) { }

    enter(): void {
        this.render();
    }

    update(deltaTime: number): void {
        if (Input.isKeyPressed("Enter")) {
            this.game.transition(GameState.Playing);
        }
    }

    fixedUpdate(deltaTime: number): void { }

    private render(): void {
        Canvas.render(
            <View style={{
                width: "100%",
                height: "100%",
                alignItems: Align.Center,
                justifyContent: Align.Center,
                backgroundColor: 0x1A1A2EFF,
            }}>
                <Label style={{ fontSize: 48, color: 0x33CCFFFF }}>
                    NEON KATANA
                </Label>
                <View style={{ height: 24 }} />
                <Label style={{ fontSize: 18, color: 0xAAAAAAFF }}>
                    Press ENTER to start
                </Label>
                <View style={{ height: 48 }} />
                <Label style={{ fontSize: 12, color: 0x666666FF }}>
                    Arrow keys to move | Space to jump
                </Label>
            </View>
        );
    }
}
