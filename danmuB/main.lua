local function GetCurrentModPath()
	if debug and REPENTANCE then
		return string.sub(debug.getinfo(GetCurrentModPath).source,2) .. "/../"
	end
end

local mod_path = GetCurrentModPath()

if mod_path then
	local dmB_init = package.loadlib(GetCurrentModPath() .. "lib/isaac-danmuB.bin","open")
	if dmB_init then
		dmB_init()
	end
else
	Isaac.ConsoleOutput("Error: danmuB service need REPENTANCE and start with --luadebug.\n")
end

