const n = Number(process.argv[2] ?? 3000000);
let total = 0;
for (let i = 0; i < n; i++) {
    const r = i % 1000;
    total = total + (r * r) % 7;
}
console.log(total);
