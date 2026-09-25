local n = tonumber(arg[1] or "200000")
local parts = {}
for i = 0, n - 1 do
    parts[#parts + 1] = "item" .. i
end
local text = table.concat(parts, ",")
local back = {}
for piece in string.gmatch(text, "([^,]*)") do
    back[#back + 1] = piece
end
local total = 0
for i = 1, #back do
    total = total + #back[i]
end
print(#back .. " " .. total .. " " .. #text)
