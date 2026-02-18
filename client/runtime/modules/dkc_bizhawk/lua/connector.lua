--[[
Copyright (c) 2023 Zunawe

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
]]

local SCRIPT_VERSION = 1

-- Set to log incoming requests
-- Will cause lag due to large console output
local DEBUG = false

--[[
This script expects to receive JSON and will send JSON back. A message should
be a list of 1 or more requests which will be executed in order. Each request
will have a corresponding response in the same order.

Every individual request and response is a JSON object with at minimum one
field `type`. The value of `type` determines what other fields may exist.

To get the script version, instead of JSON, send "VERSION" to get the script
version directly (e.g. "2").

#### Ex. 1

Request: `[{"type": "PING"}]`

Response: `[{"type": "PONG"}]`

---

#### Ex. 2

Request: `[{"type": "LOCK"}, {"type": "HASH"}]`

Response: `[{"type": "LOCKED"}, {"type": "HASH_RESPONSE", "value": "F7D18982"}]`

---

#### Ex. 3

Request:

```json
[
    {"type": "GUARD", "address": 100, "expected_data": "aGVsbG8=", "domain": "System Bus"},
    {"type": "READ", "address": 500, "size": 4, "domain": "ROM"}
]
```

Response:

```json
[
    {"type": "GUARD_RESPONSE", "address": 100, "value": true},
    {"type": "READ_RESPONSE", "value": "dGVzdA=="}
]
```

---

#### Ex. 4

Request:

```json
[
    {"type": "GUARD", "address": 100, "expected_data": "aGVsbG8=", "domain": "System Bus"},
    {"type": "READ", "address": 500, "size": 4, "domain": "ROM"}
]
```

Response:

```json
[
    {"type": "GUARD_RESPONSE", "address": 100, "value": false},
    {"type": "GUARD_RESPONSE", "address": 100, "value": false}
]
```

---

### Supported Request Types

- `PING`  
    Does nothing; resets timeout.

    Expected Response Type: `PONG`

- `SYSTEM`  
    Returns the system of the currently loaded ROM (N64, GBA, etc...).

    Expected Response Type: `SYSTEM_RESPONSE`

- `PREFERRED_CORES`  
    Returns the user's default cores for systems with multiple cores. If the
    current ROM's system has multiple cores, the one that is currently
    running is very probably the preferred core.

    Expected Response Type: `PREFERRED_CORES_RESPONSE`

- `HASH`  
    Returns the hash of the currently loaded ROM calculated by BizHawk.

    Expected Response Type: `HASH_RESPONSE`

- `MEMORY_SIZE`  
    Returns the size in bytes of the specified memory domain.

    Expected Response Type: `MEMORY_SIZE_RESPONSE`

    Additional Fields:
    - `domain` (`string`): The name of the memory domain to check

- `GUARD`  
    Checks a section of memory against `expected_data`. If the bytes starting
    at `address` do not match `expected_data`, the response will have `value`
    set to `false`, and all subsequent requests will not be executed and
    receive the same `GUARD_RESPONSE`.

    Expected Response Type: `GUARD_RESPONSE`

    Additional Fields:
    - `address` (`int`): The address of the memory to check
    - `expected_data` (string): A base64 string of contiguous data
    - `domain` (`string`): The name of the memory domain the address
    corresponds to

- `LOCK`  
    Halts emulation and blocks on incoming requests until an `UNLOCK` request
    is received or the client times out. All requests processed while locked
    will happen on the same frame.

    Expected Response Type: `LOCKED`

- `UNLOCK`  
    Resumes emulation after the current list of requests is done being
    executed.

    Expected Response Type: `UNLOCKED`

- `READ`  
    Reads an array of bytes at the provided address.

    Expected Response Type: `READ_RESPONSE`

    Additional Fields:
    - `address` (`int`): The address of the memory to read
    - `size` (`int`): The number of bytes to read
    - `domain` (`string`): The name of the memory domain the address
    corresponds to

- `WRITE`  
    Writes an array of bytes to the provided address.

    Expected Response Type: `WRITE_RESPONSE`

    Additional Fields:
    - `address` (`int`): The address of the memory to write to
    - `value` (`string`): A base64 string representing the data to write
    - `domain` (`string`): The name of the memory domain the address
    corresponds to

- `DISPLAY_MESSAGE`  
    Adds a message to the message queue which will be displayed using
    `gui.addmessage` according to the message interval.

    Expected Response Type: `DISPLAY_MESSAGE_RESPONSE`

    Additional Fields:
    - `message` (`string`): The string to display

- `SET_MESSAGE_INTERVAL`  
    Sets the minimum amount of time to wait between displaying messages.
    Potentially useful if you add many messages quickly but want players
    to be able to read each of them.

    Expected Response Type: `SET_MESSAGE_INTERVAL_RESPONSE`

    Additional Fields:
    - `value` (`number`): The number of seconds to set the interval to


### Response Types

- `PONG`  
    Acknowledges `PING`.

- `SYSTEM_RESPONSE`  
    Contains the name of the system for currently running ROM.

    Additional Fields:
    - `value` (`string`): The returned system name

- `PREFERRED_CORES_RESPONSE`  
    Contains the user's preferred cores for systems with multiple supported
    cores. Currently includes NES, SNES, GB, GBC, DGB, SGB, PCE, PCECD, and
    SGX.

    Additional Fields:
    - `value` (`{[string]: [string]}`): A dictionary map from system name to
    core name

- `HASH_RESPONSE`  
    Contains the hash of the currently loaded ROM calculated by BizHawk.

    Additional Fields:
    - `value` (`string`): The returned hash

- `MEMORY_SIZE_RESPONSE`  
    Contains the size in bytes of the specified memory domain.

    Additional Fields:
    - `value` (`number`): The size of the domain in bytes

- `GUARD_RESPONSE`  
    The result of an attempted `GUARD` request.

    Additional Fields:
    - `value` (`boolean`): true if the memory was validated, false if not
    - `address` (`int`): The address of the memory that was invalid (the same
    address provided by the `GUARD`, not the address of the individual invalid
    byte)

- `LOCKED`  
    Acknowledges `LOCK`.

- `UNLOCKED`  
    Acknowledges `UNLOCK`.

- `READ_RESPONSE`  
    Contains the result of a `READ` request.

    Additional Fields:
    - `value` (`string`): A base64 string representing the read data

- `WRITE_RESPONSE`  
    Acknowledges `WRITE`.

- `DISPLAY_MESSAGE_RESPONSE`  
    Acknowledges `DISPLAY_MESSAGE`.

- `SET_MESSAGE_INTERVAL_RESPONSE`  
    Acknowledges `SET_MESSAGE_INTERVAL`.

- `ERROR`  
    Signifies that something has gone wrong while processing a request.

    Additional Fields:
    - `err` (`string`): A description of the problem
]]

