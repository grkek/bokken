declare module "bokken/gameObject" {
    /** Primitive shapes for 2D rendering. */
    export enum Shape2D {
        Empty = "Empty",
        Quad = "Quad",
        Circle = "Circle",
        Triangle = "Triangle",
        Line = "Line",
    }

    /** Base interface for all functional units attached to a GameObject. */
    export abstract class Component {
        readonly gameObject: GameObject;
        public enabled: boolean;
    }

    /**
     * Defines the viewport and projection for 2D rendering.
     */
    export class Camera2D extends Component {
        public zoom: number;
        public isActive: boolean;
    }

    /**
     * Defines the particle system for 2D rendering.
     */
    export class ParticleEmitter2D extends Component {
        public emitting: boolean;
        public emitRate: number;
        public lifetimeMinimum: number;
        public lifetimeMaximum: number;
        public speedMinimum: number;
        public speedMaximum: number;
        public sizeStart: number;
        public sizeEnd: number;
        public sizeStartVariance: number;
        public sizeEase: number;
        public spreadAngle: number;
        public direction: number;
        public gravity: number;
        public damping: number;
        public angularVelocityMinimum: number;
        public angularVelocityMaximum: number;
        public spawnOffsetX: number;
        public spawnOffsetY: number;
        public velocityScaleEmission: boolean;
        public velocityReferenceSpeed: number;
        public colorStart: number;
        public colorEnd: number;
        public alphaEase: number;
        public zOrder: number;
        public maximumParticles: number;

        burst(count: number): void;
    }

    /**
     * 2D spatial transform: position, rotation (z-axis), scale, and draw order.
     * Does not carry visual state — attach a Mesh2D for that.
     */
    export class Transform2D extends Component {
        public positionX: number;
        public positionY: number;
        public rotation: number;
        public scaleX: number;
        public scaleY: number;
        public zOrder: number;

        /** Moves the transform by the given offset in a single native call. */
        translate(positionX: number, positionY: number): void;

        /** Rotates by the given degrees (counter-clockwise). */
        rotate(degrees: number): void;
    }

    /**
     * Visual representation of a 2D game object.
     * Separated from Transform2D so invisible objects (triggers, spawners)
     * don't carry render state.
     */
    export class Mesh2D extends Component {
        public shape: Shape2D;
        public color: number;
        public flipX: boolean;
        public flipY: boolean;
    }

    /**
     * 2D rigid-body dynamics in the XY plane.
     * Forces, velocity, and angular velocity are computed in native code.
     */
    export class Rigidbody2D extends Component {
        public mass: number;
        public useGravity: boolean;
        public isStatic: boolean;
        public velocityX: number;
        public velocityY: number;
        public angularVelocity: number;

        /** Applies a force at the center of mass. */
        applyForce(velocityX: number, velocityY: number): void;

        /** Applies angular torque in degrees. */
        applyTorque(degrees: number): void;

        /** Overwrites the current linear velocity. */
        setVelocity(velocityX: number, velocityY: number): void;
    }

    /**
     * Abstract base for JS-authored game scripts.
     * Scripts query their own transform via gameObject.getComponent(Transform2D).
     */
    export abstract class Behaviour extends Component {
        onStart?(): void;
        onUpdate?(deltaTime: number): void;
        onFixedUpdate?(deltaTime: number): void;
        onDestroy?(): void;
    }

    /** Extracts the writable public properties of a component for use as a config object. */
    type ComponentProperties<T> = {
        [K in keyof T as T[K] extends (...args: any[]) => any ? never : K]?: T[K];
    };

    /**
     * The primary container for all entities in the Bokken engine.
     * Dimension-agnostic — 2D or 3D is determined by which components
     * are attached.
     */
    export class GameObject {
        /**
         * Creates a new entity in the native registry.
         * @param name Optional display name (defaults to "Untitled").
         */
        constructor(name?: string);

        /**
         * Attaches a component and returns `this` for chaining.
         * Accepts an optional config object to set properties inline.
         *
         * @example
         * const player = new GameObject("Player")
         *     .addComponent(Transform2D, { positionX: 5, positionY: 3 })
         *     .addComponent(Mesh2D, { shape: Shape2D.Quad, colorR: 1.0 })
         *     .addComponent(Rigidbody2D, { mass: 2, useGravity: true });
         */
        addComponent<T extends Component>(componentClass: new () => T, props?: ComponentProperties<T>): this;

        /**
         * Retrieves an attached component by type.
         * @returns The component instance if found, otherwise undefined.
         */
        getComponent<T extends Component>(componentClass: new () => T): T | undefined;

        /** Sets the parent of this object in the scene hierarchy. */
        setParent(parent: GameObject | null): void;

        /** Returns all immediate child GameObjects. */
        getChildren(): GameObject[];

        /** Flags the entity for destruction at the end of the frame. */
        static destroy(obj: GameObject): void;

        /** Finds the first GameObject with the given name. O(n) — avoid in onUpdate. */
        static find(name: string): GameObject | undefined;
    }
}