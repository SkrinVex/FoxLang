const n = Number(process.argv[2] ?? 500000);
const items = [];
for (let i = 0; i < n; i++) {
    items.push(i % 100);
}
let total = 0;
for (let i = 0; i < items.length; i++) {
    total = total + items[i];
}
console.log(total);
