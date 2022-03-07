
local json = require "json"

local Lobby = luiglobals.Lobby
local MPLobbyOnline = LUI.mp_menus.MPLobbyOnline

function LeaveLobby(f5_arg0)
    LeaveXboxLive()
    if Lobby.IsInPrivateParty() == false or Lobby.IsPrivatePartyHost() then
        LUI.FlowManager.RequestLeaveMenuByName("menu_xboxlive")
        Engine.ExecNow("clearcontrollermap")
    end
end


function menu_xboxlive(f16_arg0, f16_arg1)
    local menu = LUI.MPLobbyBase.new(f16_arg0, {
        menu_title = "@PLATFORM_UI_HEADER_PLAY_MP_CAPS",
        memberListState = Lobby.MemberListStates.Prelobby
    })

    menu:setClass(LUI.MPLobbyOnline)

    local serverListButton = menu:AddButton("Find Server", function(a1, a2)
        LUI.FlowManager.RequestAddMenu(a1, "menu_systemlink_join", true, nil)
    end)
    serverListButton:setDisabledRefreshRate(500)
    if Engine.IsCoreMode() then
        menu:AddCACButton()
      

    end

    serverListButton = menu:AddButton("@MENU_PRIVATE_MATCH", MPLobbyOnline.OnPrivateMatch,
        MPLobbyOnline.disablePrivateMatchButton)
    serverListButton:rename("menu_xboxlive_private_match")
    serverListButton:setDisabledRefreshRate(500)
    if not Engine.IsCoreMode() then
        local leaderboardButton = menu:AddButton("@LUA_MENU_LEADERBOARD", "OpLeaderboardMain")
        leaderboardButton:rename("OperatorMenu_leaderboard")
    end

    menu:AddOptionsButton()
    local natType = Lobby.GetNATType()
    if natType then
        local natTypeText = Engine.Localize("NETWORK_YOURNATTYPE", natType)
        local properties = CoD.CreateState(nil, nil, 2, -62, CoD.AnchorTypes.BottomRight)
        properties.width = 250
        properties.height = CoD.TextSettings.BodyFont.Height
        properties.font = CoD.TextSettings.BodyFont.Font
        properties.color = luiglobals.Colors.white
        properties.alpha = 0.25
        local self = LUI.UIText.new(properties)
        self:setText(natTypeText)
        menu:addElement(self)
    end

    menu.isSignInMenu = true
    menu:registerEventHandler("gain_focus", LUI.MPLobbyOnline.OnGainFocus)
    menu:registerEventHandler("player_joined", luiglobals.Cac.PlayerJoinedEvent)
    menu:registerEventHandler("exit_live_lobby", LeaveLobby)

    if Engine.IsCoreMode() then
        Engine.ExecNow("eliteclan_refresh", Engine.GetFirstActiveController())
    end

    local modsList = menu:AddButton("Mod Loader", function(a1, a2)
        LUI.FlowManager.RequestAddMenu(f16_arg0, "menu_script", true)
    end)
    modsList:setDisabledRefreshRate(500)

    return menu
end

function OnBackButton(f1_arg0, f1_arg1)
    LUI.FlowManager.RequestLeaveMenu(f1_arg0)
end

function getmodname(path)
    local name = path
    local desc = "Load " .. name .. "\n"
    local infofile = path .. "/info.json"

    if (io.fileexists(infofile)) then
        pcall(function()
            local data = json.decode(io.readfile(infofile))
            desc = string.format("Author: %s | Version: %s | %s", 
                 data.author, data.version, data.desc)
            name = data.name
        end)
    end

    return name, desc
end

function createdivider(menu, text, action)
	local element = LUI.UIElement.new( {
		leftAnchor = true,
		rightAnchor = true,
		left = 0,
		right = 0,
		topAnchor = true,
		bottomAnchor = false,
		top = 0,
		bottom = 66.33
	})

	element.scrollingToNext = false
	element:addElement(LUI.MenuBuilder.BuildRegisteredType("UIGenericButton", {
        style = LUI.MenuTemplate.ButtonStyle,

		title_bar_text = Engine.ToUpperCase(Engine.Localize(text)),
		button_text = Engine.ToUpperCase(Engine.Localize(text)),
        button_action_func = action,
        text_align_without_content = LUI.Alignment.Right,
        use_locking = nil,
        disabled = nil,
        color = {0,0,1,1}
	}))

	menu:addElement(element)
end


LUI.MenuBuilder.m_types_build["menu_script"] = function(f16_arg0)
    local menu = LUI.MenuTemplate.new(f16_arg0, {
        menu_title = "Mod Loader",
        exclusiveController = 0,
        menu_width = 450,
        topAnchor = true,
        bottomAnchor = false,
        leftAnchor = true,
        rightAnchor = false,
        width = 10.000000,

    })

    menu:setClass(LUI.Options)
    menu:AddBackButton(OnBackButton)

    createdivider(menu, "s1x/mods")
     local modfolder = game:getloadedmod()
    if (modfolder ~= "") then
        local name, desc = getmodname(modfolder)
        menu:AddButtonWithSubText("^1UNLOAD", function()
            game:executecommand("unloadmod")
        end, nil, "".. name)
    end      

    if (io.directoryexists("s1x")) then
        if (io.directoryexists("s1x/mods")) then
            local mods = io.listfiles("s1x/mods")

            for i = 1, #mods do
                if (io.directoryexists(mods[i]) ) then
                    local name, desc = getmodname(mods[i])

                    if (mods[i] ~= modfolder) then
                         menu:AddButtonWithSubText("$_" .. name, function()
                            game:executecommand("loadmod " .. mods[i])
                            menu:addspacing(5)
                        end, nil, desc)

                    end
                end
            end
        else
            io.createdirectory("s1x/mods")
            menu:AddButton("(s1x/mods) Directory Created!")
        end
    else
        io.createdirectory("s1x");

    end

    menu:InitScrolling(menu.list, menu.list)


    return menu
end

LUI.MenuBuilder.m_types_build["menu_xboxlive"] = menu_xboxlive

local localize = Engine.Localize
Engine.Localize = function(...)
    local args = {...}

    if (type(args[1]) == "string" and args[1]:sub(1, 2) == "$_") then
        return args[1]:sub(3, -1)
    end

    return localize(table.unpack(args))
end
