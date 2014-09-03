function llvm_config(opt)
  --expect symlink of llvm-config to your config version (e.g llvm-config-3.4)
  local stream = assert(io.popen("llvm-config " .. opt))
  local output = ""
  --llvm-config contains '\n'
  while true do
    local curr = stream:read("*l")
    if curr == nil then
      break
    end
    output = output .. curr
  end
  stream:close()
  return output
end
