# Innova-2 Flex XCKU15P XDMA PCIe DDR4 GPIO Demo

This is a simple [Vivado 2021.2](https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/vivado-design-tools/2021-2.html) starter project for the [XCKU15P FPGA](https://www.xilinx.com/products/silicon-devices/fpga/kintex-ultrascale-plus.html) on the [Innova-2 Flex SmartNIC MNV303212A-ADLT](https://www.nvidia.com/en-us/networking/ethernet/innova-2-flex/) that implements a PCIe XDMA interface to DDR4 and BRAM, and a GPIO output to one of the LEDs. The other LED is connected to a divided down PCIe clock and blinks every couple of seconds if the XDMA block has an active clock output.

[**innova2_8gb_adlt_xdma_ddr4_demo**](https://github.com/mwrnd/innova2_8gb_adlt_xdma_ddr4_demo) is an update of this project for Vivado 2023.1.

Refer to the [innova2_flex_xcku15p_notes](https://github.com/mwrnd/innova2_flex_xcku15p_notes/) project for instructions on setting up an Innova-2 system with all drivers including [Xilinx's PCIe XDMA Drivers](https://github.com/Xilinx/dma_ip_drivers).

Refer to [this tutorial](https://github.com/mwrnd/notes/tree/main/Vivado_XDMA_DDR4_Tutorial) for detailed instructions on generating a similar project from scratch.

A test version for the [4GB MNV303212A-ADIT variant is available](https://github.com/mwrnd/innova2_ddr4_troubleshooting/tree/main/test_adit_mt40a512m16).

Here is a [simple Vivado 2022.2 demo project for the MNV303611A-EDLT without DDR](https://github.com/mwrnd/innova2_mnv303611a_xcku15p_xdma) which will also work with the MNV303212A-ADLT.




# Block Diagram

![Block Design](img/innova2_xcku15p_ddr4_bram_gpio_block_design.png)




# Table of Contents

 * [Program the Design into the XCKU15P Configuration Memory](#program-the-design-into-the-xcku15p-configuration-memory)
 * [Testing the Design](#testing-the-design)
    * [AXI BRAM Communication](#axi-bram-communication)
    * [AXI GPIO Control](#axi-gpio-control)
    * [DDR4 Communication and Throughput](#ddr4-communication-and-throughput)
       * [Test DDR4 Correct Data Retention](#test-ddr4-correct-data-retention)
       * [DDR4 Communication Error](#ddr4-communication-error)
    * [XDMA Performance](#xdma-performance)
    * [Communication Methods](#communication-methods)
 * [Recreating the Design in Vivado](#recreating-the-design-in-vivado)
 * [Block Design Customization Options](#block-design-customization-options)
    * [XDMA](#xdma)
    * [DDR4](#ddr4)




## Program the Design into the XCKU15P Configuration Memory

Refer to the `innova2_flex_xcku15p_notes` project's instructions on [Loading a User Image](https://github.com/mwrnd/innova2_flex_xcku15p_notes/#loading-a-user-image). Binary Memory Configuration Bitstream Files are included in this project.

```
cd innova2_xcku15p_ddr4_bram_gpio
md5sum *bin
echo a07d4e9c498d6ff622a6ec00cb71ed0a should be md5sum of innova2_xcku15p_ddr4_bram_gpio_primary.bin
echo 1bca96206beb99a064d0dc7367b1f0e3 should be md5sum of innova2_xcku15p_ddr4_bram_gpio_secondary.bin
```



## Testing the Design

After rebooting, the design should show up as `RAM memory: Xilinx Corporation Device 9038`. It shows up at PCIe Bus Address `03:00` for me.

```
lspci | grep -i Xilinx
sudo lspci  -s 03:00  -v
sudo lspci  -s 03:00  -vvv | grep "LnkCap\|LnkSta"
```

![lspci Xilinx 9038](img/lspci_RAM_Memory_Xilinx_Corporation_Device_9038.png)

`dmesg | grep -i xdma` provides details on how Xilinx's PCIe XDMA driver has loaded.

![dmesg xdma](img/dmesg_xdma.png)

The following memory map is used by the block design when communicating using the Xilinx XDMA Test programs from *dma_ip_drivers*.

![Address Map Layout](img/Address_Map_Layout.png)

| Block        | Address (Hex) | Size  |
| ------------ |:-------------:| :---: |
| DDR4         | 0x000000000   |  8G   |
| DDR4 Control | 0x200000000   |  1M   |
| BRAM         | 0x200100000   |  8K   |
| GPIO         | 0x200110000   |  64K  |


### AXI BRAM Communication

The commands below generate 8kb of random data, then send it to a BRAM in the XCKU15P, then read it back and confirm the data is identical. Note `h2c` is *Host-to-Card* and `c2h` is *Card-to-Host*. The address of the BRAM is `0x200100000` as noted above.
```Shell
cd ~/dma_ip_drivers/XDMA/linux-kernel/tools/
dd if=/dev/urandom bs=1 count=8192 of=TEST
sudo ./dma_to_device   --verbose --device /dev/xdma0_h2c_0 --address 0x200100000 --size 8192  -f    TEST
sudo ./dma_from_device --verbose --device /dev/xdma0_c2h_0 --address 0x200100000 --size 8192 --file RECV
sha256sum TEST RECV
```

![XDMA BRAM Test](img/XDMA_BRAM_Test.png)


### AXI GPIO Control

![User LED](img/User_LEDs_on_Innova2.png)

The design includes an [AXI GPIO](https://docs.xilinx.com/v/u/3.0-English/ds744_axi_gpio) block to control Pin A6, the *D19* LED on the back of the Innova-2. The LED can be turned off by writing a `0x01` to the `GPIO_DATA` Register. Only a single bit is enabled in the port so excess bit writes are ignored. No direction control writes are necessary as the port is set up for output-only (the `GPIO_TRI` Direction Control Register is fixed at `0xffffffff`).

![AXI GPIO](img/AXI_GPIO.png)

The commands below should turn off then turn on the *D19* LED. First, two one-byte files are created, a binary all-ones byte and a binary all-zeros byte. These are then sent to address `0x200110000`, the `GPIO_DATA` Register. As only a single bit is enabled in the block design, reading from `GPIO_DATA` returns `0x00000001` when the LED is off and `0x00000000` when it is on.
```Shell
cd ~/dma_ip_drivers/XDMA/linux-kernel/tools/
echo -n -e "\xff" >ff.bin   ;   od -A x -t x1z -v  ff.bin
echo -n -e "\x00" >00.bin   ;   od -A x -t x1z -v  00.bin
ls -l  ff.bin  00.bin
sudo ./dma_to_device   --verbose --device /dev/xdma0_h2c_0 --address 0x200110000 --size 1 -f     ff.bin
sudo ./dma_to_device   --verbose --device /dev/xdma0_h2c_0 --address 0x200110000 --size 1 -f     00.bin
sudo ./dma_from_device --verbose --device /dev/xdma0_c2h_0 --address 0x200110000 --size 8 --file RECV
od -A x -t x1z -v  RECV
```

![XDMA GPIO Test](img/XDMA_GPIO_Test.png)


### DDR4 Communication and Throughput

Memory Management prevents data reads from uninitialized memory. DDR4 must first be written to before it can be read from.

Your system must have enough free memory to test DDR4 DMA transfers. Run `free -m` to determine how much RAM you have available and keep the amount of data to transfer below that. The commands below generate 512MB of random data then transfer it to and from the Innova-2. The address of the DDR4 is `0x0` as noted earlier.

The `dd` command is used to generate a file (`of=DATA`) from pseudo-random data (`if=/dev/urandom`). The value for Block Size (`bs`) will be multiplied by the value for `count` to produce the size in bytes of the output file. For example, `8192*65536=536870912=0x20000000=512MiB`. Use a block size (`bs=`) that is a multiple of your drive's block size. `df .` informs you on which drive your current directory is located. `dumpe2fs` will tell you the drive's block size.

```Shell
df .
sudo dumpe2fs /dev/sda3 | grep "Block size"
```

![Determine SSD or Hard Drive Block Size](img/df_dumpe2fs_Determine_Block_Size.png)

Note `128MiB = 134217728 = 0x8000000` which can be generated with `dd` using the `bs=8192 count=16384` options.

To test the full 8GB of memory you can increment the address by the data size enough times that all `8Gib = 8589934592 = 0x200000000` has been tested.

If you have 8GB+ of free memory space, generate 8GB of random data with the `dd` command options `bs=8192 count=1048576` and test the DDR4 in one go.

If checksums do not match, `vbindiff DATA RECV` can be used to determine differences between the sent and received data and the failing address locations.

Note that data is loaded from your system drive into memory then sent to the Innova-2 over PCIe DMA. Likewise it is loaded from the Innova-2's DDR4 into system RAM, then onto disk. The wall time of these functions can therefore be significantly longer than the DMA Memory-to-Memory over PCIe transfer time.
```Shell
cd ~/dma_ip_drivers/XDMA/linux-kernel/tools/
free -m
dd  if=/dev/urandom  bs=8192  count=65536  of=DATA
sudo ./dma_to_device   --verbose --device /dev/xdma0_h2c_0 --address 0x0 --size 536870912  -f    DATA
sudo ./dma_from_device --verbose --device /dev/xdma0_c2h_0 --address 0x0 --size 536870912 --file RECV
sha256sum DATA RECV
```

![XDMA DDR4 Test](img/XDMA_DDR4_Test.png)


#### Test DDR4 Correct Data Retention


Test the first `1GB = 1073741824 bytes` of the DDR4 memory space using a binary all-zeros file.
```
cd ~/dma_ip_drivers/XDMA/linux-kernel/tools/
free -m
dd if=/dev/zero of=DATA bs=8192 count=131072
sudo ./dma_to_device   -v -d /dev/xdma0_h2c_0 --address 0x0 --size 1073741824 -f DATA
sudo ./dma_from_device -v -d /dev/xdma0_c2h_0 --address 0x0 --size 1073741824 -f RECV
md5sum DATA RECV
```

![Test DDR4 With All-Zeros File](img/DDR4_Test_With_All_Zeros_File.png)

Test the first `1GB = 1073741824 bytes` of the DDR4 memory space using a binary [all-ones file](https://stackoverflow.com/questions/10905062/how-do-i-get-an-equivalent-of-dev-one-in-linux).
```
cd ~/dma_ip_drivers/XDMA/linux-kernel/tools/
free -m
tr '\0' '\377' </dev/zero | dd of=DATA bs=8192 count=131072 iflag=fullblock
sudo ./dma_to_device   -v -d /dev/xdma0_h2c_0 --address 0x0 --size 1073741824 -f DATA
sudo ./dma_from_device -v -d /dev/xdma0_c2h_0 --address 0x0 --size 1073741824 -f RECV
md5sum DATA RECV
```

![Test DDR4 With All-Ones File](img/DDR4_Test_With_All_Ones_File.png)


#### DDR4 Communication Error

If you attempt to send data to the DDR4 address but get `write file: Unknown error 512` it means DDR4 did not initialize properly or the AXI bus has encountered an error and stalled. Proceed to the [Innova-2 DDR4 Troubleshooting](https://github.com/mwrnd/innova2_ddr4_troubleshooting) project.
```Shell
sudo ./dma_to_device --verbose --device /dev/xdma0_h2c_0 --address 0x0 --size 8192 -f TEST
```

![Error 512](img/XDMA_DDR4_Communication_Failure_Error_512.png)




### XDMA Performance

Xilinx's *dma_ip_drivers* include a simple performance measurement tool which tests at address `0x0` with a default transfer size of 32kb.
```Shell
cd ~/dma_ip_drivers/XDMA/linux-kernel/tools/
sudo ./performance --device /dev/xdma0_h2c_0
sudo ./performance --device /dev/xdma0_c2h_0
```

![XDMA dma_ip_drivers performance](img/xdma_performance.png)




### Communication Methods

The XDMA Driver ([Xilinx's dma_ip_drivers](https://github.com/xilinx/dma_ip_drivers)) creates [read-only and write-only](https://manpages.debian.org/bookworm/manpages-dev/open.2.en.html#File_access_mode) [character device](https://en.wikipedia.org/wiki/Device_file#Character_devices) files, `/dev/xdma0_h2c_0` and `/dev/xdma0_c2h_0`, that allow direct access to the FPGA design's AXI Bus. To read from an AXI Device at address `0x200110000` you would read from address `0x200110000` of the `/dev/xdma0_c2h_0` (Card-to-Host) file. To write you would write to the appropriate address of `/dev/xdma0_h2c_0` (Host-to-Card).

For example, to toggle the LED you can write to the appropriate address using [`dd`](https://manpages.debian.org/testing/coreutils/dd.1.en.html). Note `dd` requires numbers in Base-10 so you can use [`printf`](https://manpages.debian.org/testing/coreutils/printf.1.en.html) to convert from the hex address.
```
echo -n -e "\x00" >00.bin  ;  xxd -b 00.bin
echo -n -e "\x01" >01.bin  ;  xxd -b 01.bin
printf "%d\n" 0x200110000
sudo dd if=00.bin of=/dev/xdma0_h2c_0 count=1 bs=1 seek=8591048704
sudo dd if=01.bin of=/dev/xdma0_h2c_0 count=1 bs=1 seek=8591048704
```

![Toggle LED using dd](img/dd_LED_Toggle.png)

You can also read or write to the AXI BRAM Memory.
```
dd if=/dev/urandom bs=1 count=8192 of=TEST
printf "%d\n" 0x200100000
sudo dd if=TEST of=/dev/xdma0_h2c_0 count=1 bs=8192 seek=8590983168 oflag=seek_bytes
sudo dd if=/dev/xdma0_c2h_0 of=RECV count=1 bs=8192 skip=8590983168 iflag=skip_bytes
md5sum TEST RECV
```

![Access AXI BRAM using dd](img/dd_AXI_BRAM.png)

[xdma_test.c](xdma_test.c) is a simple C program that writes then reads to the given AXI Address which can be the DDR4 or BRAM and then toggles the LED.
```
gcc xdma_test.c -g -Wall -o xdma_test
sudo ./xdma_test /dev/xdma0_c2h_0 /dev/xdma0_h2c_0 0x200100000 0x200110000
```

![xdma_test.c XDMA File Access](img/xdma_test_0x200100000_0x200110000.png)




## Recreating the Design in Vivado

Refer to [Vivado XDMA DDR4 Tutorial](https://github.com/mwrnd/notes/tree/main/Vivado_XDMA_DDR4_Tutorial) for detailed notes on recreating a similar design starting with a blank project.

To recreate this design, run the `source` command from the main Vivado **2021.2** window. Only some versions of Vivado successfully implement this block design.

```
cd innova2_xcku15p_ddr4_bram_gpio
dir
source innova2_xcku15p_ddr4_bram_gpio.tcl
```

![Source Project Files](img/Vivado_source_project_files.png)

Click on *Generate Bitstream*.

![Generate Bitstream](img/Vivado_Generate_Bitstream.png)

Synthesis and implementation should complete within an hour.

![Synthesis and Implemetation Duration](img/synth_and_impl_duration_byte-lanes.png)

Once the Bitstream is generated, run *Write Memory Configuration File*, select *bin*, *mt25qu512_x1_x2_x4_x8*, *SPIx8*, *Load bitstream files*, and a location and name for the output binary files. The bitstream will end up in the `innova2_xcku15p_ddr4_bram_gpio/innova2_xcku15p_ddr4_bram_gpio.runs/impl_1` directory as `design_1_wrapper.bit`. Vivado will add the `_primary.bin` and `_secondary.bin` extensions as the Innova-2 uses dual MT25QU512 FLASH ICs in x8 for high speed programming.

![Write Memory Configuration File](img/Vivado_Write_Memory_Configuration_File.png)

Proceed to [Loading a User Image](https://github.com/mwrnd/innova2_flex_xcku15p_notes/#loading-a-user-image)


## Block Design Customization Options

### XDMA

The Innova-2's XCKU15P is wired for **x8** PCIe at *PCIe Block Location:* **X0Y2**. It is capable of **8.0 GT/s** Link Speed.

![XDMA Basic Customizations](img/XDMA_Customization_Options-Basic.png)

For this design I set the PCIe *Base Class* to **Memory Controller** and the *Sub-Class* to **RAM**.

![XDMA PCIe ID Customizations](img/XDMA_Customization_Options-PCIe_ID.png)

I disable the **Configuration Management Interface**.

![XDMA Misc Customizations](img/XDMA_Customization_Options-Misc.png)

### DDR4

The DDR4 is configured for a Memory Speed of **833**ps = 1200MHz = 2400 MT/s Transfer Rate. The DDR4 reference clock is **9996**ps = 100.04MHz. This project includes a custom part definition in [innova2_ku15p_MT40A1G16.csv](innova2_ku15p_MT40A1G16.csv) for the [MT40A1G16](https://www.micron.com/products/dram/ddr4-sdram/part-catalog/mt40a1g16knr-075).

![DDR4 Basic Configuration](img/DDR4_Customization_Options-Basic.png)

*Data Mask and DBI* is set to **NO DM DBI WR RD** which automatically enables ECC on a 72-Bit interface.

![When is ECC Enabled](img/DDR4_72-Bit_When_Is_ECC_Enabled.png)

The *Arbitration Scheme* is set to **Round Robin** under AXI Options.

![DDR4 AXI Configuration](img/DDR4_Customization_Options-AXI_Options.png)

