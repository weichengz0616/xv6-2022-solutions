# Context switch
#
#   void swtch(struct context *old, struct context *new);
# 
# Save current registers in old. Load from new.	

# 为什么只保存callee-saved???
# 调用swtch的函数自己会保存caller-saved到栈上, swtch还会返回
# swtch本质上就是修改了一堆寄存器, 尤其注意ra, sp
# ra: 由call指令修改, ret指令根据ra跳转. => swtch修改了ra, 因此ret指令不会返回到调用swtch的函数里
# sp: 栈也被改变了


# 它和trapframe区别是什么???
# 区别就是uservec是直接被cpu设置的, 返回指令是sret, trapframe需要保存全部寄存器
# swtch是被函数调用(call指令)的, 返回指令是ret, 还会返回原来的栈上, 因此context只保存callee寄存器
# (所谓caller, callee只是个约定, 保存和恢复操作实际上是遵循这个约定的编译器生成的代码)

.globl swtch
swtch:
        sd ra, 0(a0)
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        ld ra, 0(a1)
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)
        
        ret

	
