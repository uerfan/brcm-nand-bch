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

#define BCH_T 4
#define BCH_M 15
#define SECTOR_SZ 2048
#define OOB_SZ 8
#define SECTORS_PER_PAGE 8
#define OOB_ECC_OFS 0
#define OOB_ECC_LEN 8

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

int main(int argc, char *argv[])
{
	if(argc < 4){
		printf("USAGE: ./main 0 InputFile OutputFile");
		return 0;
	}
	unsigned poly = argc < 2 ? 0 : strtoul(argv[1], NULL, 0);
	struct bch_control *bch = init_bch(BCH_M, BCH_T, poly);
	
	if (!bch){
		printf("init_error!");
		return -1;
	}
        printf("init-bch success!\n");
	FILE* fda = fopen(argv[2],"r");
	FILE* fdb = fopen(argv[3],"w");
	printf("file open success!\n");
	uint8_t page_buffer[(SECTOR_SZ + OOB_SZ) * SECTORS_PER_PAGE];
	while (1)
	{
		if (fread(page_buffer,(SECTOR_SZ) * SECTORS_PER_PAGE,1, fda) != 1)
		{
			printf("read finished!\n");
			break;
			//return 0;
		}
		printf("read successful!\n");
		fwrite(page_buffer,SECTOR_SZ * SECTORS_PER_PAGE,1, stdout);
		// Erased pages have ECC = 0xff .. ff even though there may be user bytes in the OOB region
		printf("\n");
		int erased_block = 1;
		unsigned i;
		for (i = 0; i != SECTOR_SZ * SECTORS_PER_PAGE; ++i)
			if (page_buffer[i] != 0xff)
			{
				erased_block = 0;
				break;
			}

		for (i = 0; i != SECTORS_PER_PAGE; ++i)
		{
			const uint8_t *sector_data = page_buffer + SECTOR_SZ * i;
			uint8_t *sector_oob = page_buffer + SECTOR_SZ * SECTORS_PER_PAGE + OOB_SZ * i;
			if (erased_block)
			{
				// erased page ECC consumes full 7 bytes, including high 4 bits set to 0xf
				memset(sector_oob, 0xff, OOB_ECC_LEN);
			}
			else
			{	
				// compute ECC
				memset(sector_oob, 0, OOB_ECC_LEN);
				encode_bch(bch, sector_data, SECTOR_SZ, sector_oob);
				printf("encode success!\n");
				fwrite(sector_oob,OOB_ECC_LEN,1,stdout);
			}
		}

		//fwrite(page_buffer, (SECTOR_SZ + OOB_SZ) * SECTORS_PER_PAGE, 1, stdout);
	}
        //fwrite(page_buffer,(SECTOR_SZ + OOB_SZ) * SECTORS_PER_PAGE,1, stdout);
	fwrite(page_buffer,(SECTOR_SZ + OOB_SZ) * SECTORS_PER_PAGE,1, fdb);
	fclose(fda);
	fclose(fdb);

	fda = fopen(argv[3],"r");
	if(fread(page_buffer,(SECTOR_SZ) * SECTORS_PER_PAGE,1, fda)!=1){
		printf("read error!\n");
		return 0;
	}
	unsigned int i;
	for (i = 0; i != SECTORS_PER_PAGE; ++i){
		uint8_t *sector_data = page_buffer + SECTOR_SZ * i;
		uint8_t *sector_oob = page_buffer + SECTOR_SZ * SECTORS_PER_PAGE + OOB_SZ * i;
		sector_data[10] = (sector_data[10] & 0xF0) | ((~(sector_data[10] & 0x0F)) & 0x0F);
		unsigned int errloc[BCH_T];
		memset(errloc,0,BCH_T*sizeof(int));
		int ret = decode_bch(bch,sector_data,SECTOR_SZ,sector_oob,NULL,NULL,errloc);
		printf("Decode ret: %d\n",ret);
		int j=0;
		for(j=0;j<BCH_T;j++){
			printf("%d,",errloc[j]);
			sector_data[errloc[j]/8] ^= 1 << (errloc[j] % 8);
		}
		printf("\n");
		//sector_data[errloc[n]/8] ^= 1 << (errloc[n] % 8);
		memset(errloc,0,BCH_T*sizeof(int));
	        ret = decode_bch(bch,sector_data,SECTOR_SZ,sector_oob,NULL,NULL,errloc);			
		printf("Decode validation ret: %d\n",ret);
	}

	fclose(fda);

}
