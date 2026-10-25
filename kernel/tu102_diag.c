// SPDX-License-Identifier: GPL-2.0-only
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>

static char *target;
module_param(target, charp, 0400);
MODULE_PARM_DESC(target, "Explicit 0000:42:00.0 or 0000:43:00.0; unbound card only");
static struct dentry *directory;
static struct debugfs_blob_wrapper rom_blob;
static const u32 offsets[] = {
    0x000000, 0x100ce0, 0x625f04,
    0x1fa824, 0x1fa828, 0x409650, 0x409664, 0x40966c, 0x110100, 0x840100
};

static int __init tu102_diag_init(void)
{
    struct pci_dev *pdev;
    void __iomem *bar;
    u16 command;
    int ret, i;
    unsigned int bus;
    if (!target)
        return -EINVAL;
    if (!strcmp(target, "0000:42:00.0"))
        bus = 0x42;
    else if (!strcmp(target, "0000:43:00.0"))
        bus = 0x43;
    else
        return -EINVAL;
    pdev = pci_get_domain_bus_and_slot(0, bus, PCI_DEVFN(0, 0));
    if (!pdev)
        return -ENODEV;
    device_lock(&pdev->dev);
    if (pdev->vendor != 0x10de || pdev->device != 0x1e04 ||
        pdev->subsystem_vendor != 0x107d || pdev->subsystem_device != 0x1e04) {
        ret = -ENODEV;
        goto out;
    }
    if (pdev->driver) {
        ret = -EBUSY;
        goto out;
    }
    ret = pci_read_config_word(pdev, PCI_COMMAND, &command);
    if (ret || command == 0xffff || !(command & PCI_COMMAND_MEMORY)) {
        ret = -EACCES;
        goto out;
    }
    if (pci_resource_len(pdev, 0) < 0x840104) {
        ret = -EINVAL;
        goto out;
    }
    ret = pci_request_region(pdev, 0, "tu102_diag_readonly");
    if (ret)
        goto out;
    bar = pci_iomap(pdev, 0, 0);
    if (!bar) {
        ret = -ENOMEM;
        goto release;
    }
    rom_blob.size = 1024 * 1024;
    rom_blob.data = kvzalloc(rom_blob.size, GFP_KERNEL);
    if (!rom_blob.data) {
        ret = -ENOMEM;
        goto unmap;
    }
    memcpy_fromio(rom_blob.data, bar + 0x300000, rom_blob.size);
    if (((u8 *)rom_blob.data)[0] != 0x55 || ((u8 *)rom_blob.data)[1] != 0xaa) {
        ret = -EBADMSG;
        goto free;
    }
    for (i = 0; i < ARRAY_SIZE(offsets); i++)
        dev_info(&pdev->dev, "tu102_diag readonly offset=%#x value=%#x\n",
                 offsets[i], readl(bar + offsets[i]));
    directory = debugfs_create_dir("tu102_diag", NULL);
    if (IS_ERR_OR_NULL(directory)) {
        ret = directory ? PTR_ERR(directory) : -ENOMEM;
        directory = NULL;
        goto free;
    }
    if (IS_ERR_OR_NULL(debugfs_create_blob("prom.bin", 0400, directory, &rom_blob))) {
        debugfs_remove_recursive(directory);
        directory = NULL;
        ret = -ENOMEM;
        goto free;
    }
    dev_info(&pdev->dev, "tu102_diag: snapshot only; no DMA, reset, firmware start or register writes\n");
    ret = 0;
    goto unmap;
free:
    kvfree(rom_blob.data);
    rom_blob.data = NULL;
unmap:
    pci_iounmap(pdev, bar);
release:
    pci_release_region(pdev, 0);
out:
    device_unlock(&pdev->dev);
    pci_dev_put(pdev);
    return ret;
}

static void __exit tu102_diag_exit(void)
{
    debugfs_remove_recursive(directory);
    kvfree(rom_blob.data);
}
module_init(tu102_diag_init);
module_exit(tu102_diag_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TU102 explicit-target pre-driver snapshot; no hardware writes");
