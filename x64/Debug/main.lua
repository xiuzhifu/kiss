local path = ...
package.cpath = path.."lib\\?.dll;"..package.cpath
package.path =  path.."servicelogic\\?.lua;" ..path.."lib\\?.lua;".. package.path
local socketchannel = require "socketchannel"

--require "kissserver"
local iocpsocket = require "iocpsocket"
local iocp = iocpsocket.createsocket()
if iocp:listen(12345) then
	print("start listening at 12345 port")
else
	print("starting listening fail")
end

local clientpool = {}
iocp:onaccept(function(fd) 
	clientpool[fd] = fd
	iocp:setsocketmode(fd, iocpsocket.MESSAGE_TYPE_RAW)
	print("add a new client connected id: ",fd, "ip:", iocp:getaddr(fd))
end)

iocp:ondisconnect(function(fd)
	clientpool[fd] = nil
	print(" a client disconnect id:", fd)
end
)
iocp:recv(
	function(fd,itype, buf) 
		iocp:sendraw(fd, buf)
	end
)
iocp:start()