
os.execute "make clean ; make"

os.execute "clear"

threadsTable = { 1, 4, 16, 32, 48, 64, 96, 128 }

executable = "./build/main"

numberOfAllocators = 8

outputFile = io.open("results.lua", "w")

for allocatorIndex = 0, numberOfAllocators - 1 do

    local allocatorName
    outputFile:write("\nentry {\n\truns = {\n")
    for _, threads in ipairs(threadsTable) do
        outputFile:write("\t\t[" .. threads .. "] = {\n\t\t\t")
        local cmd = executable .. " " .. allocatorIndex .. " " .. threads
        for run = 1, 10 do
            print(cmd)
            local handler = io.popen(cmd, "r")
            allocatorName = handler:read("l")
            local res = handler:read("n")
            handler:close()
            outputFile:write(res .. ", ")
        end
        outputFile:write("\n\t\t},\n")
    end
    outputFile:write("\t},\n\tallocator = ", allocatorName .. "\n}\n")
end

outputFile:close()
