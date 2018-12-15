	lui     $t0, 0x0040
	ori     $t0, 0x1000
	addiu   $t1, $0, 1
	sw      $t1, 0($t0)
	addiu   $t1, $0, 2
	sw      $t1, 4($t0)
	addiu   $t1, $0, 3
	sw      $t1, 8($t0)
	addiu   $t1, $0, 4
	sw      $t1, 12($t0)
	addiu   $t1, $0, 5
	sw      $t1, 16($t0)
    addi	$0,$0,0 #unsupported instruction, terminate

