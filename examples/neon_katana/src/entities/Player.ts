import { GameObject, Transform2D, Mesh2D, Rigidbody2D, Shape2D, ParticleEmitter2D } from "bokken/gameObject";
import Input from "bokken/input";
import { Physics, PlayerConfiguration } from "../constants.ts";
import { Bounds } from "../core/Bounds.ts";

const ParticleEase = {
    Linear: 0,
    EaseInQuad: 1,
    EaseOutQuad: 2,
    EaseInOutQuad: 3,
    EaseOutCubic: 4,
    EaseInCubic: 5,
} as const;

class JelloSpring {
    private stiffness = 280;
    private friction = 11;
    private dispX = 0;
    private dispY = 0;
    private velX = 0;
    private velY = 0;
    private breathTimer = 0;

    impulse(sx: number, sy: number): void {
        this.velX += sx;
        this.velY += sy;
    }

    update(dt: number): void {
        const ax = -this.stiffness * this.dispX - this.friction * this.velX;
        const ay = -this.stiffness * this.dispY - this.friction * this.velY;
        this.velX += ax * dt;
        this.velY += ay * dt;
        this.dispX += this.velX * dt;
        this.dispY += this.velY * dt;
        this.breathTimer += dt;
    }

    getScaleX(): number {
        const breath = Math.sin(this.breathTimer * 1.6 * Math.PI * 2) * 0.008;
        const sy = 1 + this.dispY + breath;
        return (1 / sy) + this.dispX * 0.3;
    }

    getScaleY(): number {
        const breath = Math.sin(this.breathTimer * 1.6 * Math.PI * 2) * 0.008;
        return 1 + this.dispY + breath;
    }
}

export class Player {
    private readonly gameObject: GameObject;
    private readonly hull: GameObject;
    private readonly noseCone: GameObject;
    private readonly finLeft: GameObject;
    private readonly finRight: GameObject;
    private readonly cockpit: GameObject;
    private readonly thrustEmitter: GameObject;
    private readonly boostEmitter: GameObject;

    private heading = 90;
    private thrusting = false;
    private reversing = false;
    private boosting = false;
    private boostTimer = 0;
    private boostCooldown = 0;
    private jello = new JelloSpring();