local bizhawk_version = client.getversion()
local bizhawk_major, bizhawk_minor, bizhawk_patch = bizhawk_version:match("(%d+)%.(%d+)%.?(%d*)")
bizhawk_major = tonumber(bizhawk_major)
bizhawk_minor = tonumber(bizhawk_minor)
if bizhawk_patch == "" then
    bizhawk_patch = 0
else
    bizhawk_patch = tonumber(bizhawk_patch)
end

local lua_major, lua_minor = _VERSION:match("Lua (%d+)%.(%d+)")
lua_major = tonumber(lua_major)
lua_minor = tonumber(lua_minor)

if lua_major > 5 or (lua_major == 5 and lua_minor >= 3) then
    require("lua_5_3_compat")
end

local base64 = require("base64")
local socket = require("socket")
local json = require("json")

local SOCKET_PORT_FIRST = 43055
local SOCKET_PORT_RANGE_SIZE = 5
local SOCKET_PORT_LAST = SOCKET_PORT_FIRST + SOCKET_PORT_RANGE_SIZE

local STATE_NOT_CONNECTED = 0
local STATE_CONNECTED = 1

local server = nil
local client_socket = nil

local current_state = STATE_NOT_CONNECTED

local timeout_timer = 0
local message_timer = 0
local message_interval = 0
local prev_time = 0
local current_time = 0

