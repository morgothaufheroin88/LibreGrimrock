-- Compile without executing game code. LuaJIT 2.0 is required for bytecode v1.
local source, output = arg[1], arg[2]
local fn, err = loadfile(source)
if not fn then io.stderr:write(err, "\n"); os.exit(1) end
local stream = assert(io.open(output, "wb"))
stream:write(string.dump(fn, true))
stream:close()
