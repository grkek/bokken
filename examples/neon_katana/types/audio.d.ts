import { Component } from "./gameObject";

/**
 * Represents a live audio signal flowing through the effect chain.
 * This is an opaque handle — the actual PCM buffer lives in C++.
 * You never construct a Signal directly; it is provided by the engine.
 */
export interface Signal {
    /** Sample rate of this signal in Hz (e.g. 44100, 48000) */
    readonly sampleRate: number;

    /** Number of audio channels (1 = mono, 2 = stereo) */
    readonly channels: number;

    /** Number of frames in this buffer (samples per channel) */
    readonly frameCount: number;

    /** PCM sample format */
    readonly format: 'f32' | 'i16';
}

/**
 * Base class for all audio effects.
 * An effect is a pure signal transformation: Signal in → Signal out.
 * Effects are applied in the order they are added to a Channel.
 *
 * Implement process() to define your transformation. When the effect
 * is bypassed or disabled, process() is not called and the signal
 * passes through untouched.
 *
 * @example
 * class MyEffect extends AudioEffect {
 *     process(signal: Signal): Signal {
 *         // transform and return
 *         return signal;
 *     }
 * }
 */
export abstract class AudioEffect {
    /** Whether this effect is currently active */
    enabled: boolean;

    /**
     * Transforms the incoming signal and returns the result.
     * This is the only method you need to implement in a custom effect.
     */
    abstract process(signal: Signal): Signal;

    /**
     * Bypasses this effect without removing it from the chain.
     * The signal will pass through unmodified until enable() is called.
     * @returns this (for method chaining)
     */
    bypass(): this;

    /**
     * Re-enables this effect after bypassing.
     * @returns this (for method chaining)
     */
    enable(): this;
}

/**
 * Scales the signal volume by a fixed gain factor.
 * Implicitly added as the first node in every Channel and driven by
 * Channel.setVolume() and Channel.setMuted(). You can also add it
 * explicitly for sub-mixing within a chain.
 *
 * @example
 * channel.addEffect(new GainEffect({ gain: 0.5 }));
 */
export class GainEffect extends AudioEffect {
    /** Gain multiplier (0.0 = silence, 1.0 = unity, >1.0 = amplify) */
    gain: number;

    constructor(options?: { gain?: number });
    process(signal: Signal): Signal;
}

/**
 * Attenuates frequencies above the cutoff, letting low frequencies pass.
 * Useful for muffling sounds behind walls, underwater effects, or distance simulation.
 *
 * @example
 * channel.addEffect(new LowPassEffect({ cutoff: 800 }));
 */
export class LowPassEffect extends AudioEffect {
    /** Cutoff frequency in Hz — frequencies above this are attenuated */
    cutoff: number;

    /** Filter resonance / Q factor (default: 0.707 — Butterworth, no resonance peak) */
    resonance: number;

    constructor(options?: { cutoff?: number; resonance?: number });
    process(signal: Signal): Signal;
}

/**
 * Attenuates frequencies below the cutoff, letting high frequencies pass.
 * Useful for radio effects, thinning out rumble, or telephone-style audio.
 *
 * @example
 * channel.addEffect(new HighPassEffect({ cutoff: 3000 }));
 */
export class HighPassEffect extends AudioEffect {
    /** Cutoff frequency in Hz — frequencies below this are attenuated */
    cutoff: number;

    /** Filter resonance / Q factor (default: 0.707 — Butterworth, no resonance peak) */
    resonance: number;

    constructor(options?: { cutoff?: number; resonance?: number });
    process(signal: Signal): Signal;
}

/**
 * Simulates acoustic space by adding a reverb tail to the signal.
 * Blend dry and wet signal using the `wet` parameter.
 *
 * @example
 * channel.addEffect(new ReverbEffect({ decay: 2.5, wet: 0.4 }));
 */
export class ReverbEffect extends AudioEffect {
    /** Reverb decay time in seconds (how long the tail rings out) */
    decay: number;

    /** Wet/dry mix — 0.0 is fully dry, 1.0 is fully wet */
    wet: number;

    constructor(options?: { decay?: number; wet?: number });
    process(signal: Signal): Signal;
}

/**
 * Repeats the signal after a given time, with optional feedback for
 * multiple decaying echoes.
 *
 * @example
 * channel.addEffect(new DelayEffect({ time: 0.3, feedback: 0.4, wet: 0.35 }));
 */
