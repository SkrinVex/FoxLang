import sys

def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 200000
    index = {}
    for i in range(n):
        index["k" + str(i)] = i
    total = 0
    for i in range(n):
        total = total + index["k" + str(i)] % 1000
    print(total, len(index))

main()
