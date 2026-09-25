import sys

def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 200000
    parts = []
    for i in range(n):
        parts.append("item" + str(i))
    text = ",".join(parts)
    back = text.split(",")
    total = 0
    for i in range(len(back)):
        total = total + len(back[i])
    print(len(back), total, len(text))

main()
