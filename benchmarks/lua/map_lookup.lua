local n = tonumber(arg[1] or "200000")
local index = {}
local count = 0
for i = 0, n - 1 do
    local key = "k" .. i
    if index[key] == nil then
        count = count + 1
    end
    index[key] = i
end
local total = 0
for i = 0, n - 1 do
    total = total + index["k" .. i] % 1000
end
print(total .. " " .. count)
