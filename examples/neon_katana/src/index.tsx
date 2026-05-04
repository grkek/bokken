import {
    GameObject,
    Transform2D,
    Sprite2D,
    Animation2D,
    Camera2D,
    Mesh2D,
    Shape2D,
    ParticleEmitter2D,
    Distortion2D,
    BlendMode,
    AnimationLoopMode
} from "bokken/gameObject";

import Renderer from "bokken/renderer";
import Window from "bokken/window";
import Input from "bokken/input";

let bat: GameObject;
let transform: Transform2D;
let animation: Animation2D;
let cameraTransform: Transform2D;

function spawnLaser(originX: number, originY: number, targetX: number, targetY: number): void {
    const dx: number = targetX - originX;
    const dy: number = targetY - originY;
    const length: number = Math.sqrt(dx * dx + dy * dy);
    if (length < 0.1) return;

    const angleDeg: number = Math.atan2(dy, dx) * (180.0 / Math.PI);

    const shockwaveCount: number = Math.floor(length / 1.5) + 1;
    for (let i: number = 0; i <= shockwaveCount; i++) {
        const t: number = i / Math.max(shockwaveCount, 1);

        new GameObject("LaserWave_" + i, { destroyWhenIdle: true })
            .addComponent(Transform2D, { positionX: originX + dx * t, positionY: originY + dy * t })
            .addComponent(Distortion2D, {
                speed: 2.5,
                thickness: 0.02,
                amplitude: 0.04,
                maxRadius: 2.5
            })
            .getComponent(Distortion2D)!.trigger();
    }

    const stepCount: number = Math.floor(length / 0.3) + 1;
    for (let i: number = 0; i <= stepCount; i++) {
        const t: number = i / Math.max(stepCount, 1);

        const segment: GameObject = new GameObject("LaserSeg_" + i, { destroyWhenIdle: true });
        segment
            .addComponent(Transform2D, { positionX: originX + dx * t, positionY: originY + dy * t })
            .addComponent(ParticleEmitter2D, {
                emitting: false,
                blendMode: BlendMode.Additive,
                maximumParticles: 128,
                colorStart: 0x44DDFFFF,
                colorEnd: 0x2266FF00,
                lifetimeMinimum: 0.1,
                lifetimeMaximum: 0.4,
                speedMinimum: 1.0,
                speedMaximum: 3.0,
                sizeStart: 0.15,
                sizeEnd: 0.02,
                spreadAngle: 30,
                direction: angleDeg,
                damping: 0.5,
                gravity: 0,
                zOrder: 15,
            });
        segment.getComponent(ParticleEmitter2D)!.burst(8);
    }

    const impact: GameObject = new GameObject("LaserImpact", { destroyWhenIdle: true });
    impact
        .addComponent(Transform2D, { positionX: targetX, positionY: targetY })
        .addComponent(Distortion2D, {
            speed: 0.6,
            thickness: 0.02,
            amplitude: 0.10,
            maxRadius: 0.25
        })
        .addComponent(ParticleEmitter2D, {
            emitting: false,
            blendMode: BlendMode.Additive,
            maximumParticles: 512,
            colorStart: 0x88EEFFFF,
            colorEnd: 0x2244FF00,
            lifetimeMinimum: 0.2,
            lifetimeMaximum: 0.6,
            speedMinimum: 3.0,
            speedMaximum: 8.0,
            sizeStart: 0.3,
            sizeEnd: 0.02,
            sizeStartVariance: 0.1,
            spreadAngle: 360,
            direction: 90,
            damping: 0.3,
            gravity: 0,
            zOrder: 14,
        });

    impact.getComponent(Distortion2D)!.trigger();
    impact.getComponent(ParticleEmitter2D)!.burst(50);
}