export class DelayEffect extends AudioEffect {
    /** Delay time in seconds between the original signal and the echo */
    time: number;

    /** How much of the delayed signal feeds back into itself (0.0–1.0) */
    feedback: number;

    /** Wet/dry mix — 0.0 is fully dry, 1.0 is fully wet */
    wet: number;

    constructor(options?: { time?: number; feedback?: number; wet?: number });
    process(signal: Signal): Signal;
}

/**
 * Reduces dynamic range by attenuating signals that exceed the threshold.
 * Useful for preventing clipping, evening out loud/quiet sounds, or
 * sidechain ducking (e.g. music ducking under voiceover).
 *
 * @example
 * channel.addEffect(new CompressorEffect({ threshold: -12, ratio: 4 }));
 */
export class CompressorEffect extends AudioEffect {
    /** Level above which compression is applied, in dBFS (e.g. -12.0) */
    threshold: number;

    /** Compression ratio — 4 means 4dB in → 1dB out above threshold */
    ratio: number;

    /** Time in seconds for the compressor to engage after threshold is crossed */
    attack: number;

    /** Time in seconds for the compressor to disengage after signal drops below threshold */
    release: number;

    constructor(options?: {
        threshold?: number;
        ratio?: number;
        attack?: number;
        release?: number;
    });
    process(signal: Signal): Signal;
}

/**
 * Adds harmonic saturation and clipping to the signal.
 * Useful for weapon impacts, aggressive UI sounds, or stylized retro effects.
 *
 * @example
 * channel.addEffect(new DistortionEffect({ drive: 0.7, tone: 0.5 }));
 */
export class DistortionEffect extends AudioEffect {
    /** Amount of gain applied before clipping (0.0–1.0) */
    drive: number;

    /** Tonal balance of the distorted signal — 0.0 is darker, 1.0 is brighter */
    tone: number;

    constructor(options?: { drive?: number; tone?: number });
    process(signal: Signal): Signal;
}

/**
 * Configuration options when creating a new Channel.
 */
export interface ChannelOptions {
    /** Initial volume of the channel (0.0 = silent, 1.0 = full volume). Default: 1.0 */
    volume?: number;

    /** Whether the channel starts muted. Default: false */
    muted?: boolean;
}

/**
 * A single audio bus. Sounds routed into this channel pass through
 * its effect chain in order before reaching the master output.
 *
 * The first node in every channel is always an implicit GainEffect
 * controlled by volume and muted. User-added effects follow after it.
 *
 * Signal flow:
 *   [AudioSources] → GainEffect → [...user effects] → "Master"
 *
 * @example
 * const sfx = Mixer.createChannel("Special Effects", { volume: 0.8 });
 * sfx.addEffect(new ReverbEffect({ decay: 1.2, wet: 0.3 }))
 *    .addEffect(new CompressorEffect({ threshold: -6, ratio: 3 }));
 */
export interface Channel {
    /** The unique name of this channel */
    readonly name: string;

    /** Current volume (0.0 to 1.0) */
    volume: number;

    /** Whether this channel is currently muted */
    muted: boolean;

    /**
     * Sets the volume of this channel.
     * Equivalent to setting the implicit GainEffect at the head of the chain.
     * @returns this (for method chaining)
     */
    setVolume(volume: number): this;

    /**
     * Mutes or unmutes this channel.
     * Muting sets the implicit GainEffect to 0 without altering the volume value,
     * so unmuting restores the original volume exactly.
     * @returns this (for method chaining)
     */
    setMuted(muted: boolean): this;

    /**
     * Appends an effect to the end of this channel's effect chain.
     * Effects are processed in the order they were added.
     * @returns this (for method chaining)
     */
    addEffect(effect: AudioEffect): this;

    /**
     * Removes an effect from this channel's effect chain.
     * The signal flows directly between the surrounding effects after removal.
     * @returns this (for method chaining)
     */
    removeEffect(effect: AudioEffect): this;

    /**
     * Returns all user-added effects on this channel in chain order.
     * Does not include the implicit GainEffect.
     */
    getEffects(): AudioEffect[];
}

