const n = Number(process.argv[2] ?? 200000);
const index = new Map();
for (let i = 0; i < n; i++) {
    index.set("k" + i, i);
}
let total = 0;
for (let i = 0; i < n; i++) {
    total = total + (index.get("k" + i) % 1000);
}
console.log(total, index.size);
