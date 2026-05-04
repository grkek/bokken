export const Physics = {
    gravity: -15,
    jumpForce: 8,
    moveSpeed: 6,
    friction: 0.85,
} as const;

export const PlayerConfiguration = {
    width: 0.8,
    height: 0.8,
    spawnX: 0,
    spawnY: 3,
    fallResetY: -8,
    color: 0x33CCFFFF,
} as const;

export const LevelConfiguration = {
    groundColor: 0x44AA44FF,
    platformColor: 0x888888FF,
    coinColor: 0xFFDD00FF,
    coinSize: 0.4,
} as const;
