import Log from "bokken/log";
import Window from "bokken/window";

export function onStart() {
    // Called once when the game starts
    Log.info("Hello, World!");

    const {width, height} = Window.getSize();
}

export function onUpdate(deltaTime: number) {
    // Called every frame, deltaTime is the time in seconds since the last update
}

export function onFixedUpdate(deltaTime: number) {
    // Called at a fixed interval, deltaTime is the time in seconds since the last fixed update
}