/**
 * Marks this GameObject as the audio listener for the scene.
 * The position and orientation of the attached Transform is used by the
 * engine every frame to compute 3D spatialization for all AudioSources.
 *
 * Only one AudioListener should be active in the scene at a time.
 * Typically attached to the player or main camera GameObject.
 *
 * @example
 * const player = new GameObject({ name: "Player" });
 * const listener = player.addComponent(AudioListener);
 * listener.volume = 0.9;
 */
export class AudioListener extends Component {
    /**
     * Global volume multiplier applied to all audio received by this listener.
     * (0.0 = silence, 1.0 = full volume). Default: 1.0
     */
    volume: number;
}

/**
 * Emits audio from this GameObject's position in 3D space.
 * The Transform of the attached GameObject is read every frame to
 * update the sound's position in the C++ audio engine for spatialization.
 *
 * Route the source into a Mixer channel by setting the channel property.
 * If no channel is set, the source routes directly into "Master".
 *
 * Signal flow:
 *   AudioSource → Mixer.channel(this.channel ?? "Master") → "Master" → output
 *
 * @example
 * const fire = new GameObject({ name: "Fire" });
 * const source = fire.addComponent(AudioSource);
 * source.clip = "fire_loop";
 * source.channel = "sfx";
 * source.loop = true;
 * source.maxDistance = 20.0;
 * source.playOnStart = true;
 */
export class AudioSource extends Component {
    /**
     * The sound asset ID to play, as registered via Audio.load().
     * Setting this does not automatically play the sound.
     */
    clip: string | null;

    /**
     * The Mixer channel this source is routed through.
     * If null, the source routes directly into "Master".
     */
    channel: string | null;

    /** Volume of this individual source (0.0 to 1.0). Default: 1.0 */
    volume: number;

    /** Whether this source loops. Default: false */
    loop: boolean;

    /**
     * If true, the sound plays automatically when the component is first
     * enabled or the GameObject is instantiated. Default: false
     */
    playOnStart: boolean;

    /**
     * The distance at which this source begins to attenuate.
     * Below this distance the volume is at full. Default: 1.0
     */
    rolloffDistance: number;

    /**
     * The distance beyond which this source is no longer audible.
     * Default: 100.0
     */
    maxDistance: number;

    /** Whether this source is currently playing. */
    readonly isPlaying: boolean;

    /** Starts playback of the current clip from the beginning. */
    play(): void;

    /** Stops playback and resets position to the start. */
    stop(): void;

    /** Pauses playback without resetting the position. */
    pause(): void;

    /** Resumes playback from where it was paused. */
    resume(): void;
}

/**
 * The name of the implicit master channel.
 * Always available via Mixer.channel("Master").
 * All other channels route into this before final output.
 *
 * Ideal for global effects such as a master Compressor effect or limiter.
 *
 * Signal flow:
 *   ["Special Effects"] ───┐
 *             ["Music"] ───┤──► ["Master"] ──► output
 *          ["Ambience"] ───┘
 */
export const Master = "Master";

/**
 * Core Audio Mixer — the single source for all audio routing.
 * All Channels must be created through the Mixer. Backed by a single
 * ma_engine instance on the C++ side.
 *
 * The "Master" channel is created automatically on engine initialization.
 * All user-created channels feed into it.
 *
 * @example
 * const music = Mixer.createChannel("Music", { volume: 0.6 });
 * const sfx   = Mixer.createChannel("Special Effects");
 *
 * music.addEffect(new LowPassEffect({ cutoff: 400 }));
 * sfx.addEffect(new ReverbEffect({ decay: 0.8, wet: 0.25 }));
 *
 * Mixer.channel("Master")
 *      .addEffect(new CompressorEffect({ threshold: -6, ratio: 3 }));
 */
export class Mixer {
    /**
     * Creates a new Channel with the given name and options.
     * If a channel with the same name already exists, the existing one
     * is returned and the options are applied on top of its current state.
     */
    static createChannel(name: string, options?: ChannelOptions): Channel;

    /**
     * Retrieves an existing Channel by name.
     * Returns null if no channel with that name exists.
     * "Master" is always present and never returns null.
     */
    static channel(name: string): Channel | null;
    static channel(name: "Master"): Channel;

    /**
     * Returns the global Mixer instance.
     * You do not need to call this directly — use the static methods instead.
     */
    static getInstance(): Mixer;
}
