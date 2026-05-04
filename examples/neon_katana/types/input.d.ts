declare module "bokken/input" {
    interface Vec2 {
        readonly x: number;
        readonly y: number;
    }

    interface Input {
        /**
         * Returns true while the key is held down.
         * @param key Key name — web-standard ("KeyW", "ArrowUp", "Space", "ShiftLeft")
         *            or SDL-native ("W", "Up", "Space", "F1").
         */
        isKeyDown(key: string): boolean;

        /** Returns true only on the frame the key was pressed. */
        isKeyPressed(key: string): boolean;

        /** Returns true only on the frame the key was released. */
        isKeyReleased(key: string): boolean;

        /** Returns the current mouse position in window coordinates. */
        getMousePosition(): Vec2;

        /**
         * Returns true while the mouse button is held.
         * @param button 0 = left, 1 = middle, 2 = right.
         */
        isMouseDown(button: number): boolean;

        /** Returns true only on the frame the button was pressed. */
        isMousePressed(button: number): boolean;

        /** Returns true only on the frame the button was released. */
        isMouseReleased(button: number): boolean;

        /** Returns the scroll wheel delta for this frame. */
        getScrollDelta(): Vec2;
    }

    const input: Input;
    export default input;
}