# Kernel Oops Analysis

- On `echo "hello_world" > /dev/faulty`, an error occurred: `Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000`.
- Investigating the PC register, the program counter is at `faulty_write+0x10/0x20 [faulty]`.
- Checking the `faulty_write` function, a null pointer dereference occurs at `line 53`.
- The same result can be seen from the call trace:

```
Call trace:
 faulty_write+0x10/0x20 [faulty]
 ksys_write+0x74/0x110
 __arm64_sys_write+0x1c/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x2c/0xc0
 el0_svc+0x2c/0x90
 el0t_64_sync_handler+0xf4/0x120
 el0t_64_sync+0x18c/0x190
Code: d2800001 d2800000 d503233f d50323bf (b900003f)
---[ end trace 0000000000000000 ]---
```
