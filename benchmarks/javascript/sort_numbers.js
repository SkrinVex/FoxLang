const n = Number(process.argv[2] ?? 300000);
const numbers = [];
let seed = 1;
for (let i = 0; i < n; i++) {
    seed = (seed * 75 + 74) % 65537;
    numbers.push(seed);
}
numbers.sort((a, b) => a - b);
console.log(numbers[0], numbers[Math.floor(n / 2)], numbers[n - 1]);
