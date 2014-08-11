function iocpsocket:request(response, fd, s)
 local co = coroutine.create(
	function()
		self.reqpoll[fd] = co
		self:send(fd, iocpsocket.MESSAGE_TYPE_RAW, s, string.len(s))	
		while true do
			local ret, req = response(coroutine.yield())
			if not ret then 
				self.reqpoll[fd] = nil
				break 
			end
			self:send(fd, iocpsocket.MESSAGE_TYPE_RAW, req, string.len(req))	
		end
	end
 )
	coroutine.resume(co)
end