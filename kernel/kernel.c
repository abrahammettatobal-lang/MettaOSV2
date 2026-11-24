/* MettaOS — secuencia de bring-up en kmain (SA-01 / SA-04+). */
#include "block.h"
#include "cpu/cpu.h"
#include "fs/vfs.h"
#include "graphics/fb.h"
#include <stdint.h>
#include "interrupt/irq.h"
#include "interrupt/pic.h"
#include "interrupt/pit.h"
#include "log/klog.h"
#include "log/panic.h"
#include "metta/boot_info.h"
#include "metta/pmm.h"
#include "metta/validate.h"
#include "metta/vmm.h"
#include "proc/scheduler.h"
#include "bootsplash.h"
#include "ata_data.h"
#include "kernel.h"
#include "fs/mfs/mfs_vfs.h"
#include "serial.h"
#include "virt_disk.h"
#include "linux/user_files.h"

#include "mde.h"

#ifndef METTA_BRINGUP_SCHEDULER
#define METTA_BRINGUP_SCHEDULER 1
#endif

const void *boot_multiboot_info_raw(void);
metta_status_t mfs_vfs_kmain_stub(void);

/*
 * Sonda VMM opcional (SA-11 / SA-08). Exportar desde VMM con la misma firma;
 * símbolo débil: si no enlaza, el puntero queda NULL.
 */
metta_status_t vmm_probe_user_access(metta_virt_addr_t virt, metta_u64 len,
                                     metta_user_access_t access)
    __attribute__((weak));

/*
 * SA-17: implementación fuerte en mde/desktop/desktop.c; stub débil si MDE no enlaza.
 */
__attribute__((weak)) int metta_mde_bringup(uint32_t screen_width, uint32_t screen_height)
{
    (void)screen_width;
    (void)screen_height;
    return -1;
}

__attribute__((weak)) void metta_mde_run(void)
{
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void kernel_halt(void)
{
    for (;;) {
        __asm__ volatile("hlt");
    }
}

static void kmain_fail(metta_status_t st, const char *step)
{
    panic_fmt("kmain %s: metta_status %d", step, (int)st);
}

void kmain(metta_boot_info_t *info)
{
    metta_status_t st;
    const void    *mbi;
    int            fbst;
    virt_disk_t    vd;

    serial_init();

    st = boot_handoff_validate(info);
    if (st != METTA_OK) {
        KLOG_ERR("boot_handoff_validate failed: %d\n", (int)st);
        kernel_halt();
    }

    KLOG_BOOT("kmain: handoff OK\n");

    cpu_init();

    st = pmm_init(info->memmap, info->memmap_count);
    if (st != METTA_OK) {
        kmain_fail(st, "pmm_init");
    }
    linux_user_blobs_reserve();

    st = vmm_init();
    if (st != METTA_OK) {
        kmain_fail(st, "vmm_init");
    }

    st = vmm_late_init_after_pmm();
    if (st != METTA_OK) {
        kmain_fail(st, "vmm_late_init_after_pmm");
    }

    metta_sec_init();
    metta_sec_set_context(&(metta_sec_context_t){ .user_stack_top = 0 });
    if (vmm_probe_user_access != NULL) {
        metta_sec_set_vmm_probe(vmm_probe_user_access);
    } else {
        metta_sec_set_vmm_probe(NULL);
    }

#if METTA_BRINGUP_SCHEDULER
    scheduler_init();
#endif

    pic_init();
    irq_init();
    pit_init();
    __asm__ volatile("sti");

    KLOG_BOOT("kmain bring-up: irq timer on\n");

    st = block_init();
    if (st != METTA_OK) {
        kmain_fail(st, "block_init");
    }
    st = virt_disk_create_default(&vd);
    if (st != METTA_OK) {
        kmain_fail(st, "virt_disk_create_default");
    }
    st = virt_disk_register(&vd);
    if (st != METTA_OK) {
        kmain_fail(st, "virt_disk_register");
    }
    st = vfs_init();
    if (st != METTA_OK) {
        kmain_fail(st, "vfs_init");
    }
    metta_block_dev_t *disk = ata_data_device();
    if (disk && mfs_vfs_mount_volume(disk, "/") == METTA_OK) {
        ata_set_mounted();
        KLOG_INFO("root MFS on persistent ATA disk mounted\n");
        {
            int fd = -1;

            if (vfs_open("/home/usuario/firefox", VFS_O_RDONLY, &fd) == METTA_OK) {
                KLOG_INFO("firefox en disco, %llu bytes\n",
                          (unsigned long long)vfs_fd_size(fd));
                (void)vfs_close(fd);
            }
        }
        st = METTA_OK;
    } else {
        KLOG_WARN("MFS data volume unavailable; temporary session\n");
        st = mfs_vfs_kmain_stub();
    }
    if (st != METTA_OK) {
        kmain_fail(st, "mfs_vfs_kmain_stub");
    }


    mbi = boot_multiboot_info_raw();
    if (mbi != NULL && (info->flags & METTA_BOOT_FLAG_FRAMEBUFFER) != 0) {
        fbst = fb_init_from_multiboot(mbi);
        if (fbst == 0) {
            int mde_rc;

            KLOG_INFO("framebuffer %ux%u ready\n", info->fb_width, info->fb_height);
            {
                const framebuffer_t *fb0 = fb_get();
                if (fb0 != NULL) {
                    KLOG_INFO("fb phys=%llx pitch=%u bpp=%u\n",
                              (unsigned long long)fb0->phys_addr, fb0->pitch,
                              (unsigned)fb0->bpp);
                    /* Señal visible inmediata (azul Metta) por si el splash falla. */
                    fb_fill_rect(0, 0, (int32_t)fb0->width, (int32_t)fb0->height,
                                 fb_rgb(0x32u, 0x78u, 0xD9u));
                }
            }
            bootsplash_early_init();

            if (info->fb_width != 0u && info->fb_height != 0u) {
                mde_rc = metta_mde_bringup(info->fb_width, info->fb_height);
                if (mde_rc == 0) {
                    KLOG_INFO("mde bring-up ok\n");
                    metta_mde_run(); /* no retorna */
                }
                KLOG_WARN("metta_mde_bringup rc=%d\n", mde_rc);
            }
        } else {
            KLOG_WARN("fb_init_from_multiboot rc=%d\n", fbst);
        }
    }

    kernel_halt();
}