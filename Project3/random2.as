	beq	0	0	label
	nor	0	0	0
label	lw	0	1	data1
	beq	0	1	label
	lw	0	5	data2
	add	0	5	4
	add	4	5	4
	add	4	5	4
	halt
data1	.fill	1
data2	.fill	5