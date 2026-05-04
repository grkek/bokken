import Canvas, { View, Label, Align, Timing, useState } from "bokken/canvas";
import Input from "bokken/input";
import Log from "bokken/log";
import { type Scene, GameState } from "../core/GameState.ts";
import { Game } from "../core/Game.ts";

function MenuButton(props: {
    label: string;
    onClick: () => void;
    accent?: boolean;
    color?: number;
    borderColor?: number;
}): any {
    const bgColor = props.accent
        ? (props.color ?? 0x33FFAA22)
        : 0xFFFFFF0A;
    const brdColor = props.accent
        ? (props.borderColor ?? 0x33FFAA88)
        : 0xFFFFFF22;
    const textColor = props.accent
        ? (props.color ? (props.color | 0xFF) : 0x33FFAAFF)
        : 0xCCCCDDFF;

    return (
        <View
            style={{
                width: 280,
                height: 48,
                backgroundColor: bgColor,
                borderWidth: 1,
                borderColor: brdColor,
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
                fontSize: 16,
                color: textColor,
            }}>
                {props.label}
            </Label>
        </View>
    );
}

function Separator(): any {
    return <View style={{ height: 10 }} />;
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
        // Number keys for quick access
        if (Input.isKeyPressed("Digit1")) this.goTo(GameState.PhysicsGravity);
        if (Input.isKeyPressed("Digit2")) this.goTo(GameState.PhysicsCollision);
        if (Input.isKeyPressed("Digit3")) this.goTo(GameState.PhysicsSpring);
        if (Input.isKeyPressed("Digit4")) this.goTo(GameState.PhysicsFluid);
        if (Input.isKeyPressed("Digit5")) this.goTo(GameState.PhysicsOrbit);
    }

    fixedUpdate(_dt: number): void {}

    private startGame(): void {
        if (this.started) return;
        this.started = true;
        this.game.transition(GameState.Playing);
    }

    private goTo(state: GameState): void {
        if (this.started) return;
        this.started = true;
        this.game.transition(state);
    }

    private render(): void {
        const self = this;

        function Menu(): any {
            return (
                <View style={{
                    width: "100%",
                    height: "100%",
                    backgroundColor: 0x050510FF,
                    justifyContent: Align.Center,
                    alignItems: Align.Center,
                }}>
                    {/* Title */}
                    <Label style={{ fontSize: 14, color: 0x33FFAA55 }}>
                        A Bokken Engine Demo
                    </Label>
                    <View style={{ height: 8 }} />
                    <Label style={{ fontSize: 56, color: 0x33FFAAFF }}>
                        NEON KATANA
                    </Label>
                    <View style={{ height: 4 }} />
                    <Label style={{ fontSize: 16, color: 0xAABBCC88 }}>
                        Fly. Collect. Explore the void.
                    </Label>

                    <View style={{ height: 36 }} />

                    {/* Main game button */}
                    <MenuButton
                        label={"\u25B6  Launch Game"}
                        accent={true}
                        onClick={() => self.startGame()}
                    />

                    <View style={{ height: 28 }} />

                    {/* Physics demos section */}
                    <Label style={{ fontSize: 12, color: 0x8899AAAA }}>
                        {"\u2500\u2500\u2500 PHYSICS SIMULATIONS \u2500\u2500\u2500"}
                    </Label>
                    <View style={{ height: 12 }} />

                    <MenuButton
                        label={"1 \u2B25 Gravity + Bounce"}
                        accent={true}
                        color={0xFF884422}
                        borderColor={0xFF884488}
                        onClick={() => self.goTo(GameState.PhysicsGravity)}
                    />
                    <Separator />
                    <MenuButton
                        label={"2 \u2B25 Rigid Body Collisions"}
                        accent={true}
                        color={0xFF555522}
                        borderColor={0xFF555588}
                        onClick={() => self.goTo(GameState.PhysicsCollision)}
                    />
                    <Separator />
                    <MenuButton
                        label={"3 \u2B25 Springs & Pendulums"}
                        accent={true}
                        color={0xFFAA3322}
                        borderColor={0xFFAA3388}
                        onClick={() => self.goTo(GameState.PhysicsSpring)}
                    />
                    <Separator />
                    <MenuButton
                        label={"4 \u2B25 Fluid Simulation (SPH)"}
                        accent={true}
                        color={0x33CCFF22}
                        borderColor={0x33CCFF88}
                        onClick={() => self.goTo(GameState.PhysicsFluid)}
                    />
                    <Separator />
                    <MenuButton
                        label={"5 \u2B25 Orbital Mechanics"}
                        accent={true}
                        color={0xFFCC4422}
                        borderColor={0xFFCC4488}
                        onClick={() => self.goTo(GameState.PhysicsOrbit)}
                    />

                    <View style={{ height: 36 }} />

                    {/* Controls */}
                    <Label style={{ fontSize: 11, color: 0x444466FF }}>
                        {"WASD / Arrows: fly | Space: boost | 1-5: quick select"}
                    </Label>
                    <View style={{ height: 8 }} />
                    <Label style={{ fontSize: 11, color: 0x444466FF }}>
                        {"In demos: Space: interact | R: reset | Esc: menu"}
                    </Label>
                    <View style={{ height: 16 }} />
                    <Label style={{ fontSize: 10, color: 0x333355FF }}>
                        v3.0.0-physics
                    </Label>
                </View>
            );
        }

        Canvas.render(<Menu />);
    }
}
