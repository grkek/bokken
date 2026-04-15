import Log from "bokken/log";
import Canvas, { useState, Label, View, Align } from "bokken/canvas";

function App() {
    const [count, setCount] = useState(0);

    return (
        <View style={{ 
            justifyContent: Align.center, 
            alignItems: Align.center,
            backgroundColor: 0x222222FF,
            width: "100%",
            height: "100%"
        }}>
            {/* Using the 'text' prop is often more reliable for native bridges */}
            <Label style={{ fontSize: 60, color: 0xFFFFFFFF, margin: 20 }}>Count: {count}</Label>
            
            <View 
                style={{ 
                    width: 300,
                    height: 100,
                    backgroundColor: 0x00AAFFFF,
                    margin: 20,
                    justifyContent: Align.center,
                    alignItems: Align.center,
                }}
                onClick={() => {
                    Log.info("Button Clicked!");
                    setCount(count + 1);
                }}
            >
                <Label style={{ color: 0xFFFFFFFF, fontSize: 30 }}>Click Me!</Label>
            </View>
        </View>
    );
}

export function onStart() {
    Log.info("Canvas App Starting...");
    Canvas.render(<App />);
}

export function onUpdate(deltaTime: number) { }