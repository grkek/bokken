import Canvas, { View, Label, Align, Timing, useState } from "bokken/canvas";
import Input from "bokken/input";
import Log from "bokken/log";
import { type Scene, GameState } from "../core/GameState.ts";
import { Game } from "../core/Game.ts";

function MenuButton(props: {
    label: string;
    onClick: () => void;
    accent?: boolean;
}): any {
    return (
        <View
            style={{
                width: 220,
                height: 48,
                backgroundColor: props.accent ? 0x33CCFF22 : 0xFFFFFF0A,
                borderWidth: 1,
                borderColor: props.accent ? 0x33CCFF88 : 0xFFFFFF22,
                borderRadius: 6,
                justifyContent: Align.Center,
                alignItems: Align.Center,
                hoverScale: 1.05,
                activeScale: 0.95,
                transitionDuration: 0.15,
                transitionTiming: Timing.EaseOut,
            }}
            onClick={props.onClick}
        >
            <Label style={{
                fontSize: 18,
                color: props.accent ? 0x33CCFFFF : 0xCCCCDDFF,
            }}>
                {props.label}
            </Label>
        </View>
    );
}

export class MainMenuScene implements Scene {
    private started = false;

    constructor(private readonly game: Game) {}

    enter(): void {
        this.started = false;
        this.render();
    }

    update(dt: number): void {
        if (Input.isKeyPressed("Enter")) {
            this.startGame();
        }
    }

    fixedUpdate(_dt: number): void {}

    private startGame(): void {
        if (this.started) return;
        this.started = true;
        this.game.transition(GameState.Playing);
    }

    private render(): void {
        const self = this;

        function Menu(): any {
            return (
                <View style={{
                    width: "100%",
                    height: "100%",
                    backgroundColor: 0x0D0D1AFF,
                    justifyContent: Align.Center,
                    alignItems: Align.Center,
                }}>
                    {/* Title */}
                    <Label style={{ fontSize: 14, color: 0x33CCFF55 }}>
                        A Bokken Engine Demo
                    </Label>
                    <View style={{ height: 8 }} />
                    <Label style={{ fontSize: 56, color: 0x33CCFFFF }}>
                        NEON KATANA
                    </Label>

                    <View style={{ height: 64 }} />

                    {/* Buttons */}
                    <MenuButton
                        label="Start Game"
                        accent={true}
                        onClick={() => self.startGame()}
                    />
                    <View style={{ height: 12 }} />
                    <MenuButton
                        label="Controls"
                        onClick={() => Log.info("Movement: Arrow keys / WASD | Jump: Space")}
                    />
                    <View style={{ height: 12 }} />
                    <MenuButton
                        label="Quit"
                        onClick={() => Log.info("Thanks for playing!")}
                    />

                    <View style={{ height: 80 }} />

                    {/* Hints */}
                    <Label style={{ fontSize: 11, color: 0x444466FF }}>
                        Arrow keys / WASD to move | Space to jump
                    </Label>
                    <View style={{ height: 16 }} />
                    <Label style={{ fontSize: 10, color: 0x333355FF }}>
                        v1.0.0-alpha
                    </Label>
                </View>
            );
        }

        Canvas.render(<Menu />);
    }
}