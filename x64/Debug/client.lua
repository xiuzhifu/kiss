local path = ...
package.cpath = path.."lib\\?.dll;"..package.cpath
package.path =  path.."servicelogic\\?.lua;" ..path.."lib\\?.lua;".. package.path
local iocpsocket = require "iocpsocket"
local protobuf = require "protobuf"
local addr = io.open("msgproto.pb","rb")
local buffer = addr:read "*a"
addr:close()
protobuf.register(buffer)
local s = require "serialize"
local iocp = iocpsocket.createsocket()
local client = iocp:connect("127.0.0.1", 12345)
local msg = {
	id = 1,
	request = {
		loginreq = {
		username = "anmeng1234",
		password = "1234567890"
		}
	}
	}
msgcount = 0
iocp:recv(
	function(id, itype, buf, buflen) 
		
		--local msg1 = protobuf.decode("kissmessage.Message" , buf)
		if not (itype == iocpsocket.MESSAGE_TYPE_LUA) then return end
		msg1 = s.deserialize(buf)
		if msg1.id == 2 then
			if msg1.response.result then
				print("login ok")
			else
				print("login fail")
			end
		end
		--local code = protobuf.encode("kissmessage.Message", msg)
		local code = s.pack(msg)
		local send2, len = s.serialize(code) 
		iocp:send(client,iocp.MESSAGE_TYPE_LUA, send2, len)
		msgcount = msgcount + 1
		if msgcount > 1000 then iocp = nil end
	end
)
iocp:start()
if client then
	local code = s.pack(msg)
	local send1 , len= s.serialize(code)
	if iocp:send(client,iocp.MESSAGE_TYPE_LUA, send1, len) then
		print "send ok"
	else
		print "send fail"
	end
else
	print("client = nil")
end
iocp = nil

