class Point {
    constructor(x, y) {
        this.x = x;
        this.y = y;
    }
}

const n = Number(process.argv[2] ?? 300000);
const points = [];
for (let i = 0; i < n; i++) {
    points.push(new Point(i % 1000, i % 7));
}
let total = 0;
for (let i = 0; i < points.length; i++) {
    const p = points[i];
    total = total + p.x + p.y;
}
console.log(total);
