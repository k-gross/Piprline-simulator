	lw	0	1	data1	
	lw  0	2	data2
	beq 1	2	label
	add 2	4	2
label	nor 5	6	7
	halt
data1	.fill	5
data2	.fill	5