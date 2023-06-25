/*

Prerequisites:
 - Xilinx XDMA to AXI FPGA project with 8kb AXI BRAM, such as:
   github.com/mwrnd/innova2_xcku15p_ddr4_bram_gpio
 - XDMA Drivers from github.com/xilinx/dma_ip_drivers
   Install Instructions at github.com/mwrnd/innova2_flex_xcku15p_notes

Compile with:

  gcc  xdma_test.c  -g  -Wall  -o xdma_test

Run with:

  sudo ./xdma_test /dev/xdma0_c2h_0 /dev/xdma0_h2c_0 0x200100000 0x200110000

*/

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>


// Using 8 kilobyte == 2^13 = 8192 byte array. Size was defined in the
// Vivado FPGA Project Block Diagram Address Editor as the Data Range for BRAM
// On Linux, read/write can transfer at most 0x7FFFF000 (2,147,479,552) bytes
#define DATA_SIZE 8192


int main(int argc, char **argv)
{
	uint64_t axi_bram_addr;
	uint64_t axi_gpio_addr;
	int xdma_fd_read;
	int xdma_fd_wrte;
	char *xdma_c2h_name;
	char *xdma_h2c_name;
	uint8_t wrte_data[DATA_SIZE];
	uint8_t read_data[DATA_SIZE];
	uint32_t val = 0;
	int errorcount = 0;
	ssize_t rc;




	// Display Program Usage Instructions

	if (argc != 5)
	{
		printf("%s is a simple FPGA XDMA Test Program that\n", argv[0]);
		printf("reads and writes to BRAM and toggles a GPIO LED\n");
		printf("Usage:\n");
		printf(" sudo %s C2H_DEVICE_NAME  ", argv[0]);
		printf("H2C_DEVICE_NAME  AXI_BRAM_ADDR AXI_GPIO_ADDR\n");
		printf("Example:\n");
		printf(" sudo %s /dev/xdma0_c2h_0 ", argv[0]);
		printf("/dev/xdma0_h2c_0 0x200100000   0x200110000\n");
		printf("\n");
		exit(EXIT_FAILURE);
	}




	// Read in Program Arguments

	// TODO - sprintf and strcpy are unsafe for use
	// with unsanitized and/or untrusted user inputs

	xdma_fd_read = 0;
	xdma_fd_wrte = 0;
	axi_bram_addr = (uint64_t)strtol(argv[3], NULL, 16);
	axi_gpio_addr = (uint64_t)strtol(argv[4], NULL, 16);
	xdma_c2h_name = malloc(strlen(argv[1]));
	if (!(xdma_c2h_name)) { printf("malloc failed in main, c2h"); exit(-1); }
	xdma_h2c_name = malloc(strlen(argv[2]));
	if (!(xdma_h2c_name)) { printf("malloc failed in main, h2c"); exit(-1); }
	strcpy(xdma_c2h_name, argv[1]);
	strcpy(xdma_h2c_name, argv[2]);

	printf("FPGA XDMA AXI BRAM and GPIO Test Program\n");
	printf("C2H Device: %s\n", xdma_c2h_name);
	printf("H2C Device: %s\n", xdma_h2c_name);
	printf("AXI BRAM Address: %s = 0x%0lX = %ld\n",
		argv[3], axi_bram_addr, axi_bram_addr);
	printf("AXI GPIO Address: %s = 0x%0lX = %ld\n",
		argv[4], axi_gpio_addr, axi_gpio_addr);




	// Open the XDMA Files

	xdma_fd_wrte = open(xdma_h2c_name, O_WRONLY);

	if (xdma_fd_wrte < 0) {
		fprintf(stderr, "unable to open write device %s, %d.\n",
			xdma_h2c_name, xdma_fd_wrte);
		perror("File Open");
	}


	xdma_fd_read = open(xdma_c2h_name, O_RDONLY);

	if (xdma_fd_read < 0) {
		fprintf(stderr, "unable to open read device %s, %d.\n",
			xdma_c2h_name, xdma_fd_read);
		perror("File Open");
	}




	// Generate Random Data

	// Seed the random number generator with an address returned by malloc
	srandom((int)((long int)xdma_c2h_name));

	// Generate a random data array of size DATA_SIZE
	for (int indx = 0; indx < DATA_SIZE ; indx = indx + 4)
	{
		val = rand();
		memcpy(&wrte_data[indx], &val, 4);
	}




	// -------- AXI BRAM Write then Read Test ----------------------------

	// Write the random data to the FPGA's AXI BRAM

	rc = lseek(xdma_fd_wrte, axi_bram_addr, SEEK_SET);

	if (rc < 0) {
		fprintf(stderr, "%s, seek offset failed at 0x%lX, 0x%ld.\n",
				xdma_h2c_name, axi_bram_addr, rc);
		perror("File Seek");
		return -EIO;
	}

	rc = write(xdma_fd_wrte, wrte_data, DATA_SIZE);

	if (rc < 0) {
		fprintf(stderr, "%s, write data @ 0x%lX failed, %ld.\n",
			xdma_h2c_name, axi_bram_addr, rc);
		perror("File Write");
		return -EIO;
	}
	
	if (rc != DATA_SIZE) {
		fprintf(stderr, "%s, write underflow 0x%ld/0x%d @ 0x%lx.\n",
			xdma_h2c_name, rc, DATA_SIZE, axi_bram_addr);
		return -EIO;
	}

	printf("\nWrote %ld bytes to   %s at address 0x%lX\n",
		rc, xdma_h2c_name, axi_bram_addr);




	// Read data from the FPGA's AXI BRAM

	rc = lseek(xdma_fd_read, axi_bram_addr, SEEK_SET);

	if (rc < 0) {
		fprintf(stderr, "%s, seek offset failed at 0x%lX, 0x%ld.\n",
				xdma_c2h_name, axi_bram_addr, rc);
		perror("File Seek");
		return -EIO;

	}

	rc = read(xdma_fd_read, read_data, DATA_SIZE);

	if (rc < 0) {

		fprintf(stderr, "%s, read data @ 0x%lX failed, %ld.\n",
			xdma_c2h_name, axi_bram_addr, rc);
		perror("File Read");
		return -EIO;
	}

	if (rc != DATA_SIZE) {
		fprintf(stderr, "%s, read underflow 0x%ld/0x%d @ 0x%lx.\n",
			xdma_c2h_name, rc, DATA_SIZE, axi_bram_addr);

		return -EIO;
	}

	printf("\nRead  %ld bytes from %s at address 0x%lX\n",
		rc, xdma_c2h_name, axi_bram_addr);




	// Compare the Written and Read Data

	errorcount = 0;
	for (int indx = 0; indx < DATA_SIZE ; indx++)
	{
		if (read_data[indx] != wrte_data[indx])
		{
			errorcount++;
			printf("Data did not match at index %d, ", indx);
			printf("read_data = 0x%02X, wrte_data = 0x%02X\n",
				read_data[indx], wrte_data[indx]);
		}

		// too many errors, something is wrong, do not check any more
		if (errorcount > 7 ) { break; }
	}

	if (errorcount == 0)
	{
		printf("\nSuccess - Read Data matches Written Data!\n\n");
	} else {
		printf("Too many errors encountered, something is wrong.\n\n");
	}




	// -------- AXI GPIO LED Test ----------------------------------------

	// Toggle LED once a second for 7 seconds
	for (int indx = 0; indx < 8 ; indx++)
	{
		// LED in design is controlled by the LSB of the first byte
		wrte_data[0] = (0x01 & (uint8_t)indx);

		// Write 1 byte using pwrite, which combines lseek and write
		rc = pwrite(xdma_fd_wrte, (char *)wrte_data, 1, axi_gpio_addr);
		//fsync(xdma_fd_wrte);

		if (rc < 0) {
			fprintf(stderr, "%s, write byte @ 0x%lX failed, %ld.\n",
				xdma_h2c_name, axi_gpio_addr, rc);
			perror("File Write");
			return -EIO;
		}


		printf("Wrote 0x%02X to %s at address 0x%lX",
			wrte_data[0], xdma_h2c_name, axi_gpio_addr);

		if (wrte_data[0]) {
			// LED signal is inverted in hardware, 1==OFF
			printf(", LED D19 should be OFF.\n");
		} else {
			printf(", LED D19 should be ON.\n");
		}


		sleep(1);
	}




	printf("\n");

	close(xdma_fd_wrte);
	close(xdma_fd_read);
	exit(EXIT_SUCCESS);
}

