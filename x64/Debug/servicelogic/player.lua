local msgdefine = require "msgdefine"
local protobuf = require "protobuf"
local addr = io.open("msgproto.pb","rb")
local buffer = addr:read "*a"
addr:close()
protobuf.register(buffer)
local seri = require "serialize"
local player = {}
player.__index = player

function player.create(id)
	local p = setmetatable({}, player)
	p.msglist = {}
	p.id = id
	return p
end

function ping()
end

function player:send(itype, s)
	if itype == self.iocp.MESSAGE_TYPE_DEFINE then
		self.iocp:send(self.id, self.iocp.MESSAGE_TYPE_DEFINE,s)
	elseif itype == self.iocp.MESSAGE_TYPE_LUA then
		local code = seri.pack(s)
		local sended, len = seri.serialize(code)
		self.iocp:send(self.id,self.iocp.MESSAGE_TYPE_LUA, sended, len)
	elseif itype == self.iocp.MESSAGE_TYPE_PB then
		local code = protobuf.encode("kissmessage.Message", s)
		self.iocp:send(self.id,self.iocp.MESSAGE_TYPE_PB, code, len)
	end
end

function player:addmsg(itype, msg)
	if itype == self.iocp.MESSAGE_TYPE_DEFINE then
	self.msglist[#self.msglist + 1] = msg
	elseif itype == self.iocp.MESSAGE_TYPE_LUA then
		self.msglist[#self.msglist + 1] = seri.deserialize(msg)
	elseif itype == self.iocp.MESSAGE_TYPE_PB then
		self.msglist[#self.msglist + 1] = protobuf.decode("kissmessage.Message" , msg)
	end
end
function player:dispatch(tick)
	if #self.msglist > 0 then
		local msg = self.msglist[1]		
		self:send(self.iocp.MESSAGE_TYPE_DEFINE, 10)
		table.remove(self.msglist, 1)
		msgcount = msgcount + 1
	end
end


function player.say()
	
end

return player



