n = (ARGV[0] || "500000").to_i
items = []
n.times { |i| items.push(i % 100) }
total = 0
i = 0
while i < items.size
  total = total + items[i]
  i += 1
end
puts total
