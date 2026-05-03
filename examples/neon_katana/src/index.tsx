import Log from "bokken/log";
import Canvas, { useState, Label, View, Align, Timing, Position, FlexDirection } from "bokken/canvas";

interface SliderProperties {
    value: number;
    onChange: (value: number) => void;
    min?: number;
    max?: number;
    width?: number;
}

function Slider({ value, onChange, min = 0, max = 100, width = 200 }: SliderProperties) {
    const trackHeight = 8;
    const thumbSize = 24;
    const ratio = (value - min) / (max - min);
    const trackFill = Math.round(ratio * width);

    return (
        <View style={{ width, height: thumbSize, justifyContent: Align.Center }}>
            <View style={{
                position: Position.Absolute,
                left: 0, right: 0,
                top: (thumbSize - trackHeight) / 2,
                height: trackHeight,
                backgroundColor: 0x3A3A3AFF
            }} />

            <View style={{
                position: Position.Absolute,
                left: 0,
                top: (thumbSize - trackHeight) / 2,
                width: trackFill,
                height: trackHeight,
                backgroundColor: 0x4A9FFFFF
            }} />

            <View style={{
                position: Position.Absolute,
                left: trackFill - thumbSize / 2,
                top: 0,
                width: thumbSize,
                height: thumbSize,
                backgroundColor: 0xFFFFFFFF,
                hoverScale: 1.15,
                activeScale: 0.9,
                transitionDuration: 0.2,
                transitionTiming: Timing.Bounce
            }} />

            {Array.from({ length: 10 }, (_: unknown, i: number) => {
                const stepValue = min + ((max - min) * (i + 0.5)) / 10;
                return (
                    <View
                        style={{
                            position: Position.Absolute,
                            left: (width / 10) * i,
                            top: 0,
                            width: width / 10,
                            height: thumbSize,
                            backgroundColor: 0x00000000
                        }}
                        onClick={() => onChange(Math.round(stepValue))}
                    />
                );
            })}
        </View>
    );
}

interface RadioProperties {
    options: string[];
    selected: string;
    onChange: (value: string) => void;
}

function Radio({ options, selected, onChange }: RadioProperties) {
    return (
        <View style={{ flexDirection: FlexDirection.Column, width: 200 }}>
            {options.map((opt: string, i: number) => (
                <View
                    style={{
                        width: 200,
                        height: 32,
                        flexDirection: FlexDirection.Row,
                        alignItems: Align.Center,
                        marginBottom: 8,
                        hoverScale: 1.05,
                        transitionDuration: 0.2
                    }}
                    onClick={() => onChange(opt)}
                >
                    <View style={{
                        width: 24,
                        height: 24,
                        borderWidth: 2,
                        borderColor: selected === opt ? 0x4A9FFFFF : 0x888888FF,
                        backgroundColor: 0x00000000,
                        justifyContent: Align.Center,
                        alignItems: Align.Center
                    }}>
                        <View style={{
                            width: 12,
                            height: 12,
                            backgroundColor: selected === opt ? 0x4A9FFFFF : 0x00000000
                        }} />
                    </View>

                    <Label style={{
                        fontSize: 20,
                        color: selected === opt ? 0xFFFFFFFF : 0xAAAAAAFF,
                        marginLeft: 12
                    }}>
                        {opt}
                    </Label>
                </View>
            ))}
        </View>
    );
}

function App() {
    const [count, setCount] = useState(0);
    const [volume, setVolume] = useState(50);
    const [difficulty, setDifficulty] = useState("Normal");

    return (
        <View style={{
            width: "100%",
            height: "100%",
            backgroundColor: 0x00000000,
            justifyContent: Align.Center,
            alignItems: Align.Center
        }}>
            <Label style={{ fontSize: 48, color: 0xFFFFFF55 }}>Hello, World!</Label>

            <Label style={{ fontSize: 48, color: 0xFFFFFFFF, marginBottom: 20 }}>
                {count}
            </Label>

            <View
                style={{
                    width: 64,
                    height: 64,
                    backgroundColor: 0x1C242B55,
                    justifyContent: Align.Center,
                    alignItems: Align.Center,
                    hoverScale: 1.25,
                    activeScale: 0.92,
                    transitionDuration: 0.4,
                    transitionTiming: Timing.Bounce
                }}
                onClick={() => setCount(count + 1)}
            >
                <Label style={{ color: 0xFFFFFFFF, fontSize: 48 }}>+</Label>
            </View>

            <Label style={{ fontSize: 24, color: 0xFFFFFFFF, marginTop: 40, marginBottom: 8 }}>
                Volume: {volume}
            </Label>
            <Slider value={volume} onChange={setVolume} min={0} max={100} width={200} />

            <Label style={{ fontSize: 24, color: 0xFFFFFFFF, marginTop: 32, marginBottom: 12 }}>
                Difficulty
            </Label>
            <Radio
                options={["Easy", "Normal", "Hard"]}
                selected={difficulty}
                onChange={setDifficulty}
            />
        </View>
    );
}

export function onStart() {
    Log.info("Initializing Neon Katana...");
    Canvas.render(<App />);
}

export function onUpdate(deltaTime: number) {

}