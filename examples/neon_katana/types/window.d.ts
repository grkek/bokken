declare module "bokken/window" {
    interface Window {
        /** Returns the current window client area dimensions in pixels. */
        getSize(): { width: number; height: number };

        /** Sets the text displayed in the window's title bar. */
        setTitle(title: string): void;
    }

    const window: Window;
    export default window;
}