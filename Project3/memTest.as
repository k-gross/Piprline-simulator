	lw	0	1	data1	
	lw  0	2	data2
	beq 1	2	label
	add 2	4	2
label	nor 5	6	7
	sw	0	2	4
	lw	0	3	data3
	beq	1	3	label
	lw	0	1	data3
	halt
data1	.fill	5
data2	.fill	5
data3	.fill	324324