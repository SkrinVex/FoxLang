import sys

class Point:
    __slots__ = ("x", "y")

    def __init__(self, x, y):
        self.x = x
        self.y = y

def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 300000
    points = []
    for i in range(n):
        points.append(Point(i % 1000, i % 7))
    total = 0
    for i in range(len(points)):
        p = points[i]
        total = total + p.x + p.y
    print(total)

main()
