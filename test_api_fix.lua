#!/usr/bin/env lua

-- Test script to verify the API command fix
-- This simulates the scenario where OSV_API is not set

-- Mock the context module to simulate missing API configuration
package.path = "/workspace/modules/cli/lib/?.lua;" .. package.path

-- Create a mock context with nil api
local context = {
  api = nil,  -- This simulates the missing OSV_API environment variable
  ssl_key = nil,
  ssl_cert = nil,
  ssl_cacert = nil,
  ssl_verify = nil
}

-- Mock the context module
package.loaded.context = context

-- Mock other required modules
package.loaded.json = {
  decode = function(str) return {} end,
  encode = function(obj) return "{}" end
}

package.loaded["socket.http"] = {
  request = function() return nil, 404 end
}

package.loaded.ssl = {}
package.loaded["socket.url"] = {
  escape = function(str) return str end
}
package.loaded.ltn12 = {
  sink = {
    table = function(t) return function(chunk) table.insert(t, chunk) end end
  }
}
package.loaded.socket = {
  try = function(x) return x end,
  tcp = function() return {} end
}

-- Mock utility functions
function split(str, pattern)
  local result = {}
  for match in string.gmatch(str, pattern) do
    table.insert(result, match)
  end
  return result
end

function file_exists(path)
  return false
end

-- Load the osv_api module
local osv_api = dofile("/workspace/modules/cli/lib/osv_api.lua")

-- Test the fix
print("Testing API command fix...")
print("Attempting to call osv_request with nil context.api...")

local success, err = pcall(function()
  osv_request({"os", "version"}, "GET")
end)

if success then
  print("ERROR: Expected an error but got success!")
else
  print("SUCCESS: Got expected error:")
  print("  " .. err)
  
  -- Check if the error message contains helpful information
  if string.find(err, "OSV_API") and string.find(err, "environment variable") then
    print("SUCCESS: Error message contains helpful configuration instructions")
  else
    print("WARNING: Error message might not be helpful enough")
  end
end

print("\nTesting osv_schema with error handling...")
local success2, err2 = pcall(function()
  osv_schema()
end)

if success2 then
  print("ERROR: Expected an error but got success!")
else
  print("SUCCESS: Got expected error:")
  print("  " .. err2)
end

print("\nTest completed!")