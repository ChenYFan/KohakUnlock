# RTX 3090: upstream setup and lessons

Use upstream: https://github.com/Zagitta/gpu-unlock . We do not publish a separate Ampere implementation.
Known-working source in this experiment: `a9b0c75bb79b0c9fac6be68feade78d0b8715395`.
Four RTX 3090s worked with Ubuntu 24.04, kernel 6.8.0-147-generic, NVIDIA open 595.99.02. Firmware helper inputs labeled 610.43.03 did NOT require installing NVIDIA driver 610.

## Mechanism

The early module uses the Ampere firmware/payload path to open protected feature access, selects full rates, performs its reset/handoff sequence, and lets the unmodified NVIDIA driver initialize. It does not flash VBIOS or add VRAM/cores. Power-loss clears volatile state; automatic early-boot reapplication is what makes it persist operationally.

## Practical procedure

Pin and inspect source, build/test, preserve known-good kernel/initramfs, generate a separate image, load before udev/NVIDIA, target one exact PCI address first, select one-shot GRUB, verify actual tensor performance and correctness. Only broaden to more cards after the one-card run. We later confirmed four cards on a normal warm reboot and selected the tested four-card entry as default, retaining stock Ubuntu and a visible boot menu.

A pinned custom image does not automatically follow kernel/driver upgrades. Rebuild/revalidate or select stock first. Otherwise the initramfs can contain stale driver components. No installer here changes that setup.

## Failures encountered

- Revision fc833215 added a pre-launch HALTED-clear check. Our first attempt timed out at 0x110100; the failed module retained DMA/BAR resources and NVIDIA could not attach to that card. Full power-off plus normal boot recovered it. Rolling back source to a9b0c75 and cold-starting worked. Later warm boots also worked. That does not isolate every variable in the original failure.
- Initial source packaging referenced removed documentation. Build/test before root installation.
- initramfs-tools modules.d loading can occur after udev; a pre-udev init-top hook was necessary. Inspect ORDER and exact included module/firmware, not merely build exit status.
- Windows CRLF in transferred shell hooks broke initramfs script parsing. Validate shell syntax and LF endings.
- A slow server reboot included old-session swap/unmount timeouts. Do not infer GPU failure from an SSH timeout. Inspect console and prior boot logs.
- A persistence daemon lacked access to root:vglusers 0660 nodes. Giving its account the appropriate primary group solved our setup without world-writable device nodes. This is a local permission issue, not a universal requirement.

## References

- https://github.com/Zagitta/gpu-unlock/issues/11 — Ubuntu pre-udev ordering
- https://github.com/Zagitta/gpu-unlock/issues/17 — dual-3090 report with a9b0c75
- https://github.com/Zagitta/gpu-unlock/issues/23 — fc83321 non-cold failure report
- https://github.com/Zagitta/gpu-unlock/pull/24 — Ada-specific port; not a device-ID-only extension
- https://huggingface.co/TOTORONG/AD103_win_unlocker — board-specific Ada UEFI research

Do not apply Ampere firmware/reset code to Turing or Blackwell. A similar throughput restriction does not establish compatible firmware or registers.
