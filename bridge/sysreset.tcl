proc sysresetreq {} {
    # Read DP CTRL/STAT register (DP register 0x4)
    set dp_ctrlstat [dap dpreg 0x4]
    echo "DP CTRL/STAT before: $dp_ctrlstat"
    
    # Set SYSRESETREQ bit (bit 2)
    set dp_ctrlstat [expr {$dp_ctrlstat | 0x04}]
    dap dpreg 0x4 $dp_ctrlstat
    echo "SYSRESETREQ asserted"
    
    # Wait a bit
    sleep 100
    
    # De-assert SYSRESETREQ
    set dp_ctrlstat [expr {$dp_ctrlstat & ~0x04}]
    dap dpreg 0x4 $dp_ctrlstat
    echo "SYSRESETREQ de-asserted"
    
    # Wait for reset to complete
    sleep 200
    
    # Read CTRL/STAT again
    set dp_ctrlstat [dap dpreg 0x4]
    echo "DP CTRL/STAT after: $dp_ctrlstat"
}

proc init_dp {} {
    # Read DPIDR (DP register 0x0)
    set dpidr [dap dpreg 0x0]
    echo "DPIDR: $dpidr"
    
    # Read CTRL/STAT
    set dp_ctrlstat [dap dpreg 0x4]
    echo "CTRL/STAT: $dp_ctrlstat"
    
    # Enable debug power domain - set CSYSPWRUPREQ (bit 28) and CDBGPWRUPREQ (bit 28 on AP)
    # Actually in DP CTRL/STAT, we need bit 28 (CDBGPWRUPREQ) and bit 30 (CSYSPWRUPREQ) for AP access
    set dp_ctrlstat [expr {$dp_ctrlstat | 0x50000000}]
    dap dpreg 0x4 $dp_ctrlstat
    echo "Power-up requests set"
    
    # Wait for power-up
    sleep 10
    
    # Read back
    set dp_ctrlstat [dap dpreg 0x4]
    echo "CTRL/STAT after power-up: $dp_ctrlstat"
    
    # Try to read AP 0 IDR
    # First select AP 0
    dap dpreg 0x8 0x00000000
    sleep 1
    
    # Now read AP 0 register 0x0 (IDR) via DP register 0xC (RDBUFF) or directly
    # APACC access: write AP register address to DP SELECT, then read via DP RDBUFF
    set apidr [dap apreg 0x0 0x0]
    echo "AP 0 IDR: $apidr"
}

proc read_mem_at {addr} {
    if {[catch {read_memory 0xe000ed00 32 1} result]} {
        echo "Failed to read memory at 0x$addr: $result"
    } else {
        echo "Memory at 0x$addr: $result"
    }
}
