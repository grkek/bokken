/**
 * Records highly detailed diagnostic information.
 * Typically suppressed in 'Release' builds to maintain performance.
 * @param message The primary log message.
 * @param args Optional metadata or objects to be serialized and inspected.
 */
export function debug(message: string, ...args: any[]): void;

/**
 * Records general application flow and milestone events (e.g., "Scene Loaded").
 * The standard level for tracking engine state during normal execution.
 */
export function info(message: string, ...args: any[]): void;

/**
 * Records non-critical issues that don't stop the engine but require attention.
 * Highlighted in yellow in the C++ debugger/IDE console.
 */
export function warning(message: string, ...args: any[]): void;

/**
 * Records critical failures, exceptions, or invalid engine states.
 * Highlighted in red and typically includes a JavaScript stack trace in the native output.
 */
export function error(message: string, ...args: any[]): void;