export function onStart(): void {
    const camera: GameObject = new GameObject("Camera");
    camera.addComponent(Transform2D, { positionX: 0, positionY: 0 });
    camera.addComponent(Camera2D, { zoom: 32, isActive: true });
    cameraTransform = camera.getComponent(Transform2D)!;

    Renderer.pipeline.addStage("distortion", "distortion", {
        intensity: 0.15
    });
    Renderer.pipeline.moveStage("distortion", 1);

    Renderer.pipeline.addStage("bloom", "bloom", {
        threshold: 0.35,
        intensity: 0.9,
        radius: 1.4
    });
    Renderer.pipeline.moveStage("bloom", 2);

    Renderer.pipeline.addStage("color-grade", "color-grade", {
        exposure: 1.15,
        saturation: 1.2,
        gamma: 2.2
    });
    Renderer.pipeline.moveStage("color-grade", 3);

    const ground: GameObject = new GameObject("Ground");
    ground
        .addComponent(Transform2D, { positionX: 0, positionY: -8, scaleX: 40, scaleY: 0.6 })
        .addComponent(Mesh2D, { shape: Shape2D.Quad, color: 0x3A4855FF });

    const colors: number[] = [0x566E7FFF, 0x4A5E6FFF, 0x6B8399FF, 0x4A7A9AFF, 0x3E5F80FF];
    for (let i: number = 0; i < 30; i++) {
        const block: GameObject = new GameObject("Block_" + i);
        const bx: number = (i - 15) * 1.3;
        const by: number = -7.0 + (i % 3) * 0.5;
        const sz: number = 0.4 + (i % 4) * 0.2;
        block
            .addComponent(Transform2D, { positionX: bx, positionY: by, scaleX: sz, scaleY: sz })
            .addComponent(Mesh2D, { shape: Shape2D.Quad, color: colors[i % colors.length] });
    }

    for (let i: number = 0; i < 10; i++) {
        const pillar: GameObject = new GameObject("Pillar_" + i);
        const px: number = (i - 5) * 4.0 + 2.0;
        const ph: number = 3.0 + (i % 3) * 2.0;
        pillar
            .addComponent(Transform2D, { positionX: px, positionY: -8 + ph * 0.5 + 0.3, scaleX: 0.5, scaleY: ph })
            .addComponent(Mesh2D, { shape: Shape2D.Quad, color: 0x4D6070FF });
    }

    for (let i: number = 0; i < 20; i++) {
        const dot: GameObject = new GameObject("BgDot_" + i);
        const dx: number = (i - 10) * 2.0 + 0.7;
        const dy: number = 1.0 + (i % 5) * 1.5;
        const ds: number = 0.12 + (i % 3) * 0.1;
        dot
            .addComponent(Transform2D, { positionX: dx, positionY: dy, scaleX: ds, scaleY: ds })
            .addComponent(Mesh2D, { shape: Shape2D.Quad, color: 0x7090A8FF });
    }

    for (let i: number = 0; i < 5; i++) {
        const plat: GameObject = new GameObject("Platform_" + i);
        const ppx: number = (i - 2) * 6.0;
        const ppy: number = -3.0 + (i % 2) * 2.0;
        plat
            .addComponent(Transform2D, { positionX: ppx, positionY: ppy, scaleX: 3.0, scaleY: 0.3 })
            .addComponent(Mesh2D, { shape: Shape2D.Quad, color: 0x506878FF });
    }

    bat = new GameObject("Bat");
    bat
        .addComponent(Transform2D, { positionX: 0, positionY: 0, scaleX: 3, scaleY: 3 })
        .addComponent(Sprite2D, { texturePath: "/sprites/bat.png" })
        .addComponent(Animation2D);

    transform = bat.getComponent(Transform2D)!;
    animation = bat.getComponent(Animation2D)!;

    animation.addClip({
        name: "fly",
        frames: { frameWidth: 32, frameHeight: 32, count: 4 },
        fps: 8,
        loop: AnimationLoopMode.Loop
    });

    animation.play("fly");
}

export function onUpdate(deltaTime: number): void {
    const speed: number = 5.0;

    if (Input.isKeyDown("KeyD") || Input.isKeyDown("ArrowRight")) {
        transform.positionX += speed * deltaTime;
        const sprite: Sprite2D = bat.getComponent(Sprite2D)!;
        sprite.flipX = false;
    }

    if (Input.isKeyDown("KeyA") || Input.isKeyDown("ArrowLeft")) {
        transform.positionX -= speed * deltaTime;
        const sprite: Sprite2D = bat.getComponent(Sprite2D)!;
        sprite.flipX = true;
    }

    if (Input.isKeyDown("KeyW") || Input.isKeyDown("ArrowUp")) {
        transform.positionY += speed * deltaTime;
    }

    if (Input.isKeyDown("KeyS") || Input.isKeyDown("ArrowDown")) {
        transform.positionY -= speed * deltaTime;
    }

    // Smooth camera follow.
    const lerp: number = 5.0;
    cameraTransform.positionX += (transform.positionX - cameraTransform.positionX) * lerp * deltaTime;
    cameraTransform.positionY += (transform.positionY - cameraTransform.positionY) * lerp * deltaTime;

    if (Input.isMousePressed(0)) {
        const mouse = Input.getMousePosition();
        const size = Window.getSize();
        const zoom: number = 32.0;
        const worldMouseX: number = cameraTransform.positionX + (mouse.x - size.width * 0.5) / zoom;
        const worldMouseY: number = cameraTransform.positionY - (mouse.y - size.height * 0.5) / zoom;
        spawnLaser(transform.positionX, transform.positionY, worldMouseX, worldMouseY);
    }
}

export function onFixedUpdate(deltaTime: number): void { }