local locked = false

local rom_hash = nil
local overlay_status = "Waiting for SekaiLink client..."
local overlay_last_error = ""
local overlay_messages = {}

local OVERLAY_MARGIN_X = 6
local OVERLAY_MARGIN_Y = 8
local OVERLAY_LINE_HEIGHT = 16
local OVERLAY_FONT_SIZE = 14
local OVERLAY_MAX_MESSAGES = 5
local OVERLAY_MESSAGE_TTL_SECONDS = 7
local OVERLAY_FADE_SECONDS = 2.5
local OVERLAY_TOGGLE_KEY = "Insert"
local OVERLAY_TOGGLE_HINT = "Shift+Insert"
local OVERLAY_SHIFT_KEYS = {"ShiftKey", "LShiftKey", "RShiftKey"}
local overlay_visible = true
local overlay_hotkey_was_down = false

function queue_push (self, value)
    self[self.right] = value
    self.right = self.right + 1
end

function queue_is_empty (self)
    return self.right == self.left
end

function queue_shift (self)
    value = self[self.left]
    self[self.left] = nil
    self.left = self.left + 1
    return value
end

function new_queue ()
    local queue = {left = 1, right = 1}
    return setmetatable(queue, {__index = {is_empty = queue_is_empty, push = queue_push, shift = queue_shift}})
end

local message_queue = new_queue()

function overlay_text_max_chars ()
    local width = client.screenwidth()
    local max_chars = math.floor((width - (OVERLAY_MARGIN_X * 2)) / 8)
    if max_chars < 24 then
        max_chars = 24
    end
    return max_chars
end

function overlay_trim_text (value, max_chars)
    local text = tostring(value or "")
    if #text <= max_chars then
        return text
    end
    return text:sub(1, max_chars - 3).."..."
end

function overlay_alpha_color (rgb, alpha_factor)
    local factor = alpha_factor or 1
    if factor < 0 then
        factor = 0
    elseif factor > 1 then
        factor = 1
    end

    local alpha = math.floor(255 * factor)
    return alpha * 0x1000000 + rgb
end

function overlay_message_rgb (message)
    local lower = string.lower(tostring(message or ""))
    if string.find(lower, "[error]", 1, true) ~= nil or string.find(lower, "error", 1, true) ~= nil then
        return 0x00FF7A7A
    end
    if string.find(lower, "[warn]", 1, true) ~= nil or string.find(lower, "warning", 1, true) ~= nil then
        return 0x00FFD166
    end
    if string.find(lower, "[info]", 1, true) ~= nil then
        return 0x008FD3FF
    end
    return 0x00F2F2F2
end

function overlay_prune_messages (now)
    local kept = {}
    for i = 1, #overlay_messages do
        local item = overlay_messages[i]
        if item ~= nil and item["expires_at"] ~= nil and item["expires_at"] > now then
            table.insert(kept, item)
        end
    end
    overlay_messages = kept
end

function overlay_push_message (message)
    local normalized = tostring(message or "")
    if normalized == "" then
        return
    end

    local now = socket.socket.gettime()
    table.insert(
        overlay_messages,
        {
            ["text"] = normalized,
            ["expires_at"] = now + OVERLAY_MESSAGE_TTL_SECONDS,
        }
    )
    while #overlay_messages > OVERLAY_MAX_MESSAGES do
        table.remove(overlay_messages, 1)
    end

    local lower = string.lower(normalized)
    if string.find(lower, "error", 1, true) ~= nil then
        overlay_last_error = normalized
    end
end

function overlay_set_status (message)
    overlay_status = tostring(message or "")
end

