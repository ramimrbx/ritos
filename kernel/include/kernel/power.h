#ifndef KERNEL_POWER_H
#define KERNEL_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

void sys_reboot(void);
void sys_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
