declare module "bokken/canvas" {
    /** Alignment of items along the container's main or cross axis. */
    export enum Align {
        Start = "Start",
        Center = "Center",
        End = "End",
    }

    /** Mathematical curves for transition animations. */
    export enum Timing {
        Linear = "Linear",
        EaseIn = "EaseIn",
        EaseOut = "EaseOut",
        EaseInOut = "EaseInOut",
        Bounce = "Bounce",
        Back = "Back",
        Step = "Step",
    }

    /** Direction in which child elements are placed in a container. */
    export enum FlexDirection {
        Row = "Row",
        Column = "Column",
    }

    /** Positioning strategy for calculating element coordinates. */
    export enum Position {
        Relative = "Relative",
        Absolute = "Absolute",
    }

    /** How an element handles content that exceeds its boundaries. */
    export enum Overflow {
        Visible = "Visible",
        Hidden = "Hidden",
    }

    /**
     * Simple Style Sheet (SSS) properties passed to the native layout engine.
     *
     * Colors use 0xRRGGBBAA hex format. Dimensions accept either a number (pixels)
     * or a percentage string ("50%") for width/height only — all other numeric
     * properties are pixel values.
     */
    export interface SSSProperties {
        // Layout & Flexbox
        flexDirection?: FlexDirection;
        flex?: number;

        padding?: number;
        paddingTop?: number;
        paddingBottom?: number;
        paddingLeft?: number;
        paddingRight?: number;

        margin?: number;
        marginTop?: number;
        marginBottom?: number;
        marginLeft?: number;
        marginRight?: number;

        // Positioning
        position?: Position;
        top?: number;
        bottom?: number;
        left?: number;
        right?: number;
        zIndex?: number;

        // Sizing & Fonts
        width?: number | string;
        height?: number | string;
        font?: string;
        fontSize?: number;

        // Visuals & Borders
        backgroundColor?: number;
        color?: number;
        opacity?: number;
        borderRadius?: number;
        borderWidth?: number;
        borderColor?: number;
        overflow?: Overflow;

        // Alignment
        alignItems?: Align;
        justifyContent?: Align;

        // Animation & Interaction
        transitionDuration?: number;
        transitionTiming?: Timing;
        hoverScale?: number;
        activeScale?: number;
    }

    /** Shared properties for all visual Canvas components. */
    export interface BaseProperties {
        style?: SSSProperties;
        children?: any;
    }

    /** Properties for the Label (text display) component. */
    export interface LabelProperties extends BaseProperties {
        children?: string | number | (string | number)[];
    }

    /** Properties for the View (container) component. */
    export interface ViewProperties extends BaseProperties {
        onClick?: () => void;
    }

    /** String tag for View components, used by createElement. */
    export const View: string;

    /** String tag for Label components, used by createElement. */
    export const Label: string;

    /**
     * Hook to manage local state within a functional component.
     * Triggers a re-render of the Canvas tree when the setter is called.
     */
    export function useState<T>(initialValue: T): [T, (newValue: T | ((prev: T) => T)) => void];

    /** The default export contains the JSX factory and render entry point. */
    interface CanvasDefault {
        /**
         * JSX factory. Transforms JSX syntax into internal element trees.
         * Set as the jsxFactory in your tsconfig.
         */
        createElement(type: any, props: any, ...children: any[]): any;

        /**
         * Mounts the root element and starts the reconciliation loop
         * with the native UI layer.
         */
        render(element: any): void;
    }

    const canvas: CanvasDefault;
    export default canvas;

    global {
        namespace JSX {
            interface IntrinsicElements {
                view: ViewProperties;
                label: LabelProperties;
            }
        }
    }
}