/* The alignment of items along the container's main or cross axis. */
export enum Align {
    /* Align items to the beginning of the container */
    Start = "Start",
    /* Align items to the center of the container */
    Center = "Center",
    /* Align items to the end of the container */
    End = "End"
}

/* Mathematical curves for transition animations. */
export enum Timing {
    /* Constant speed from start to end */
    Linear = "Linear",
    /* Starts slow and accelerates at the end */
    EaseIn = "EaseIn",
    /* Starts fast and decelerates at the end */
    EaseOut = "EaseOut",
    /* Slow start and end, fast in the middle */
    EaseInOut = "EaseInOut",
    /* Simulates a physical bouncing effect at the end */
    Bounce = "Bounce",
    /* Slightly overshoots the target before settling */
    Back = "Back",
    /* Instantaneous transition at the end of the duration */
    Step = "Step"
}

/* The direction in which child elements are placed in a container. */
export enum FlexDirection {
    /* Children are placed horizontally from left to right */
    Row = "Row",
    /* Children are placed vertically from top to bottom */
    Column = "Column"
}

/* The positioning strategy used for calculating element coordinates. */
export enum Position {
    /* Positioned according to the normal flow of the layout */
    Relative = "Relative",
    /* Positioned relative to the nearest positioned ancestor */
    Absolute = "Absolute"
}

/* How an element handles content that exceeds its boundaries. */
export enum Overflow {
    /* Content is allowed to render outside the element box */
    Visible = "Visible",
    /* Content outside the element box is clipped and invisible */
    Hidden = "Hidden"
}

/**
 * Simple Style Sheet (SSS) Properties.
 * These properties are passed to the C++ Layout Engine.
 */
export interface SSSProperties {
    /** Layout & Flexbox **/

    /* Direction of child flow: 'row' or 'column' (Default: 'column') */
    flexDirection?: FlexDirection;

    /* How the element grows relative to siblings (0 = static, 1 = fill) */
    flex?: number;

    /* Internal spacing within the element boundary */
    padding?: number | string;

    /* Specific directional paddings */
    paddingTop?: number | string;
    paddingBottom?: number | string;
    paddingLeft?: number | string;
    paddingRight?: number | string;

    /* External spacing around the element boundary */
    margin?: number | string;

    /* Specific directional margins (The fix for your button layout) */
    marginTop?: number | string;
    marginBottom?: number | string;
    marginLeft?: number | string;
    marginRight?: number | string;

    /** Positioning **/

    /* 'relative' (default) or 'absolute' (breaks out of the flex stack) */
    position?: Position;

    /* Used with position: 'absolute' to pin elements to screen edges */
    top?: number | string;
    bottom?: number | string;
    left?: number | string;
    right?: number | string;

    /* Z-index for layering UI elements (depth) */
    zIndex?: number;

    /** Visuals & Borders **/

    /* Background color in 0xRRGGBBAA format */
    backgroundColor?: number;

    /* Default text color for child Label components */
    color?: string | number;

    /* Text size in pixels */
    fontSize?: number;

    /* Fixed width or percentage (e.g., "100%") */
    width?: number | string;

    /* Fixed height or percentage (e.g., "100%") */
    height?: number | string;

    /* Transparency level from 0.0 to 1.0 */
    opacity?: number;

    /* Corner rounding radius in pixels */
    borderRadius?: number;

    /* Border thickness and color */
    borderWidth?: number;
    borderColor?: number;

    /* Allows clipping children that overflow the parent boundary */
    overflow?: Overflow;

    /** Alignment **/

    /* Horizontal alignment of child elements */
    alignItems?: Align;

    /* Vertical distribution of child elements */
    justifyContent?: Align;

    /** Animation & Interaction **/

    /* Animation duration in seconds */
    transitionDuration?: number;

    /* The easing curve used for transitions */
    transitionTiming?: Timing;

    /* Scale multiplier when mouse is over the element */
    hoverScale?: number;

    /* Scale multiplier when the element is clicked */
    activeScale?: number;
}

/**
 * Utility for defining reusable component styles with type-safety.
 */
export namespace StyleSheet {
    type NamedStyles<T> = { [P in keyof T]: SSSProperties };
    /** * Flattens and validates style objects for use in components. 
     * In a production build, this may return an ID (number) to optimize the C++ bridge.
     */
    function create<T extends NamedStyles<T>>(styles: T | NamedStyles<T>): T;
}

/**
 * Shared properties for all visual Canvas components.
 */
export interface BaseProperties {
    /** Inline styles or a StyleSheet reference. */
    style?: SSSProperties | number;
    /** Optional identifier for CSS-like global styling (if implemented). */
    className?: string;
    /** Nested elements or text. */
    children?: any;
}

/**
 * Text display component properties.
 */
export interface LabelProperties extends BaseProperties {
    /** The raw string or number to be rendered by the native font engine. */
    children?: string | number | (string | number)[];
}

/**
 * 
 * View display component properties.
 */
export interface ViewProperties extends BaseProperties {
    /** 
     * Callback triggered by the C++ Input System.
     */
    onClick?: () => void;
}

/** A container component used for layout and grouping. Maps to a native View. */
export function View(properties: ViewProperties): any;
/** A text-only element. Maps to a native Text renderer. */
export function Label(properties: LabelProperties): any;

/**
 * Internal JSX Factory used by the TypeScript compiler to handle <></> fragments.
 */
export function Fragment(props: { children?: any }): any;

/**
 * Core JSX Factory. Transforms JSX syntax into internal Element trees.
 * Invoked automatically by TSC based on your 'jsxFactory' setting.
 */
export function createElement(type: any, props: any, ...children: any[]): any;

/**
 * Hook to manage local state within a functional component.
 * Triggers a re-render of the Canvas tree when the setter is called.
 */
export function useState<T>(initialValue: T): [T, (newValue: T | ((prev: T) => T)) => void];

/**
 * Hook for performing side effects (e.g., subscribing to C++ engine events).
 * @param effect Function to run after render. Return a cleanup function to handle unmounting.
 * @param deps Dependency array to control when the effect re-runs.
 */
export function useEffect(effect: () => void | (() => void), deps?: any[]): void;

/**
 * The entry point for the Canvas system.
 * Mounts the root component and starts the reconciliation loop with the C++ UI Layer.
 */
export function render(element: any): void;

declare global {
    namespace JSX {
        /**
         * Mapping of lowercase tags (e.g., <view />) to their property interfaces.
         */
        interface IntrinsicElements {
            view: BaseProperties;
            label: LabelProperties;
        }
    }
}