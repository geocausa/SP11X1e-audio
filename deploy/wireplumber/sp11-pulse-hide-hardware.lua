-- SP11 production desktop endpoint policy.
--
-- GNOME's output picker enumerates sinks through the pipewire-pulse bridge.
-- Keep the physical ALSA speaker available to native PipeWire/WirePlumber so
-- UbiG can target it, but remove read permission to that one node from the
-- bridge.  Pulse clients therefore see only the UbiG virtual sink.

local log = Log.open_topic("sp11-endpoint")

local clients = ObjectManager {
  Interest { type = "client" }
}

local nodes = ObjectManager {
  Interest { type = "node" }
}

local function is_pulse_bridge(client)
  local p = client["properties"]
  return p["config.name"] == "pipewire-pulse.conf"
     and p["application.process.binary"] == "pipewire"
     and p["client.api"] == nil
end

local function is_physical_speaker(node)
  return node["properties"]["node.name"] ==
    "alsa_output.platform-sound.HiFi__Speaker__sink"
end

local function apply_policy()
  for client in clients:iterate { type = "client" } do
    if is_pulse_bridge(client) then
      for node in nodes:iterate { type = "node" } do
        if is_physical_speaker(node) then
          local id = node["bound-id"]
          log:info(client, "hiding SP11 physical speaker from pipewire-pulse")
          client:update_permissions { [id] = "-" }
        end
      end
    end
  end
end

clients:connect("object-added", function () apply_policy() end)
nodes:connect("object-added", function () apply_policy() end)

clients:activate()
nodes:activate()
