/** * Retrieves the current width and height of the window's client area in pixels. 
 * Used for calculating UI layout and aspect ratios.
 */
export function getSize(): { width: number, height: number };

/** * Requests the OS to resize the window. 
 * Note: This may trigger a C++ callback to recreate the SwapChain or Framebuffers.
 * @param width The new width in pixels.
 * @param height The new height in pixels.
 */
export function setSize(width: number, height: number): void;

/** * Sets the text displayed in the window's title bar. 
 * Useful for showing the game name, version, or real-time performance metrics (FPS).
 */
export function setTitle(title: string): void;

/** * Returns true if the window is currently in a Borderless or Exclusive Fullscreen state. 
 */
export function isFullscreen(): boolean;

/** * Toggles the window between Windowed and Fullscreen modes. 
 * @param value True for Fullscreen, False for Windowed.
 */
export function setFullscreen(value: boolean): void;

/** * Signals the C++ Engine to begin the shutdown sequence. 
 * This will close the OS window and terminate the application at the end of the frame.
 */
export function close(): void;