function overlay_update_hotkey ()
    local pressed = false
    local ok, keys = pcall(input.get)

    local function key_is_down (name)
        return keys[name] ~= nil and keys[name] ~= false
    end

    local function any_key_down (names)
        for i = 1, #names do
            if key_is_down(names[i]) then
                return true
            end
        end
        return false
    end

    if ok and type(keys) == "table" then
        pressed = any_key_down(OVERLAY_SHIFT_KEYS) and key_is_down(OVERLAY_TOGGLE_KEY)
    end

    if pressed and not overlay_hotkey_was_down then
        overlay_visible = not overlay_visible
        if overlay_visible then
            overlay_push_message("[info] Overlay visible ("..OVERLAY_TOGGLE_HINT.." to hide)")
        end
    end
    overlay_hotkey_was_down = pressed
end

function overlay_draw_text (x, y, text, color, background)
    local ok = pcall(
        gui.drawText,
        x,
        y,
        text,
        color,
        background or 0xB0000000,
        OVERLAY_FONT_SIZE,
        "Courier New",
        "middle",
        "bottom",
        nil,
        "client"
    )
    if not ok then
        if type(color) == "number" and color < 0x55000000 then
            return
        end
        gui.text(x, y, text, color, "black")
    end
end

function overlay_draw ()
    if not overlay_visible then
        return
    end

    local now = socket.socket.gettime()
    overlay_prune_messages(now)

    local max_chars = overlay_text_max_chars()
    local bottom_y = client.screenheight() - OVERLAY_MARGIN_Y - OVERLAY_LINE_HEIGHT
    local messages_drawn = 0

    for i = #overlay_messages, 1, -1 do
        if messages_drawn >= OVERLAY_MAX_MESSAGES then
            break
        end

        local item = overlay_messages[i]
        local remaining = item["expires_at"] - now
        if remaining > 0 then
            local fade_factor = 1
            if remaining < OVERLAY_FADE_SECONDS then
                fade_factor = remaining / OVERLAY_FADE_SECONDS
            end

            local message_y = bottom_y - (messages_drawn * OVERLAY_LINE_HEIGHT)
            local color = overlay_alpha_color(overlay_message_rgb(item["text"]), fade_factor)
            local bg = overlay_alpha_color(0x00000000, 0.72 * fade_factor)
            overlay_draw_text(
                OVERLAY_MARGIN_X,
                message_y,
                overlay_trim_text(item["text"], max_chars),
                color,
                bg
            )
            messages_drawn = messages_drawn + 1
        end
    end

    local status_y = bottom_y - (messages_drawn * OVERLAY_LINE_HEIGHT) - OVERLAY_LINE_HEIGHT
    if current_state == STATE_CONNECTED then
        overlay_draw_text(OVERLAY_MARGIN_X, status_y, "Client: connected", "green")
    else
        overlay_draw_text(OVERLAY_MARGIN_X, status_y, "Client: disconnected", "orange")
    end

    status_y = status_y - OVERLAY_LINE_HEIGHT
    overlay_draw_text(OVERLAY_MARGIN_X, status_y, overlay_trim_text("State: "..overlay_status, max_chars), "white")

    status_y = status_y - OVERLAY_LINE_HEIGHT
    if overlay_last_error ~= "" then
        overlay_draw_text(
            OVERLAY_MARGIN_X,
            status_y,
            overlay_trim_text("Last error: "..overlay_last_error, max_chars),
            "red"
        )
    end
end

function lock ()
    locked = true
    client_socket:settimeout(2)
end

function unlock ()
    locked = false
    client_socket:settimeout(0)
end

