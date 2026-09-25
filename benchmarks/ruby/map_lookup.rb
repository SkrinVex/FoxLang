n = (ARGV[0] || "200000").to_i
index = {}
n.times { |i| index["k" + i.to_s] = i }
total = 0
n.times { |i| total = total + index["k" + i.to_s] % 1000 }
puts "#{total} #{index.size}"