    constructor() {
        const bw = PlayerConfiguration.bodyWidth;
        const bh = PlayerConfiguration.bodyHeight;

        // Root — invisible anchor with physics. Children inherit its transform.
        this.gameObject = new GameObject("Player")
            .addComponent(Transform2D, {
                positionX: PlayerConfiguration.spawnX,
                positionY: PlayerConfiguration.spawnY,
                scaleX: 1,
                scaleY: 1,
            })
            .addComponent(Rigidbody2D, { mass: 1, useGravity: false });

        // Hull (main body).
        this.hull = new GameObject("PlayerHull")
            .addComponent(Transform2D, {
                positionX: 0, positionY: 0,
                scaleX: bw, scaleY: bh,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Quad, color: PlayerConfiguration.color });
        this.hull.setParent(this.gameObject);

        // Nose cone (triangle on top).
        this.noseCone = new GameObject("PlayerNose")
            .addComponent(Transform2D, {
                positionX: 0, positionY: bh * 0.5 + 0.12,
                scaleX: bw * 0.9, scaleY: 0.28,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Triangle, color: PlayerConfiguration.noseColor });
        this.noseCone.setParent(this.gameObject);

        // Left fin.
        this.finLeft = new GameObject("PlayerFinL")
            .addComponent(Transform2D, {
                positionX: -bw * 0.55, positionY: -bh * 0.35,
                scaleX: 0.18, scaleY: 0.28,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Triangle, color: PlayerConfiguration.finColor });
        this.finLeft.setParent(this.gameObject);

        // Right fin.
        this.finRight = new GameObject("PlayerFinR")
            .addComponent(Transform2D, {
                positionX: bw * 0.55, positionY: -bh * 0.35,
                scaleX: 0.18, scaleY: 0.28,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Triangle, color: PlayerConfiguration.finColor });
        this.finRight.setParent(this.gameObject);

        // Cockpit window (small circle).
        this.cockpit = new GameObject("PlayerCockpit")
            .addComponent(Transform2D, {
                positionX: 0, positionY: bh * 0.15,
                scaleX: bw * 0.45, scaleY: bw * 0.45,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Circle, color: PlayerConfiguration.windowColor });
        this.cockpit.setParent(this.gameObject);

        // Engine thrust emitter (not parented — positioned manually in world space).
        this.thrustEmitter = new GameObject("PlayerThrust")
            .addComponent(Transform2D)
            .addComponent(ParticleEmitter2D, {
                emitting: false,
                emitRate: 80,
                colorStart: 0xFF6600EE,
                colorEnd: 0xFF220000,
                speedMinimum: 3,
                speedMaximum: 7,
                direction: 270,
                spreadAngle: 25,
                sizeStart: 0.14,
                sizeEnd: 0.02,
                sizeStartVariance: 0.04,
                sizeEase: ParticleEase.EaseOutCubic,
                lifetimeMinimum: 0.08,
                lifetimeMaximum: 0.2,
                gravity: 0,
                damping: 1.0,
                spawnOffsetX: 0.06,
                alphaEase: ParticleEase.EaseInQuad,
                maximumParticles: 100,
            });

        // Boost emitter (not parented — positioned manually).
        this.boostEmitter = new GameObject("PlayerBoost")
            .addComponent(Transform2D)
            .addComponent(ParticleEmitter2D, {
                emitting: false,
                emitRate: 120,
                colorStart: 0x44DDFFCC,
                colorEnd: 0x2288FF00,
                speedMinimum: 4,
                speedMaximum: 12,
                direction: 270,
                spreadAngle: 40,
                sizeStart: 0.2,
                sizeEnd: 0.03,
                sizeStartVariance: 0.08,
                sizeEase: ParticleEase.EaseOutQuad,
                lifetimeMinimum: 0.1,
                lifetimeMaximum: 0.35,
                gravity: 0,
                damping: 2.0,
                spawnOffsetX: 0.1,
                spawnOffsetY: 0.05,
                alphaEase: ParticleEase.EaseOutCubic,
                maximumParticles: 120,
            });
    }

    get transform(): Transform2D {
        return this.gameObject.getComponent(Transform2D)!;
    }

    get rigidbody(): Rigidbody2D {
        return this.gameObject.getComponent(Rigidbody2D)!;
    }

    get bounds(): Bounds {
        const t = this.transform;
        return new Bounds(
            t.positionX, t.positionY,
            PlayerConfiguration.bodyWidth * 1.2,
            PlayerConfiguration.bodyHeight * 1.2,
        );
    }

    handleInput(dt: number): void {
        const rb = this.rigidbody;
        const t = this.transform;

        // Rotation.
        const rotLeft = Input.isKeyDown("ArrowLeft") || Input.isKeyDown("KeyA");
        const rotRight = Input.isKeyDown("ArrowRight") || Input.isKeyDown("KeyD");
        if (rotLeft) this.heading += Physics.rotationSpeed * dt;
        if (rotRight) this.heading -= Physics.rotationSpeed * dt;
        this.heading = ((this.heading % 360) + 360) % 360;

        // Record thrust state for fixedUpdate.
        this.thrusting = Input.isKeyDown("ArrowUp") || Input.isKeyDown("KeyW");
        this.reversing = Input.isKeyDown("ArrowDown") || Input.isKeyDown("KeyS");

        if (this.thrusting) {
            this.jello.impulse(0, 0.03);
        }

        // Boost.
        if (this.boostCooldown > 0) this.boostCooldown -= dt;

        if ((Input.isKeyPressed("Space") || Input.isKeyPressed("ShiftLeft") || Input.isKeyPressed("ShiftRight")) && this.boostCooldown <= 0) {
            this.boosting = true;
            this.boostTimer = 0.15;
            // TODO: Uncomment this to ruin the fun
            // this.boostCooldown = 0.8;
            const headingRad = this.heading * (Math.PI / 180);
            rb.velocityX = rb.velocityX + Math.cos(headingRad) * 50;
            rb.velocityY = rb.velocityY + Math.sin(headingRad) * 50;
            this.jello.impulse(0, 0.3);
            this.boostEmitter.getComponent(ParticleEmitter2D)!.burst(50);
        }

        if (this.boosting) {
            this.boostTimer -= dt;
            if (this.boostTimer <= 0) this.boosting = false;
        }

        // Apply heading as rotation on the root transform.
        // heading=90 means pointing up. Engine rotation 0 means unrotated (pointing right).
        // So rotation = heading - 90 makes heading=90 -> rotation=0 (up).
        t.rotation = this.heading - 90;

        // Position emitters in world space (behind the rocket).
        this.syncEmitters(t);

        // Jello — applied to root scale, children inherit it.
        this.jello.update(dt);
        t.scaleX = this.jello.getScaleX();
        t.scaleY = this.jello.getScaleY();
    }

