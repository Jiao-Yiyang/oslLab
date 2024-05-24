.code16
.text
.global _start
_start:
  cli            # clear interuption
  xorw %ax, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %ss
  inb $0x92, %al # Fast setup A20 Line with port 0x92, necessary or not?
  orb $0x02, %al
  outb %al, $0x92
  lgdt gdt_desc  # loading gdt

  # TODO: Lab1-2, set the lowest bit of cr0
  movl %cr0, %eax
  orl $0x1, %eax
  movl %eax, %cr0
  ljmp $0x08, $start32 # reload code segment selector and ljmp to start32

.code32
start32:
  movw $0x10, %ax      # setting data segment selector
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %fs
  movw %ax, %ss

  movl $0x1ffffc, %esp # setting esp

  call load_kernel     # call load_kernel in boot.c

.L0:
  jmp .L0              # should never come back

.p2align 2
gdt: # 8 bytes for each table entry, at least 1 entry
  # .word limit[15:0], base[15:0]
  # .byte base[23:16], (0x90|(type)), (0xc0|(limit[19:16])), base[31:24]

  # empty entry
  .word 0, 0
  .byte 0, 0, 0, 0

  # TODO: Lab1-2, code segment entry
  code_seg:
    .word 0xffff # limit (set to maximum)
    .word 0x0    # base (set to 0)
    .byte 0x0    # base
    .byte 0x9a   # type (execute/read, accessed)
    .byte 0xcf   # limit (granularity set to 4KB, 32-bit mode)
    .byte 0x0    # base
  # TODO: Lab1-2, data segment entry
  data_seg:
    .word 0xffff # limit (set to maximum)
    .word 0x0    # base (set to 0)
    .byte 0x0    # base
    .byte 0x92   # type (read/write, accessed)
    .byte 0xcf   # limit (granularity set to 4KB, 32-bit mode)
    .byte 0x0    # base
gdt_desc: # 6 bytes in total
  .word (gdt_desc - gdt - 1) # sizeof(gdt) - 1
  .long gdt # offset, i.e. linear address of the table itself
