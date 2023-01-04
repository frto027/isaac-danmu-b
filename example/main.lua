danmu = "test"

local MyMod = RegisterMod("test",1)

local color = KColor(1,0,0,0.5)

MyMod:AddCallback(ModCallbacks.MC_POST_RENDER, function()
	if danmuB then
		EID.font:DrawStringScaledUTF8("人气值："..tostring(danmuB.getPopularity()),80,200,1,1,color)

		danmuB.receive(function(text)
			danmu = text
		end)
		EID.font:DrawStringScaledUTF8(danmu,80,180,1,1,color)
	end

end)