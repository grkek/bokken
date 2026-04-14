/**
 * Represents a collection of GameObjects and their hierarchical relationships.
 * Only one Scene can be "active" in the C++ Renderer at a time.
 */
export class Scene {
    /** The unique name of the scene (e.g., "MainMenu" or "Level_01"). */
    public readonly name: string;

    /** * The root GameObject of the scene. 
     * All top-level GameObjects are technically children of this root.
     */
    public readonly root: GameObject;

    /**
     * Creates a new, empty Scene in C++ memory.
     * @param name The identifier for this scene.
     */
    constructor(name: string);

    /**
     * Serializes all GameObjects and Components into a binary format.
     * Used by the C++ Engine to create save files or scene assets.
     */
    save(): ArrayBuffer;

    /**
     * Iterates through all GameObjects in the scene and invokes a callback.
     * @param callback The function to execute for every object found.
     */
    forEach(callback: (object: GameObject) => void): void;
}

/**
 * Global Scene Management system.
 * Handles the transition between different game states and levels.
 */
export namespace SceneManager {
    /**
     * Loads a scene by its registered name or asset path.
     * This will trigger an 'onDestroy' lifecycle event for all objects in the current scene.
     */
    function loadScene(name: string): void;

    /**
     * Returns the currently active Scene being processed by the C++ Engine.
     */
    function getActiveScene(): Scene;

    /**
     * Creates an empty scene and sets it as the active one.
     */
    function createEmpty(name: string): Scene;

    /**
     * Prevents a specific GameObject from being destroyed when changing scenes.
     * Essential for persistent objects like Global Managers or Player State.
     */
    function dontDestroyOnLoad(obj: GameObject): void;
}