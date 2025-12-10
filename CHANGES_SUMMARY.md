# Fix for "bad argument #1 to sub (string expected, got nil)" Error

## Problem Description
The OSv CLI `api` command was crashing with the error "bad argument #1 to sub (string expected, got nil)" when the OSV_API environment variable was not set or the --api command line option was not provided.

## Root Cause
1. The `osv_request()` function in `/workspace/modules/cli/lib/osv_api.lua` was not checking if `context.api` was nil before attempting to use it
2. The `osv_schema()` function was calling `string.sub()` on potentially nil values returned from failed API requests
3. When `context.api` is nil, HTTP requests fail and return nil responses, which were then passed to `string.sub()` causing the crash

## Files Modified

### 1. `/workspace/modules/cli/lib/osv_api.lua`

#### Change 1: Added API endpoint validation in `osv_request()` function
**Lines 144-153**: Added check for `context.api` before attempting HTTP requests
```lua
-- Check if API endpoint is configured
if not context.api then
  error("OSv API endpoint not configured. Please set OSV_API environment variable or use --api option.\nExample: export OSV_API=http://192.168.122.76:8000")
end
```

#### Change 2: Added defensive checks in `osv_schema()` function
**Lines 58-63**: Added null check before calling `string.sub()` on `resource.definition.resourcePath`
```lua
-- Defensive check: ensure resourcePath exists before calling string.sub
if resource.definition and resource.definition.resourcePath then
  resource.cli_alias = string.sub(resource.definition.resourcePath, 2)
else
  resource.cli_alias = resource.path or "unknown"
end
```

**Lines 65-72**: Added null check before calling `string.sub()` on `api.path`
```lua
for _, api in ipairs(resource.definition.apis or {}) do
  -- Defensive check: ensure api.path exists before calling string.sub
  if api.path then
    api.cli_alias = split(string.sub(api.path, 2), "[^/]+")
  else
    api.cli_alias = {"unknown"}
  end
end
```

### 2. `/workspace/modules/cli/commands/api.lua`

#### Change 3: Added error handling for schema loading
**Lines 152-161**: Wrapped `osv_schema()` call in `pcall()` to catch and display configuration errors gracefully
```lua
-- Load the schema with error handling
local schema
local success, err = pcall(function()
  schema = osv_schema()
end)

if not success then
  io.stderr:write("Error: ", err, "\n")
  return
end
```

## Expected Behavior After Fix

### Before Fix
```bash
/# api
api: bad argument #1 to sub (string expected, got nil)
/# api os version
api: bad argument #1 to sub (string expected, got nil)
```

### After Fix
```bash
/# api
Error: OSv API endpoint not configured. Please set OSV_API environment variable or use --api option.
Example: export OSV_API=http://192.168.122.76:8000

/# api os version
Error: OSv API endpoint not configured. Please set OSV_API environment variable or use --api option.
Example: export OSV_API=http://192.168.122.76:8000
```

### With Proper Configuration
```bash
/# export OSV_API=http://192.168.122.76:8000
/# api os version
# Should work as expected, showing OS version information
```

## Testing
- Created test scripts to verify the fix handles nil `context.api` correctly
- Verified that helpful error messages are displayed instead of Lua crashes
- Ensured that existing functionality remains intact when API endpoint is properly configured

## Backward Compatibility
- All changes are backward compatible
- Existing error handling patterns are preserved
- No changes to the API or command line interface
- SSL configuration and other features remain unchanged

## Security Considerations
- No security implications from these changes
- Error messages do not expose sensitive information
- Input validation is improved, not weakened