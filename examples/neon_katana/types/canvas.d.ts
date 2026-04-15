export enum Align {
    'start',
    'center',
    'end'
}

/**
 * Simple Style Sheet (SSS) Properties.
 * These properties are passed to the C++ Layout Engine.
 */
export interface SSSProperties {
    /**
     * Determines how child elements are aligned along the cross-axis (horizontal).
     * For example, 'center' will horizontally center all children within this container.
     */
    alignItems?: Align;
    /** 
     * Determines how child elements are distributed along the main-axis (vertical).
     * For example, 'center' will vertically center the entire stack of children.
     */
    justifyContent?: Align;
    /** Internal spacing within the element boundary. */
    padding?: number | string;
    /** External spacing around the element boundary. */
    margin?: number | string;
    /** Hex, RGB, or named color for the quad background. */
    backgroundColor?: numberw;
    /** Default text color for child Label components. */
    color?: string | number;
    /** Text size in pixels (handled by the C++ Font Atlas). */
    fontSize?: number;
    /** Fixed width or percentage of the parent container. */
    width?: number | string;
    /** Fixed height or percentage of the parent container. */
    height?: number | string;
    /** Transparency level from 0.0 (invisible) to 1.0 (opaque). */
    opacity?: number;
    /** Corner rounding radius for the C++ fragment shader. */
    borderRadius?: number;
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
            button: ButtonProperties;
            label: LabelProperties;
        }
    }
}