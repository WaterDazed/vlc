function probe()
	vlc.msg.dbg("Access:" .. vlc.access)
	vlc.msg.dbg("Path:" .. vlc.path)
	return vlc.access == "mock" and vlc.path == "length=100"
end

function parse()
	vlc.msg.dbg("Parse is triggered " .. vlc.access)
	local line = vlc.readline()
	local title = string.match(line, "<title>(.-)</title>")
	return { { name = title, path = vlc.access .. "://" .. vlc.path } }
end
