# nvme_uio
Simple implementation of NVMe (USB3.0 Host Controller) Linux usermode driver

based on [liva/xhci_uio](https://github.com/liva/xhci_uio).

## HowTo

Warning: Following steps will replace your kernel driver from `nvme` to `uio_pci_generic`.

### Setup environment
Run `make install_uio_module` to build & install `uio_pci_generic` module.

### Load `uio_pci_generic`

Run `make check` to ensure that nvme device is there.

```
$ make check
00:0e.0 0108: 80ee:4e56 (prog-if 02 [NVM Express])
	Control: I/O+ Mem+ BusMaster- SpecCycle- MemWINV- VGASnoop- ParErr- Stepping- SERR- FastB2B- DisINTx-
	Status: Cap+ 66MHz- UDF- FastB2B- ParErr- DEVSEL=fast >TAbort- <TAbort- <MAbort- >SERR- <PERR- INTx-
	Interrupt: pin A routed to IRQ 22
	Region 0: Memory at f0808000 (32-bit, non-prefetchable) [size=32K]
	Region 2: I/O ports at d070 [size=8]
	Kernel driver in use: nvme

   bus: 00:0e.0
vendor: 80ee
device: 4e56
driver: nvme
```

After that, run `make load` to load `uio_pci_generic` for that device.


### Complie and Run.

```
$ make run
```

