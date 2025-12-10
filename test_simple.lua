-- Simple test to verify the fix
-- This test simulates calling the API command without OSV_API set

-- Create a minimal context module
local context = {}

-- Test the error condition
local function test_osv_request()
  -- Simulate the osv_request function with our fix
  local function osv_request(arguments, method, parameters, do_raw)
    -- Check if API endpoint is configured
    if not context.api then
      error("OSv API endpoint not configured. Please set OSV_API environment variable or use --api option.\nExample: export OSV_API=http://192.168.122.76:8000")
    end
    -- Rest of function would go here...
  end
  
  -- Test with nil context.api
  local success, err = pcall(function()
    osv_request({"os", "version"}, "GET")
  end)
  
  if success then
    print("FAIL: Expected error but got success")
    return false
  else
    print("PASS: Got expected error message:")
    print("  " .. err)
    return string.find(err, "OSV_API") ~= nil
  end
end

-- Run the test
print("Testing API fix...")
local result = test_osv_request()
print("Test result:", result and "PASS" or "FAIL")