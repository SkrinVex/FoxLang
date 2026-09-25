n = (ARGV[0] || "3000000").to_i
total = 0
i = 0
while i < n
  r = i % 1000
  total = total + r * r % 7
  i += 1
end
puts total