    // Called from fixedUpdate — forces and drag.
    applyGravity(dt: number): void {
        const rb = this.rigidbody;
        const headingRad = this.heading * (Math.PI / 180);
        const dirX = Math.cos(headingRad);
        const dirY = Math.sin(headingRad);

        if (this.thrusting) {
            rb.applyForce(dirX * Physics.thrustForce, dirY * Physics.thrustForce);
        }
        if (this.reversing) {
            rb.applyForce(-dirX * Physics.reverseForce, -dirY * Physics.reverseForce);
        }

        // Speed cap.
        const speed = Math.sqrt(rb.velocityX * rb.velocityX + rb.velocityY * rb.velocityY);
        if (speed > Physics.maxSpeed) {
            const s = Physics.maxSpeed / speed;
            rb.velocityX = rb.velocityX * s;
            rb.velocityY = rb.velocityY * s;
        }

        // Drag.
        const dragFactor = Math.max(0, 1 - Physics.drag * dt);
        rb.velocityX = rb.velocityX * dragFactor;
        rb.velocityY = rb.velocityY * dragFactor;
        if (Math.abs(rb.velocityX) < 0.01) rb.velocityX = 0;
        if (Math.abs(rb.velocityY) < 0.01) rb.velocityY = 0;
    }

    respawn(): void {
        const t = this.transform;
        t.positionX = PlayerConfiguration.spawnX;
        t.positionY = PlayerConfiguration.spawnY;
        t.scaleX = 1;
        t.scaleY = 1;
        t.rotation = 0;
        this.rigidbody.setVelocity(0, 0);
        this.heading = 90;
        this.boosting = false;
        this.boostTimer = 0;
        this.boostCooldown = 0;
        this.thrusting = false;
        this.reversing = false;
        this.thrustEmitter.getComponent(ParticleEmitter2D)!.emitting = false;
        this.boostEmitter.getComponent(ParticleEmitter2D)!.emitting = false;
    }

    hasFallen(): boolean {
        return false;
    }

    private syncEmitters(playerT: Transform2D): void {
        const headingRad = this.heading * (Math.PI / 180);
        const exhaustDist = PlayerConfiguration.bodyHeight * 0.55;
        const exX = playerT.positionX - Math.cos(headingRad) * exhaustDist;
        const exY = playerT.positionY - Math.sin(headingRad) * exhaustDist;

        const thrustT = this.thrustEmitter.getComponent(Transform2D)!;
        const thrust = this.thrustEmitter.getComponent(ParticleEmitter2D)!;
        thrustT.positionX = exX;
        thrustT.positionY = exY;
        thrust.direction = this.heading + 180;
        thrust.emitting = this.thrusting;

        const boostT = this.boostEmitter.getComponent(Transform2D)!;
        const boost = this.boostEmitter.getComponent(ParticleEmitter2D)!;
        boostT.positionX = exX;
        boostT.positionY = exY;
        boost.direction = this.heading + 180;
        boost.emitting = this.boosting;
    }
}