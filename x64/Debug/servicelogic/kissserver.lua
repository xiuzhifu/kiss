local iocpsocket = require "iocpsocket"
local player = require "player"
local iocp = iocpsocket.createsocket()
if iocp:listen(12345) then
	print("start listening at 12345 port")
else
	print("starting listening fail")
end
clientcount = 0;
local clientpool = {}
iocp:onaccept(function(fd) 
	local player = player.create(fd)
	player.iocp = iocp
	clientpool[fd] = player
	print("add a new client connected id: ",fd, "ip:", iocp:getaddr(fd))
end)

iocp:ondisconnect(function(fd)
	local player = clientpool[fd]
	clientpool[fd] = nil
	player = nil
	print(" a client disconnect id:", fd)
end
)
iocp:recv(
	function(fd,itype, buf) 
		local player = clientpool[fd]
		if player then
			player:addmsg(itype, buf)
		end
	end
)
local lasttick = 0;
msgcount = 0;
poll = function (tick)
	if tick - lasttick > 1000 then 
		lasttick = tick
		k, b = collectgarbage("count")
		if msgcount > 0 then
			print("player count: ", #clientpool, " proc msg count :",msgcount, "k:", k)
			msgcount = 0
		end
	end
	
	for i, v in pairs(clientpool) do
		v:dispatch(tick)
	end
	return true
end
iocp:start(poll)