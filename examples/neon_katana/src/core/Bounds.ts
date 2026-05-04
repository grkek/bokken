export class Bounds {
    constructor(
        public readonly x: number,
        public readonly y: number,
        public readonly width: number,
        public readonly height: number,
    ) {}

    get left(): number   { return this.x - this.width / 2; }
    get right(): number  { return this.x + this.width / 2; }
    get top(): number    { return this.y + this.height / 2; }
    get bottom(): number { return this.y - this.height / 2; }

    overlaps(other: Bounds): boolean {
        return this.left < other.right &&
               this.right > other.left &&
               this.bottom < other.top &&
               this.top > other.bottom;
    }
}
