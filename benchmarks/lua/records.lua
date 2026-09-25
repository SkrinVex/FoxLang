local n = tonumber(arg[1] or "300000")
local points = {}
for i = 0, n - 1 do
    points[#points + 1] = {x = i % 1000, y = i % 7}
end
local total = 0
for i = 1, #points do
    local p = points[i]
    total = total + p.x + p.y
end
print(total)
