/**
 * The base interface for all functional units attached to a GameObject.
 * Data for Native components resides in C++ memory, while Behaviours live in the JS VM.
 */
export abstract class Component {
    /** The GameObject instance this component is currently attached to. */
    readonly gameObject: GameObject;

    /** * Toggles the component's execution. 
     * When false, C++ systems (Physics/Renderer) and JS lifecycle hooks will skip this component. 
     */
    public enabled: boolean;
}

/**
 * Mandatory Native Component that defines the object's presence in 3D space.
 * Every GameObject is born with a Transform. Modifications are mirrored instantly 
 * in the C++ Scene Graph for rendering and spatial queries.
 */
export class Transform extends Component {
    /** Position on the X, Y, and Z axes in world/local space. */
    public x: number;
    public y: number;
    public z: number;

    /** Euler rotation angles in degrees for each axis. */
    public rotationX: number;
    public rotationY: number;
    public rotationZ: number;

    /** Scale multipliers for each axis. (1.0 is original size). */
    public scaleX: number;
    public scaleY: number;
    public scaleZ: number;

    /** * Moves the transform by a given offset. 
     * This is faster than setting x/y/z individually as it performs a single C++ vector addition.
     */
    translate(x: number, y: number, z: number): void;

    /** * Rotates the transform by the specified Euler angles. 
     */
    rotate(x: number, y: number, z: number): void;
}

/**
 * The entry point for custom game logic. 
 * Instances are tracked by the C++ Registry and invoked during the engine's logical update pass.
 */
export abstract class Behaviour extends Component {
    /** Shortcut to the GameObject's Transform component. */
    readonly transform: Transform;

    /** Invoked by the C++ Engine once when the script is first enabled or instantiated. */
    onStart?(): void;

    /** Invoked by the C++ Engine every frame. Use for input and non-physics logic. */
    onUpdate?(deltaTime: number): void;

    /** * Invoked at a consistent, fixed time interval. 
     * Primary location for physics calculations and Rigidbody manipulations. 
     */
    onFixedUpdate?(deltaTime: number): void;

    /** Invoked by the C++ Engine immediately before the object or component is destroyed. */
    onDestroy?(): void;
}

/** * Native Physics Component linking the entity to the C++ Physics World (e.g., Box2D/Bullet).
 * Forces and velocities are computed in native code for maximum performance.
 */
export class Rigidbody extends Component {
    /** The mass of the object. Higher mass requires more force to move. */
    public mass: number;

    /** If true, the C++ physics solver will apply a global gravity force to this body. */
    public useGravity: boolean;

    /** If true, the object is immovable (e.g., a floor) but can still collide with other bodies. */
    public isStatic: boolean;

    /** * Pushes the object in C++ memory. 
     * Force is applied at the center of mass unless otherwise specified by the native implementation.
     */
    applyForce(x: number, y: number, z: number): void;

    /** Overwrites the current linear velocity in the C++ physics state. */
    setVelocity(x: number, y: number, z: number): void;
}

/**
 * The primary container for all entities in the Bokken Engine.
 * A GameObject is essentially a 'Handle' to a C++ Entity ID. 
 * It manages the lifecycle of both Native components and Scripted Behaviours.
 */
export class GameObject {
    /** Unique internal ID used to communicate with the C++ Entity Registry. */
    private readonly nativeHandle: number;

    /** The display name used for debugging and searching within the C++ Registry. */
    public name: string;

    /** * Shortcut to the Transform component. 
     * Guaranteed to be present on every GameObject. 
     */
    readonly transform: Transform;

    /** * Creates a new entity within the C++ Registry.
     * @param name Optional name for the entity (defaults to "New GameObject").
     */
    constructor(name?: string);

    /**
     * Instantiates and attaches a Component or Behaviour to this entity.
     * @param componentClass The class constructor (e.g., Rigidbody or a custom Behaviour).
     * @returns The newly created component instance.
     */
    addComponent<T extends Component>(componentClass: new () => T): T;

    /** * Searches the entity for a component of the specified type.
     * @returns The component instance if found, otherwise undefined.
     */
    getComponent<T extends Component>(componentClass: new () => T): T | undefined;

    /** * Helper to set the parent of this object's transform.
     * Behind the scenes, C++ will update the Scene Graph.
     */
    setParent(parent: GameObject | null): void;

    /** Returns all immediate child GameObjects. */
    getChildren(): GameObject[];

    /** * Flags the entity for removal in the C++ Registry. 
     * This will destroy all attached components and free the native memory at the end of the frame.
     */
    static destroy(obj: GameObject): void;

    /** * Iterates through the C++ Registry to find the first GameObject with the matching name. 
     * Note: This is an O(n) operation; avoid calling it inside onUpdate.
     */
    static find(name: string): GameObject | undefined;
}