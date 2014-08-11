local path = ...
package.cpath = path.."lib\\?.dll;"..package.cpath
package.path =  path.."servicelogic\\?.lua;" ..path.."lib\\?.lua;".. package.path
--require "kissserver"
local iocpsocket = require "iocpsocket"
local iocp = iocpsocket.createsocket()
iocp:start()
local client = iocp:connect("127.0.0.1", 12345)
iocp:recv(
	function(id, itype, buf)
	function(id, itype, buf)
		print(buf)
	end
)
if client then
	iocp:send(client, iocpsocket.MESSAGE_TYPE_RAW, "anmeng", 6)
end