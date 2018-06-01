#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bch.h"

/**
 *
 * This program reads raw NAND image from standard input and updates ECC bytes in the OOB block for each sector.
 * Data layout is as following:
 *
 * 2 KB page, consisting of 4 x 512 B sectors
 * 64 bytes OOB, consisting of 4 x 16 B OOB regions, one for each sector
 *
 * In each OOB region, the first 9 1/2 bytes are user defined and the remaining 6 1/2 bytes are ECC.
 *
 */

#define BCH_T 16
#define BCH_M 15
#define DATA_SZ 512
#define OOB_ECC_OFS 0
#define OOB_ECC_LEN 26 // 16*15 = 208; 208/8 = 26

// Wide right shift by 4 bits. Preserves the very first 4 bits of the output.
static void shift_half_byte(const uint8_t *src, uint8_t *dest, size_t sz)
{
	// go right to left since input and output may overlap
	unsigned j;
	dest[sz] = src[sz - 1] << 4;
	for (j = sz; j != 0; --j)
		dest[j] = src[j] >> 4 | src[j - 1] << 4;
	dest[0] |= src[0] >> 4;
}

static void flip_bit(uint8_t* data,int arr_off,int inner_off){
	data[arr_off]^=(1<<inner_off);
}

static void write2file(uint8_t* data,int len,int i,int j){
	char str[80];
	sprintf(str,"flip_%d_%d.dat",i,j);
	FILE *temp = fopen(str,"w");
	fwrite(data,len,1,temp);
	fclose(temp);
}


int main(int argc, char *argv[])
{
	
	unsigned poly = 0;
	struct bch_control *bch = init_bch(BCH_M, BCH_T, poly);
	
	if (!bch){
		printf("init_error!");
		return -1;
	}
    printf("init-bch success!\n");
	
	// odinary ECC generate 
	FILE* fda = fopen("a.bin","r");
	printf("file open success!\n");
	uint8_t page_buffer[DATA_SZ + OOB_ECC_LEN];
	if (fread(page_buffer,DATA_SZ + OOB_ECC_LEN,1, fda) != 1)
	{
		printf("read error!\n");
		return -1;
	}
	printf("read successful!\n");
	uint8_t *sector_data = page_buffer;
	uint8_t *sector_oob = page_buffer + DATA_SZ;
	memset(sector_oob, 0, OOB_ECC_LEN);
	encode_bch(bch, sector_data, DATA_SZ, sector_oob);
	printf("encode successful!\n");
	fclose(fda);
	

	uint8_t *validate_ecc[OOB_ECC_LEN];
	unsigned int errloc[BCH_T];
	int i,j=0;
	int sum_big_0=0;
	int sum_small_0=0;
	for (i = 0; i != DATA_SZ + OOB_ECC_LEN; i++){
		for(j = 0; j < 8; j++){
			flip_bit(sector_data,i,j);
			memset(errloc,0,BCH_T*sizeof(int));
			int ret = decode_bch(bch,sector_data,DATA_SZ,sector_oob,NULL,NULL,errloc);
			if(ret>=0){
				printf("Decode flips=%d ret: %d\n",i*8+j,ret);
			}
			//if(ret>=0 && i*8+j>BCH_T){
				write2file(sector_data,DATA_SZ,i,j);	
			//}
			flip_bit(sector_data,i,j);
			
		}
	}

}
