import sys

def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 300000
    numbers = []
    seed = 1
    for i in range(n):
        seed = (seed * 75 + 74) % 65537
        numbers.append(seed)
    numbers.sort()
    print(numbers[0], numbers[n // 2], numbers[n - 1])

main()
