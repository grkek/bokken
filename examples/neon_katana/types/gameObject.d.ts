declare module "bokken/gameObject" {
    /** Primitive shapes for 2D rendering. */
    export enum Shape2D {
        Empty = "Empty",
        Quad = "Quad",
        Circle = "Circle",
        Triangle = "Triangle",
        Line = "Line",
    }

    /** Blend mode for quads, sprites, and particles. */
    export enum BlendMode {
        /** Standard porter-duff over (src * srcA + dst * (1-srcA)). */
        Alpha = "alpha",
        /** Brightens what's behind (src * srcA + dst). Fire, explosions, magic. */
        Additive = "additive",
        /** Subtler than additive (1 - (1-src)*(1-dst)). Soft glows. */
        Screen = "screen",
    }

    /** Loop behaviour for animation clips. */
    export enum AnimationLoopMode {
        None = "None",
        Loop = "Loop",
        PingPong = "PingPong"
    }

    /**
     * Describes a grid region of a sprite sheet for automatic frame slicing.
     * Passed as the `frames` field of an animation clip when you want the
     * engine to carve the frames out of the Sprite2D's texture automatically.
     */
    export interface AnimationFrameGrid {
        /** Width of each frame in pixels. */
        frameWidth: number;
        /** Height of each frame in pixels. */
        frameHeight: number;
        /** Number of frames to extract. 0 or omitted fills the available space. */
        count?: number;
        /** Pixel X offset from the left edge of the texture. */
        offsetX?: number;
        /** Pixel Y offset from the top edge of the texture. */
        offsetY?: number;
        /** Horizontal gap between frames in pixels. */
        paddingX?: number;
        /** Vertical gap between frames in pixels. */
        paddingY?: number;
        /** Optional per-clip texture path. Overrides the Sprite2D's texturePath. */
        texturePath?: string;
    }

    /**
     * Defines a single named animation clip.
     *
     * `frames` accepts either:
     *   - An array of TextureCache region name strings (explicit control).
     *   - An AnimationFrameGrid object that auto-slices the sibling
     *     Sprite2D's texture into a uniform grid.
     */
    export interface AnimationClipDefinition {
        /** Unique clip name (e.g. "idle", "run", "jump"). */
        name: string;
        /** Frame source — explicit region names or auto-slice grid. */
        frames: string[] | AnimationFrameGrid;
        /** Playback speed in frames per second. Defaults to 12. */
        fps?: number;
        /** Loop behaviour. Defaults to "Loop". */
        loop?: AnimationLoopMode;
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
     * CPU-side 2D particle emitter.
     *
     * Spawns particles at the owning Transform2D's position, simulates
     * them each frame, and submits colored quads to the SpriteBatcher.
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
        /** Blend mode for all particles in this emitter. */
        public blendMode: BlendMode;

        burst(count: number): void;
    }

    /**
     * 2D spatial transform: position, rotation (z-axis), scale, and draw order.
     * Does not carry visual state — attach a Mesh2D or Sprite2D for that.
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
     * Visual representation of a 2D game object using solid-color primitives.
     * Separated from Transform2D so invisible objects (triggers, spawners)
     * don't carry render state.
     *
     * For textured rendering, use Sprite2D instead. When both Sprite2D and
     * Mesh2D exist on the same GameObject, Sprite2D takes priority.
     */
    export class Mesh2D extends Component {
        public shape: Shape2D;
        public color: number;
        public flipX: boolean;
        public flipY: boolean;
    }

    /**
     * Visual representation using a texture or texture atlas region.
     *
     * Draws a textured quad sourced from the TextureCache. The texture is
     * loaded lazily on first use — just set `texturePath` to a virtual path
     * inside the asset pack and the engine handles the rest.
     *
     * When used with Animation2D, the animation controller writes into
     * `regionName` each frame to drive sprite sheet playback.
     *
     * Takes rendering priority over Mesh2D when both are present.
     */
    export class Sprite2D extends Component {
        /** Path to the texture in the asset pack VFS (e.g. "/textures/player.png"). */
        public texturePath: string;
        /** Named region within the texture. Empty string uses the full texture. */
        public regionName: string;
        /** Tint colour multiplied with the texture sample. Packed as 0xRRGGBBAA. */
        public tint: number;
        /** Overall opacity, multiplied into tint alpha at draw time. */
        public opacity: number;
        public flipX: boolean;
        public flipY: boolean;
        /** Override draw width in pixels. 0 uses the source region's width. */
        public overrideWidth: number;
        /** Override draw height in pixels. 0 uses the source region's height. */
        public overrideHeight: number;
        /** Horizontal anchor in [0,1]. 0.5 = center (default). */
        public anchorX: number;
        /** Vertical anchor in [0,1]. 0.5 = center (default). */
        public anchorY: number;
        /** Blend mode for this sprite. */
        public blendMode: BlendMode;
    }

    /**
     * Sprite sheet animation controller.
     *
     * Holds a dictionary of named clips and drives the active clip's frame
     * counter. Each tick it writes the current frame's region name into the
     * sibling Sprite2D, so the renderer picks it up automatically.
     *
     * Requires a Sprite2D on the same GameObject.
     */
    export class Animation2D extends Component {
        /** Whether the current clip is actively advancing frames. */
        readonly isPlaying: boolean;
        /** The name of the currently active clip. */
        readonly activeClip: string;
        /** The current frame index within the active clip. */
        readonly frameIndex: number;
        /** The TextureCache region name of the current frame. */
        readonly currentRegion: string;

        /**
         * Add or replace an animation clip.
         *
         * @example
         * // Auto-slice from the Sprite2D's texture.
         * animation.addClip({
         *     name: "run",
         *     frames: { frameWidth: 32, frameHeight: 32, count: 6, offsetY: 64 },
         *     fps: 12,
         *     loop: "Loop"
         * });
         *
         * @example
         * // Auto-slice from a different texture per clip.
         * animation.addClip({
         *     name: "idle",
         *     frames: { texturePath: "/textures/robot-idle.png", frameWidth: 48, frameHeight: 48, count: 8 },
         *     fps: 8
         * });
         *
         * @example
         * // Explicit regions: hand-picked frames from the TextureCache.
         * animation.addClip({
         *     name: "special",
         *     frames: ["attack_0", "attack_1", "attack_2"],
         *     fps: 8,
         *     loop: "None"
         * });
         */
        addClip(clip: AnimationClipDefinition): this;

        /** Start playing a clip from the beginning. */
        play(clipName: string): void;
        /** Pause on the current frame. */
        pause(): void;
        /** Resume from where it was paused. */
        resume(): void;
        /** Stop and reset to the first frame. */
        stop(): void;
    }

    /**
     * Screen-space distortion effect attached to a game object.
     *
     * Reads the world position from the sibling Transform2D, converts to
     * normalised screen coordinates using the active camera, and pushes
     * shockwave effects into the DistortionStage in the render pipeline.
     *
     * Requires a Transform2D on the same GameObject and a "distortion"
     * stage in the pipeline. If no distortion stage exists, trigger()
     * silently does nothing.
     *
     * @example
     * // One-shot explosion shockwave — fires on attach.
     * explosion
     *     .addComponent(Transform2D, { positionX: 3, positionY: 1 })
     *     .addComponent(Distortion2D, { autoStart: true, amplitude: 0.08 });
     *
     * @example
     * // Manual trigger on an existing object.
     * const fx = obj.getComponent(Distortion2D)!;
     * fx.amplitude = 0.1;
     * fx.trigger();
     */
    export class Distortion2D extends Component {
        /** Expansion rate of the shockwave ring in UV/sec. */
        public speed: number;
        /** Width of the distortion band. */
        public thickness: number;
        /** Peak displacement strength. */
        public amplitude: number;
        /** Auto-remove threshold. Wave is discarded when radius exceeds this. */
        public maxRadius: number;
        /** If true, a shockwave fires automatically when the component is attached. */
        public autoStart: boolean;

        /**
         * Fire a shockwave from the current world position.
         * Each call creates an independent wave — safe to call multiple times.
         */
        trigger(): void;
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

    /** Options for the GameObject constructor. */
    export interface GameObjectProperties {
        /**
         * When true, the engine automatically destroys this object once
         * every component reports idle (e.g. particles expired, animation
         * finished, shockwave fired). Designed for fire-and-forget objects
         * like explosions, laser segments, and one-shot effects.
         */
        destroyWhenIdle?: boolean;
    }

    /**
     * The primary container for all entities in the Bokken engine.
     * Dimension-agnostic — 2D or 3D is determined by which components
     * are attached.
     */
    export class GameObject {
        /**
         * Creates a new entity in the native registry.
         * @param name Optional display name (defaults to "Untitled").
         * @param properties Optional metadata for lifecycle control.
         *
         * @example
         * // Persistent object — lives until manually destroyed.
         * const player = new GameObject("Player");
         *
         * @example
         * // Fire-and-forget — auto-destroyed when all components go idle.
         * const explosion = new GameObject("Explosion", { destroyWhenIdle: true });
         */
        constructor(name?: string, properties?: GameObjectProperties);

        /**
         * When true, the engine automatically destroys this object once
         * every component reports idle. Can be set after construction.
         */
        public destroyWhenIdle: boolean;

        /**
         * Attaches a component and returns `this` for chaining.
         * Accepts an optional config object to set properties inline.
         *
         * @example
         * const player = new GameObject("Player")
         *     .addComponent(Transform2D, { positionX: 5, positionY: 3 })
         *     .addComponent(Sprite2D, { texturePath: "/textures/knight.png" })
         *     .addComponent(Animation2D);
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

        /** Finds the first GameObject with the given name. O(1) via hash map. */
        static find(name: string): GameObject | undefined;
    }
}