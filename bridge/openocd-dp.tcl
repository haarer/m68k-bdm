# Manual DP initialization script
# Try to read DP IDR
puts "Reading DPIDR..."
catch {set dpidr [dap dpreg 0x0]} err
puts "DPIDR: $dpidr (error: $err)"

# Read CTRL/STAT
puts "Reading CTRL/STAT..."
catch {set ctrlstat [dap dpreg 0x4]} err
puts "CTRL/STAT: $ctrlstat (error: $err)"

# Set power-up requests
puts "Setting power-up requests..."
catch {dap dpreg 0x4 0x50000000} err
puts "Set power-up: $err"

# Read back CTRL/STAT
catch {set ctrlstat [dap dpreg 0x4]} err
puts "CTRL/STAT after power-up: $ctrlstat (error: $err)"

# Try SYSRESETREQ
puts "Setting SYSRESETREQ..."
catch {dap dpreg 0x4 0x50000004} err
puts "SYSRESETREQ set: $err"

# Wait
sleep 100

# De-assert SYSRESETREQ
puts "De-asserting SYSRESETREQ..."
catch {dap dpreg 0x4 0x50000000} err
puts "SYSRESETREQ cleared: $err"

sleep 200

# Read back CTRL/STAT
catch {set ctrlstat [dap dpreg 0x4]} err
puts "CTRL/STAT after reset: $ctrlstat (error: $err)"

# Now try to examine
puts "Examining target..."
stm32f4x.cpu arp_examine
puts "Target examined: [stm32f4x.cpu curstate]"
