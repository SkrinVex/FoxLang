require "json"

n = (ARGV[0] || "50000").to_i
records = []
n.times { |i| records.push({ "id" => i, "name" => "user" + i.to_s }) }
text = JSON.generate(records)
back = JSON.parse(text)
total = 0
back.each { |record| total = total + record["id"] % 1000 }
puts "#{back.size} #{total} #{text.size}"
