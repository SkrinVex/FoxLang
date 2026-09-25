n = (ARGV[0] || "200000").to_i
parts = []
n.times { |i| parts.push("item" + i.to_s) }
text = parts.join(",")
back = text.split(",", -1)
total = 0
back.each { |piece| total = total + piece.size }
puts "#{back.size} #{total} #{text.size}"
