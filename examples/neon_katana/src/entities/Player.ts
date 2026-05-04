import { GameObject, Transform2D, Mesh2D, Rigidbody2D, Shape2D } from "bokken/gameObject";
import Input from "bokken/input";
import { Physics, PlayerConfiguration } from "../constants.ts";
import { Bounds } from "../core/Bounds.ts";

export class Player {
    private readonly gameObject: GameObject;
    private grounded = false;

    constructor() {
        this.gameObject = new GameObject("Player")
            .addComponent(Transform2D, {
                positionX: PlayerConfiguration.spawnX,
                positionY: PlayerConfiguration.spawnY,
                scaleX: PlayerConfiguration.width,
                scaleY: PlayerConfiguration.height,
            })
            .addComponent(Mesh2D, { shape: Shape2D.Quad, color: PlayerConfiguration.color })
            .addComponent(Rigidbody2D, { mass: 1, useGravity: true });
    }

    get transform(): Transform2D {
        return this.gameObject.getComponent(Transform2D)!;
    }

    get rigidbody(): Rigidbody2D {
        return this.gameObject.getComponent(Rigidbody2D)!;
    }

    get bounds(): Bounds {
        const t = this.transform;
        return new Bounds(t.positionX, t.positionY, PlayerConfiguration.width, PlayerConfiguration.height);
    }

    get isGrounded(): boolean {
        return this.grounded;
    }

    set isGrounded(value: boolean) {
        this.grounded = value;
    }

    handleInput(): void {
        const rb = this.rigidbody;

        if (Input.isKeyDown("ArrowLeft") || Input.isKeyDown("KeyA")) {
            rb.velocityX = -Physics.moveSpeed;
        } else if (Input.isKeyDown("ArrowRight") || Input.isKeyDown("KeyD")) {
            rb.velocityX = Physics.moveSpeed;
        } else {
            rb.velocityX = rb.velocityX * Physics.friction;
        }

        if (this.grounded && Input.isKeyPressed("Space")) {
            rb.velocityY = Physics.jumpForce;
            this.grounded = false;
        }
    }

    applyGravity(deltaTime: number): void {
        const rb = this.rigidbody;
        rb.velocityY = rb.velocityY + Physics.gravity * deltaTime;
    }

    respawn(): void {
        const t = this.transform;
        t.positionX = PlayerConfiguration.spawnX;
        t.positionY = PlayerConfiguration.spawnY;
        this.rigidbody.setVelocity(0, 0);
    }

    hasFallen(): boolean {
        return this.transform.positionY < PlayerConfiguration.fallResetY;
    }
}
