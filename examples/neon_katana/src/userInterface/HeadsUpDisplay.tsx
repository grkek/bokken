import Canvas, { View, Label } from "bokken/canvas";

export class HeadsUpDisplay {
    private frameCount = 0;
    private elapsed = 0;
    private fps = 0;

    render(): void {
        Canvas.render(
            <View style={{
                position: "Absolute",
                top: 12,
                left: 12,
                backgroundColor: 0x00000088,
                padding: 8,
                borderRadius: 4,
            }}>
                <Label style={{ fontSize: 14, color: 0xFFFFFFFF }}>
                    {`FPS: ${this.fps}`}
                </Label>
            </View>
        );
    }

    update(deltaTime: number): void {
        this.frameCount++;
        this.elapsed += deltaTime;

        if (this.elapsed >= 1.0) {
            this.fps = this.frameCount;
            this.frameCount = 0;
            this.elapsed -= 1.0;
            this.render();
        }
    }
}
