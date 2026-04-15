import Canvas, { useState, Label, View, Align, Timing } from "bokken/canvas";

function App() {
    const [count, setCount] = useState(0);

    return (
        <View style={{
            width: "100%",
            height: "100%",
            backgroundColor: 0x00000000,
            justifyContent: Align.Center,
            alignItems: Align.Center
        }}>
            {/* Display Count */}
            <Label style={{ fontSize: 48, color: 0xFFFFFFFF, marginBottom: 20 }}>
                {count}
            </Label>

            {/* Animated Button */}
            <View 
                style={{
                    width: 64,
                    height: 64,
                    backgroundColor: 0x1C242BFF,
                    justifyContent: Align.Center,
                    alignItems: Align.Center,
                    hoverScale: 1.05,
                    activeScale: 0.92,
                    transitionDuration: 0.2,
                    transitionTiming: Timing.EaseOut
                }}
                onClick={() => setCount(count + 1)}
            >
                <Label style={{ color: 0xFFFFFFFF, fontSize: 48 }}>+</Label>
            </View>
        </View>
    );
}

export function onStart() {
    Canvas.render(<App />);
}