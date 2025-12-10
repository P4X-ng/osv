export conf_drivers_xen?=0

export conf_drivers_virtio_blk?=0
ifeq ($(conf_drivers_virtio_blk),1)
export conf_drivers_virtio?=1
endif

export conf_drivers_virtio_fs?=0
ifeq ($(conf_drivers_virtio_fs),1)
export conf_drivers_virtio?=1
endif

export conf_drivers_virtio_scsi?=0
ifeq ($(conf_drivers_virtio_scsi),1)
export conf_drivers_virtio?=1
export conf_drivers_scsi?=1
endif

export conf_drivers_virtio_net?=0
ifeq ($(conf_drivers_virtio_net),1)
export conf_drivers_virtio?=1
endif

export conf_drivers_virtio_rng?=0
ifeq ($(conf_drivers_virtio_rng),1)
export conf_drivers_virtio?=1
endif

export conf_drivers_nvme?=0
ifeq ($(conf_drivers_nvme),1)
export conf_drivers_pci?=1
endif

export conf_drivers_ide?=0
ifeq ($(conf_drivers_ide),1)
export conf_drivers_pci?=1
endif

export conf_drivers_ahci?=0
ifeq ($(conf_drivers_ahci),1)
export conf_drivers_pci?=1
endif

export conf_drivers_pvscsi?=0
ifeq ($(conf_drivers_pvscsi),1)
export conf_drivers_pci?=1
export conf_drivers_scsi?=1
endif

export conf_drivers_vmw_pvscsi?=0
ifeq ($(conf_drivers_vmw_pvscsi),1)
export conf_drivers_pci?=1
export conf_drivers_scsi?=1
endif

export conf_drivers_xenfront?=0
ifeq ($(conf_drivers_xenfront),1)
export conf_drivers_xen?=1
endif

export conf_drivers_xenfront_blk?=0
ifeq ($(conf_drivers_xenfront_blk),1)
export conf_drivers_xenfront?=1
endif

export conf_drivers_xenfront_net?=0
ifeq ($(conf_drivers_xenfront_net),1)
export conf_drivers_xenfront?=1
endif

export conf_drivers_xenfront_xenbus?=0
ifeq ($(conf_drivers_xenfront_xenbus),1)
export conf_drivers_xenfront?=1
endif

export conf_drivers_cadence?=0

export conf_drivers_virtio_mmio?=0
ifeq ($(conf_drivers_virtio_mmio),1)
export conf_drivers_virtio?=1
endif

export conf_drivers_virtio_pci?=0
ifeq ($(conf_drivers_virtio_pci),1)
export conf_drivers_virtio?=1
export conf_drivers_pci?=1
endif

export conf_drivers_virtio?=0

export conf_drivers_scsi?=0

export conf_drivers_pci?=0

export conf_drivers_acpi?=0

export conf_drivers_hpet?=0

export conf_drivers_hyperv?=0

export conf_drivers_vmware?=0

export conf_drivers_random?=1

export conf_drivers_net?=0