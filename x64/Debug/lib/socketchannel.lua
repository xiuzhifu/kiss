local  socketchannel = {
	reqpoll = {},
	rsppoll = {}
}

function socketchannel:request(response, fd, ...)
 local co = coroutine.create(
	function()
		self.reqpoll[fd] = co
		self:send(fd, ...)	
		while true do
			local yp = coroutine.yield()
			if response then
				local ret, req = response(coroutine.yield())
				if ret then
					self:send(fd, req)
				else 
					self.reqpoll[fd] = nil
					break 
				end
			else
				rsppoll[fd] = yp
				break
			end
		end
	end
 )
	coroutine.resume(co)
end

function socketchannel:response(fd)
	if self.reqpoll[fd] then
		return true, self.reqpoll[fd]
	end
	return false
end

function socketchannel:checkfd(fd, ... )
	if not self.reqpoll[fd] then return false end
	coroutine.resume(self.reqpoll[fd], ...)
end