local socketdriver = require "socketdriver.c"
local iocpsocket = {iocp = nil}
iocpsocket.__gc = function(p) socketdriver.freesocket(p.iocp) end 
iocpsocket.__index = iocpsocket
function checkself(self)
	if not self then print("please call like iocpsocket:xxx style") end
end
iocpsocket.MESSAGE_TYPE_DEFINE = 1
iocpsocket.MESSAGE_TYPE_LUA = 2
iocpsocket.MESSAGE_TYPE_PB = 3
iocpsocket.MESSAGE_TYPE_RAW = 4
iocpsocket.MODE_NORMOL = 1
iocpsocket.MODE_RAW = 2
iocpsocket.MODE_SUB = 3

function iocpsocket.createsocket()	
	local ret = setmetatable({}, iocpsocket)
	ret.iocp = socketdriver.createsocket()	
	return ret
end;

function iocpsocket:onaccept(callback)
	checkself(self)
	socketdriver.onaccept(self.iocp, callback)
end

function iocpsocket:ondisconnect(callback)
	checkself(self)
	socketdriver.ondisconnect(self.iocp, callback)
end

function iocpsocket:listen(port, ip)
	checkself(self)
	if not ip then
		return socketdriver.listen(self.iocp, port)
	else
		return socketdriver.listen(self.iocp, ip, port)
	end
end

function iocpsocket:connect(ip, port)
	checkself(self)
	if ip and port then
		return socketdriver.connect(self.iocp, ip, port)
	end
	return false
end

function iocpsocket:send(fd, itype, s,...)
	checkself(self)
	if s then
		return socketdriver.send(self.iocp, fd, itype, s, ...)
	else
		return false
	end
end

function iocpsocket:recv(callback)
	checkself(self)
	socketdriver.recv(self.iocp, callback)
end

function iocpsocket:start(callback)
	checkself(self)
	if callback then iocpsocket.poll(callback) end
	socketdriver.start(self.iocp)
end

function iocpsocket.poll(callback)
	if callback then socketdriver.poll(callback) end
end

function iocpsocket:kick(fd)
	checkself(self)
	socketdriver.kick(self.iocp, fd)
end

function iocpsocket:close()
	checkself(self)
	socketdriver.stop(self.iocp)
end

function iocpsocket:getaddr(fd)
	checkself(self)
	return socketdriver.getsocketaddr(self.iocp,fd)
end

function iocpsocket:setsocketmode(fd, mode)
	checkself(self)
	socketdriver.setmode(self, fd, mode)
end

return iocpsocket

