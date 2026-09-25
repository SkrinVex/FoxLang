n = (ARGV[0] || "300000").to_i
numbers = []
seed = 1
n.times do
  seed = (seed * 75 + 74) % 65537
  numbers.push(seed)
end
numbers.sort!
puts "#{numbers[0]} #{numbers[n / 2]} #{numbers[n - 1]}"
