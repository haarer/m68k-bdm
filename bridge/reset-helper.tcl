proc sysreset_dap {dap_name} {
    puts "Powering up DP: ${dap_name} dpreg 0x4 0x50000000"
    catch {${dap_name} dpreg 0x4 0x50000000} err
    puts "Result: $err"
    sleep 20
    
    set cs [${dap_name} dpreg 0x4]
    puts "CTRL/STAT: $cs"
    
    # Assert SYSRESETREQ (bit 2)
    puts "Asserting SYSRESETREQ..."
    ${dap_name} dpreg 0x4 0x50000004
    sleep 100
    
    # De-assert
    puts "De-asserting SYSRESETREQ..."
    ${dap_name} dpreg 0x4 0x50000000
    sleep 200
    
    puts "Reset completed"
}
