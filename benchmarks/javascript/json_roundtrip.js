const n = Number(process.argv[2] ?? 50000);
const records = [];
for (let i = 0; i < n; i++) {
    records.push({ id: i, name: "user" + i });
}
const text = JSON.stringify(records);
const back = JSON.parse(text);
let total = 0;
for (let i = 0; i < back.length; i++) {
    total = total + (back[i].id % 1000);
}
console.log(back.length, total, text.length);
