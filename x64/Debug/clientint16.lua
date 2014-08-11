local path = ...
package.cpath = path.."lib\\?.dll;"..package.cpath
package.path =  path.."servicelogic\\?.lua;" ..path.."lib\\?.lua;".. package.path
local iocpsocket = require "iocpsocket"
local s = require "serialize"
local iocp = iocpsocket.createsocket()
local client = iocp:connect("127.0.0.1", 12345)
local i = 0;
iocp:recv(
	function(id, itype, buf, buflen) 
		if iocp == nil then return end
		if itype == iocp.MESSAGE_TYPE_DEFINE then
		print(buf)
		iocp:send(client,iocp.MESSAGE_TYPE_DEFINE, i)
		i = i + 1
		if i > 1000 then  
			--iocp:stop()
		end
		end
	end
)
iocp:start()
if client then
	if iocp:send(client,iocp.MESSAGE_TYPE_DEFINE, 199) then
		print "send ok"
	else
		print "send fail"
	end
else
	print("client = nil")
	iocp:kick(client)
	iocp:close()
	iocp.poll(function(tick) return false end)
	iocp = nil
	collectgarbage()
end
