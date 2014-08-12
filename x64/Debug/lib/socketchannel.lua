local  socketchannel = {
	reqpoll = {},
	rsppoll = {}
}

function socketchannel:request(response, fd, s)
 local co = coroutine.create(
	function()
		self.socket:sendraw(fd, s)	
		while true do
			local yparam = coroutine.yield()
			if response then
				local ret, req = response(fd, yparam)
				if ret then
					self.socket:sendraw(fd, req)
				else 
					self.reqpoll[fd] = nil
					break 
				end
			else
				rsppoll[fd] = yparam
				break
			end
		end
	end
 )
 	self.reqpoll[fd] = co
	coroutine.resume(co)
end

function socketchannel:response(fd)
	if self.reqpoll[fd] then
		return true, self.reqpoll[fd]
	end
	return false
end

function socketchannel:checkfd(fd, s)
	if not self.reqpoll[fd] then return false end
	coroutine.resume(self.reqpoll[fd], s)
end

return socketchannel