-- scriptdemo/client.lua — runs INSIDE GTA V (client-side scripting, like FiveM)
-- This file is streamed by the server on join and executed by the game client.
mymp.print("[scriptdemo] client.lua loaded inside GTA V")

mymp.on("demo:hello", function(data)
    mymp.print("[scriptdemo] server says: " .. tostring(data.msg))
    mymp.send("demo:pong", { msg = "pong from GTA V" })
end)

mymp.on("chat", function(d)
    if d.msg == "!scriptdemo" then
        mymp.send("demo:triggered", { by = d.name })
        mymp.print("[scriptdemo] triggered by " .. tostring(d.name))
    end
end)
