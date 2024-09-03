#! ../lua_interpreter/lua

os.execute 'mkdir CSVs'

function entry(t)
    local filename = "CSVs/" .. t.allocator .. ".csv"
    local file = io.open(filename, "w")
    local runs = t.runs
    local workload = 2 ^ 22
    local datapoints = 8
    local timescale = 10 ^ (-9)
    local millions = 10 ^ 6

    assert(file)
    file:write(t.allocator)                -- Line name
    file:write("\n# of Threads, Mops/s\n") -- Column Header

    function iter(t, s)
        return function(state)
            local i = state.i + 1
            local t = state.table
            if state.elementsFound >= state.tableSize then return nil end
            while not t[i] do i = i + 1 end
            state.elementsFound = state.elementsFound + 1
            state.i = i
            return i, t[i]
        end, { table = t, ['i'] = 0, elementsFound = 0, tableSize = s }
    end

    for threads, times in iter(runs, threadConfigurations) do
        local sum = 0
        local max = math.mininteger
        local min = math.maxinteger

        file:write(threads .. ",")

        for _, time in ipairs(times) do
            if time > max then
                max = time
            end
            if time < min then
                min = time
            end
            sum = sum + time
        end

        sum = sum - max - min

        local result = sum / datapoints         -- average time
        result = result * timescale             -- convert to seconds
        result = (workload / millions) / result -- Mops/s

        file:write(result .. "\n")
    end

    file:close()
end

dofile 'results.lua'
