local n = tonumber(arg[1] or "3000000")
local total = 0
for i = 0, n - 1 do
    local r = i % 1000
    total = total + r * r % 7
end
print(total)
