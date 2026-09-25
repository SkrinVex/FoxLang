local n = tonumber(arg[1] or "500000")
local items = {}
for i = 0, n - 1 do
    items[#items + 1] = i % 100
end
local total = 0
for i = 1, #items do
    total = total + items[i]
end
print(total)
