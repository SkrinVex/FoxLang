const n = Number(process.argv[2] ?? 200000);
const parts = [];
for (let i = 0; i < n; i++) {
    parts.push("item" + i);
}
const text = parts.join(",");
const back = text.split(",");
let total = 0;
for (let i = 0; i < back.length; i++) {
    total = total + back[i].length;
}
console.log(back.length, total, text.length);
