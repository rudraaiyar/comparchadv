	lui	$t0, 0x1001
	ori	$t0, $t0, 0x0
	addiu	$t1, $0 5
	sw	$t1, 0($t0)
    addi	$0,$0,0 #unsupported instruction, terminate
