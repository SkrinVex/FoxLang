import sys

def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 500000
    items = []
    for i in range(n):
        items.append(i % 100)
    total = 0
    for i in range(len(items)):
        total = total + items[i]
    print(total)

main()