request_handlers = {
    ["PING"] = function (req)
        local res = {}

        res["type"] = "PONG"

        return res
    end,

    ["SYSTEM"] = function (req)
        local res = {}

        res["type"] = "SYSTEM_RESPONSE"
        res["value"] = emu.getsystemid()

        return res
    end,

    ["PREFERRED_CORES"] = function (req)
        local res = {}
        local preferred_cores = client.getconfig().PreferredCores
        local systems_enumerator = preferred_cores.Keys:GetEnumerator()

        res["type"] = "PREFERRED_CORES_RESPONSE"
        res["value"] = {}

        while systems_enumerator:MoveNext() do
            res["value"][systems_enumerator.Current] = preferred_cores[systems_enumerator.Current]
        end

        return res
    end,

    ["HASH"] = function (req)
        local res = {}

        res["type"] = "HASH_RESPONSE"
        res["value"] = rom_hash

        return res
    end,

    ["MEMORY_SIZE"] = function (req)
        local res = {}

        res["type"] = "MEMORY_SIZE_RESPONSE"
        res["value"] = memory.getmemorydomainsize(req["domain"])

        return res
    end,

    ["GUARD"] = function (req)
        local res = {}
        local expected_data = base64.decode(req["expected_data"])
        local actual_data = memory.read_bytes_as_array(req["address"], #expected_data, req["domain"])

        local data_is_validated = true
        for i, byte in ipairs(actual_data) do
            if byte ~= expected_data[i] then
                data_is_validated = false
                break
            end
        end

        res["type"] = "GUARD_RESPONSE"
        res["value"] = data_is_validated
        res["address"] = req["address"]

        return res
    end,

    ["LOCK"] = function (req)
        local res = {}

        res["type"] = "LOCKED"
        lock()

        return res
    end,

    ["UNLOCK"] = function (req)
        local res = {}

        res["type"] = "UNLOCKED"
        unlock()

        return res
    end,

    ["READ"] = function (req)
        local res = {}

        res["type"] = "READ_RESPONSE"
        res["value"] = base64.encode(memory.read_bytes_as_array(req["address"], req["size"], req["domain"]))

        return res
    end,

    ["WRITE"] = function (req)
        local res = {}

        res["type"] = "WRITE_RESPONSE"
        memory.write_bytes_as_array(req["address"], base64.decode(req["value"]), req["domain"])

        return res
    end,

    ["DISPLAY_MESSAGE"] = function (req)
        local res = {}

        res["type"] = "DISPLAY_MESSAGE_RESPONSE"
        message_queue:push(req["message"])

        return res
    end,

    ["SET_MESSAGE_INTERVAL"] = function (req)
        local res = {}

        res["type"] = "SET_MESSAGE_INTERVAL_RESPONSE"
        message_interval = req["value"]

        return res
    end,

    ["default"] = function (req)
        local res = {}

        res["type"] = "ERROR"
        res["err"] = "Unknown command: "..req["type"]

        return res
    end,
}

function process_request (req)
    if request_handlers[req["type"]] then
        return request_handlers[req["type"]](req)
    else
        return request_handlers["default"](req)
    end
end

-- Receive data from AP client and send message back
function send_receive ()
    local message, err = client_socket:receive()

    -- Handle errors
    if err == "closed" then
        if current_state == STATE_CONNECTED then
            overlay_set_status("Connection closed")
            overlay_push_message("[warn] Connection to client closed")
        end
        current_state = STATE_NOT_CONNECTED
        return
    elseif err == "timeout" then
        unlock()
        return
    elseif err ~= nil then
        overlay_set_status("Connection error")
        overlay_last_error = tostring(err)
        overlay_push_message("[error] "..tostring(err))
        current_state = STATE_NOT_CONNECTED
        unlock()
        return
    end

    -- Reset timeout timer
    timeout_timer = 5

    -- Process received data
    if DEBUG then
        print("Received Message ["..emu.framecount().."]: "..'"'..message..'"')
    end

    if message == "VERSION" then
        client_socket:send(tostring(SCRIPT_VERSION).."\n")
    else
        local res = {}
        local data = json.decode(message)
        local failed_guard_response = nil
        for i, req in ipairs(data) do
            if failed_guard_response ~= nil then
                res[i] = failed_guard_response
            else
                -- An error is more likely to cause an NLua exception than to return an error here
                local status, response = pcall(process_request, req)
                if status then
                    res[i] = response

                    -- If the GUARD validation failed, skip the remaining commands
                    if response["type"] == "GUARD_RESPONSE" and not response["value"] then
                        failed_guard_response = response
                    end
                else
                    if type(response) ~= "string" then response = "Unknown error" end
                    res[i] = {type = "ERROR", err = response}
                end
            end
        end

        client_socket:send(json.encode(res).."\n")
    end
end

function initialize_server ()
    local err
    local port = SOCKET_PORT_FIRST
    local res = nil

    server, err = socket.socket.tcp4()
    while res == nil and port <= SOCKET_PORT_LAST do
        res, err = server:bind("localhost", port)
        if res == nil and err ~= "address already in use" then
            print(err)
            return
        end

        if res == nil then
            port = port + 1
        end
    end

    if port > SOCKET_PORT_LAST then
        print("Too many instances of connector script already running. Exiting.")
        return
    end

    res, err = server:listen(0)

    if err ~= nil then
        print(err)
        return
    end

    server:settimeout(0)
end

function main ()
    while true do
        if server == nil then
            initialize_server()
        end

        current_time = socket.socket.gettime()
        timeout_timer = timeout_timer - (current_time - prev_time)
        message_timer = message_timer - (current_time - prev_time)
        prev_time = current_time

        if message_timer <= 0 and not message_queue:is_empty() then
            overlay_push_message(message_queue:shift())
            message_timer = message_interval
        end

        if current_state == STATE_NOT_CONNECTED then
            overlay_set_status("Looking for client...")
            if emu.framecount() % 30 == 0 then
                local client, timeout = server:accept()
                if timeout == nil then
                    overlay_set_status("Connected to SekaiLink")
                    overlay_push_message("[info] Connected to SekaiLink")
                    current_state = STATE_CONNECTED
                    client_socket = client
                    server:close()
                    server = nil
                    client_socket:settimeout(0)
                end
            end
        else
            repeat
                send_receive()
            until not locked

            if timeout_timer <= 0 then
                overlay_set_status("Disconnected from SekaiLink")
                overlay_push_message("[warn] Client timed out")
                current_state = STATE_NOT_CONNECTED
            end
        end

        overlay_update_hotkey()
        overlay_draw()
        coroutine.yield()
    end
end

event.onexit(function ()
    print("\n-- Restarting Script --\n")
    if server ~= nil then
        server:close()
    end
end)

if bizhawk_major < 2 or (bizhawk_major == 2 and bizhawk_minor < 7) then
    print("Must use BizHawk 2.7.0 or newer")
else
    if bizhawk_major > 2 or (bizhawk_major == 2 and bizhawk_minor > 10) then
        print("Warning: This version of BizHawk is newer than this script. If it doesn't work, consider downgrading to 2.10.")
    end

    if emu.getsystemid() == "NULL" then
        print("No ROM is loaded. Please load a ROM.")
        while emu.getsystemid() == "NULL" do
            emu.frameadvance()
        end
    end

    rom_hash = gameinfo.getromhash()

    overlay_push_message("[info] "..OVERLAY_TOGGLE_HINT.." toggles overlay")
    print("Waiting for client to connect. This may take longer the more instances of this script you have open at once.\n")

    local co = coroutine.create(main)
    function tick ()
        local status, err = coroutine.resume(co)

        if not status and err ~= "cannot resume dead coroutine" then
            print("\nERROR: "..err)
            print("Consider reporting this crash.\n")
    
            if server ~= nil then
                server:close()
            end

            co = coroutine.create(main)
        end
    end

    -- Gambatte has a setting which can cause script execution to become
    -- misaligned, so for GB and GBC we explicitly set the callback on
    -- vblank instead.
    -- https://github.com/TASEmulators/BizHawk/issues/3711
    if emu.getsystemid() == "GB" or emu.getsystemid() == "GBC" or emu.getsystemid() == "SGB" then
        event.onmemoryexecute(tick, 0x40, "tick", "System Bus")
    else
        event.onframeend(tick)
    end

    while true do
        emu.frameadvance()
    end
end
