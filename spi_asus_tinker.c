#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

unsigned short getShortFromArray(uint8_t array[2]);
void connectToSPI(struct spi_ioc_transfer tr);
unsigned short * getCurrentArray(struct spi_ioc_transfer tr);
void printArrayToFile(unsigned short * poiter);
void startMotor(struct spi_ioc_transfer tr);
void stopMotor(struct spi_ioc_transfer tr);
void setReferenceCurrent(struct spi_ioc_transfer tr,int cur);

//errors funcion
static void pabord(const char *s)
{
	perror(s);
	abort();
}

//SPI settings
static uint8_t mode = SPI_MODE_0;
static uint8_t bits = 8;
static uint32_t speed = 5333000;


//variables for SPI
int ret = 0;
int fd;
uint8_t tx[] = {0x12, 0x34};
uint8_t rx[] = {0, 0};


int main(int argc, char *argv[])
{
	int i = 0;

	fd = open("/dev/spidev2.1", O_RDWR);

	if(fd<0)
	{
		pabord("can't open device");
	}

	/*spi mode*/
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if(ret == -1)
	{
		pabord("can't set spi mode");
	}

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if(ret == -1)
	{
		pabord("can't set spi mode");
	}

	/* bits per word */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if(ret == -1)
	{
		pabord("can't set bits per word");
	}
	
	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if(ret == -1)
	{
		pabord("can't set bits per word");
	}
	
	/*max speed hz */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if(ret == -1)
	{
		pabord("can't set max speed Hz");
	}
	
	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if(ret == -1)
	{
		pabord("can't set max speed Hz");
	}

	printf("spi mode: %d\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed %d Hz (%d kHz)\n", speed,speed/1000);

	
	/*full-duplex transfer*/
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long) tx,
		.rx_buf = (unsigned long) rx,
		.len = 2,
		.delay_usecs = 10,
		.speed_hz = speed,
		.bits_per_word = bits,	
	};

	connectToSPI(tr);


	printf("---Enter \"s\" for START motor\n");
	printf("---Enter \"x\" for STOP motor\n");
	printf("---Enter \"r\" SET reference current\n");
	printf("---Enter \"a\" for read and write current value\n");
	printf("---Enter \"q\" for exit\n");
	
	while(1)
	{
		char in;
		unsigned short *pointerToArray;

		scanf("%c", &in);
		if(in == 's')
		{
			startMotor(tr);
		}
		else if(in == 'x')
		{
			stopMotor(tr);
		}
		else if(in == 'r')
		{
			int ref = 0;
			printf("---Enter interger value from -1000 to 1000: ");
			int scan = scanf("%d", &ref);
			if (scan < 1)  { printf("\twrong reference\n"); continue; }

			setReferenceCurrent(tr, ref);
		}
		else if(in == 'a')
		{
			pointerToArray = getCurrentArray(tr);
			printArrayToFile(pointerToArray);	
		}
		else if(in == 'q')
		{
			printf("\tquit from program\n");
			break;
		}
	}
		
	while(i < 5)
	{
		tx[0] = 1;
		tx[1] = 1;
		ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);

		if (ret < 1)
		{
			pabord("can't send spi message");
		}

		for (ret = 0; ret < 2; ret++)
		{
			printf("%.2x ", rx[ret]);
		}

		puts("");
		i += 1;
	}

	close(fd);

	return ret;
			

}

void connectToSPI(struct spi_ioc_transfer tr)
{
	int ret = 0;
	unsigned short res = 0;

	printf("Connecting to MC...\n");

	tx[0] = 0b10101010;
	tx[1] = 0b10101010;

	while(1)
	{
		int i;

		ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
		if (ret < 1)
		{
			pabord("can't send spi message");
		}
		
		res = getShortFromArray(rx);	
		printf("\trecieve: %.4x\n", (int)res);

		if(res == 0xAAAA)
		{
			printf("...successful\n");
			break;
		}

		if(i > 5)
		{
			printf("something wrong\n");
			close(fd);
			exit(1);
		}

		i++;
	}
}

void startMotor(struct spi_ioc_transfer tr)
{
	int ret = 0;

	tx[0] = 0x10;
	tx[1] = 0x00;

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
	{
		pabord("can't send spi message");
	}
	printf("\tstart motor\n");
}

void stopMotor(struct spi_ioc_transfer tr)
{
	int ret = 0;

	tx[0] = 0x20;
	tx[1] = 0x00;

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
	{
		pabord("can't send spi message");
	}
	printf("\tmotor stopped\n");
}

void setReferenceCurrent(struct spi_ioc_transfer tr,int cur)
{
	int ret = 0;
	uint16_t dCur = 0;
	uint8_t aCur[] = {0 , 0};
	
	if(cur >= 0)
	{
		dCur = (uint16_t) cur;
	}
	else
	{
		dCur = (uint16_t) (-1 * cur);
	}

	if(dCur > 1000) dCur = 1000;
	
	aCur[0] = (uint8_t) (dCur >> 8);
	aCur[1] = (uint8_t) (dCur & 0x00FF);

	if(cur >= 0)
	{
		tx[0] = 0x30|aCur[0];
		tx[1] = aCur[1];
	}
	else
	{
		tx[0] = 0x40|aCur[0];
		tx[1] = aCur[1];
	}

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
	{
		pabord("can't send spi message");
	}
	printf("\tsetting reference current to %.2f\n",(dCur/100.0f));

}

unsigned short * getCurrentArray(struct spi_ioc_transfer tr)
{
	int ret = 0;
	unsigned short num = 0;
	static unsigned short array[5000];

	tx[0] = 0b11110000;
	tx[1] = 0b00001111;

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
	{
		pabord("can't send spi message");
	}
	
	tx[0] = 0x00;
	tx[1] = 0x00;

	for(int j = 0; j < 5000; j++)
	{
		ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
		if (ret < 1)
		{
			pabord("can't send spi message");
		}
		num = getShortFromArray(rx);
		array[j] = num;	
	}	
	return array;
}	


void printArrayToFile(unsigned short * poiter)
{
	FILE *fp;

	if((fp = fopen("exp.csv","w")) == NULL)
	{
		printf("Cannot open file.\n");
		return;
	}

	for(int i = 0; i < 5000; i++)
	{
		fprintf(fp, "%.4d,%.5d\n", i, *(poiter + i));
	}

	fclose(fp);
	printf("\tdata was recorded\n");
}

unsigned short getShortFromArray(uint8_t array[2])
{
	unsigned short res = 0x0000;
	res = array[1];
	res = res | (array[0] << 8);
	return res;	
}
