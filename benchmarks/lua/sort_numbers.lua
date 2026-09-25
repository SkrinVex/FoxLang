local n = tonumber(arg[1] or "300000")
local numbers = {}
local seed = 1
for i = 0, n - 1 do
    seed = (seed * 75 + 74) % 65537
    numbers[#numbers + 1] = seed
end
table.sort(numbers)
print(numbers[1] .. " " .. numbers[n // 2 + 1] .. " " .. numbers[n])
