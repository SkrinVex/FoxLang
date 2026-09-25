Point = Struct.new(:x, :y)

n = (ARGV[0] || "300000").to_i
points = []
n.times { |i| points.push(Point.new(i % 1000, i % 7)) }
total = 0
points.each { |p| total = total + p.x + p.y }
puts total
