export const Physics = {
    thrustForce: 200,
    reverseForce: 6,
    maxSpeed: 30,
    drag: 0.4,
    rotationSpeed: 240,
} as const;

export const PlayerConfiguration = {
    spawnX: 0,
    spawnY: 0,
    bodyWidth: 0.35,
    bodyHeight: 0.9,
    color: 0xCCDDEEEE,
    noseColor: 0xCCDDEEEE,
    finColor: 0x5588CCEE,
    windowColor: 0x33CCFFEE,
} as const;

export const LevelConfiguration = {
    parallaxFactors: [0.08, 0.25] as readonly number[],

    // Crystals
    crystalColor: 0xFFFFFFFF,
    crystalGlowColor: 0x33FFAA00,
    crystalSize: 0.35,
    crystalCleanRadius: 55,
} as const;