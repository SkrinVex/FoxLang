import json
import sys

def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 50000
    records = []
    for i in range(n):
        records.append({"id": i, "name": "user" + str(i)})
    text = json.dumps(records, separators=(",", ":"))
    back = json.loads(text)
    total = 0
    for i in range(len(back)):
        total = total + back[i]["id"] % 1000
    print(len(back), total, len(text))

main()
