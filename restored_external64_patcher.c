#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "patchfinder64.c"

#define GET_OFFSET(len, x) (x - (uintptr_t) restored_external)

typedef unsigned long long addr_t;

static uint32_t arm64_branch_instruction(uintptr_t from, uintptr_t to) {
  return from > to ? 0x18000000 - (from - to) / 4 : 0x14000000 + (to - from) / 4;
}


void exception() {
        printf("patch not found!\n");
    	exit(1);
}

static addr_t
cbz_ref64_back(const uint8_t *buf, addr_t start, size_t length) {

    //find cbz/cbnz
    uint32_t cbz_mask = 0x7E000000;
    uint32_t instr = 0;
    uint32_t offset = 0;
    uint32_t imm = 0;
    addr_t cbz = start;
    while (cbz) {
        instr = *(uint32_t *) (buf + cbz);
        if ((instr & cbz_mask) == 0x34000000) {
            imm = ((instr & 0x00FFFFFF) >> 5) << 2;
            offset = imm;
            if (cbz + offset == start)
                return cbz;
        }
        cbz -= 4;
    }

    return 0;

}


int get_skip_sealing_patch(void *restored_external, size_t len) {

    printf("len: %zu\n", len);

	printf("getting %s()\n", __FUNCTION__);

	void *skipping_sealing = memmem(restored_external,len,"Skipping sealing system volume", strlen("Skipping sealing system volume"));
    if (!skipping_sealing) {
    	exception();
    }

	printf("[*] Skipping sealing system volume string at 0x%llx\n", (int64_t) GET_OFFSET(len, skipping_sealing));

    addr_t ref_skipping_sealing = xref64(restored_external,0,len,(addr_t)GET_OFFSET(len, skipping_sealing));
    
    if(!ref_skipping_sealing) {
    	exception();
    }

    printf("[*] Skipping sealing system volume xref at 0x%llx\n", (int64_t) ref_skipping_sealing);

    addr_t ref_ref_skipping_sealing = cbz_ref64_back(restored_external, ref_skipping_sealing, ref_skipping_sealing);

    //iOS 15
    if(!ref_ref_skipping_sealing) {
        ref_skipping_sealing -= 4;
    	ref_ref_skipping_sealing = cbz_ref64_back(restored_external, ref_skipping_sealing, ref_skipping_sealing);
    }

    if(!ref_ref_skipping_sealing) {
    	exception();
    }

    printf("[*] Skipping sealing system volume branch to xref at 0x%llx\n", (int64_t) ref_ref_skipping_sealing);

    printf("[*] Assembling arm64 branch\n");

    uintptr_t ref1 = (uintptr_t)ref_ref_skipping_sealing;
 
    uintptr_t ref2 = (uintptr_t)ref_skipping_sealing;

    uint32_t our_branch = arm64_branch_instruction(ref1, ref2);

    *(uint32_t *) (restored_external + ref_ref_skipping_sealing) = our_branch;

    return 0;

}


int main(int argc, char* argv[]) { 

	if (argc < 3) {
		printf("Incorrect usage!\n");
		printf("Usage: %s [restored_external] [Patched restored_external]\n", argv[0]);
		return -1;
	}

	char *in = argv[1];
	char *out = argv[2];

	void *restored_external;
	size_t len;

	 FILE* fp = fopen(in, "rb");
     if (!fp) {
     	printf("[-] Failed to open restored_external\n");
     	return -1;
     }

    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    restored_external = (void*)malloc(len);
    if(!restored_external) {
        printf("[-] Out of memory\n");
        fclose(fp);
        return -1;
    }

    fread(restored_external, 1, len, fp);
    fclose(fp);

    get_skip_sealing_patch(restored_external,len);


    printf("[*] Writing out patched file to %s\n", out);

    fp = fopen(out, "wb+");

    fwrite(restored_external, 1, len, fp);
    fflush(fp);
    fclose(fp);
    
    free(restored_external);

    
    return 0;

}
