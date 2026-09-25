import sys

def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 3000000
    total = 0
    for i in range(n):
        r = i % 1000
        total = total + r * r % 7
    print(total)

main()
