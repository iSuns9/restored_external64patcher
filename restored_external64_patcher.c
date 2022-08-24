#ifdef __gnu_linux__
    #define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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

/*function imported from xerub's patchfinder64*/

static addr_t
xref64(const uint8_t *buf, addr_t start, addr_t end, addr_t what)
{
    addr_t i;
    uint64_t value[32];

    memset(value, 0, sizeof(value));

    end &= ~3;
    for (i = start & ~3; i < end; i += 4) {
        uint32_t op = *(uint32_t *)(buf + i);
        unsigned reg = op & 0x1F;
        if ((op & 0x9F000000) == 0x90000000) {
            signed adr = ((op & 0x60000000) >> 18) | ((op & 0xFFFFE0) << 8);
            //printf("%llx: ADRP X%d, 0x%llx\n", i, reg, ((long long)adr << 1) + (i & ~0xFFF));
            value[reg] = ((long long)adr << 1) + (i & ~0xFFF);
            continue;				// XXX should not XREF on its own?
        /*} else if ((op & 0xFFE0FFE0) == 0xAA0003E0) {
            unsigned rd = op & 0x1F;
            unsigned rm = (op >> 16) & 0x1F;
            //printf("%llx: MOV X%d, X%d\n", i, rd, rm);
            value[rd] = value[rm];*/
        } else if ((op & 0xFF000000) == 0x91000000) {
            unsigned rn = (op >> 5) & 0x1F;
            unsigned shift = (op >> 22) & 3;
            unsigned imm = (op >> 10) & 0xFFF;
            if (shift == 1) {
                imm <<= 12;
            } else {
                //assert(shift == 0);
                if (shift > 1) continue;
            }
            //printf("%llx: ADD X%d, X%d, 0x%x\n", i, reg, rn, imm);
            value[reg] = value[rn] + imm;
        } else if ((op & 0xF9C00000) == 0xF9400000) {
            unsigned rn = (op >> 5) & 0x1F;
            unsigned imm = ((op >> 10) & 0xFFF) << 3;
            //printf("%llx: LDR X%d, [X%d, 0x%x]\n", i, reg, rn, imm);
            if (!imm) continue;			// XXX not counted as true xref
            value[reg] = value[rn] + imm;	// XXX address, not actual value
        /*} else if ((op & 0xF9C00000) == 0xF9000000) {
            unsigned rn = (op >> 5) & 0x1F;
            unsigned imm = ((op >> 10) & 0xFFF) << 3;
            //printf("%llx: STR X%d, [X%d, 0x%x]\n", i, reg, rn, imm);
            if (!imm) continue;			// XXX not counted as true xref
            value[rn] = value[rn] + imm;	// XXX address, not actual value*/
        } else if ((op & 0x9F000000) == 0x10000000) {
            signed adr = ((op & 0x60000000) >> 18) | ((op & 0xFFFFE0) << 8);
            //printf("%llx: ADR X%d, 0x%llx\n", i, reg, ((long long)adr >> 11) + i);
            value[reg] = ((long long)adr >> 11) + i;
        } else if ((op & 0xFF000000) == 0x58000000) {
            unsigned adr = (op & 0xFFFFE0) >> 3;
            //printf("%llx: LDR X%d, =0x%llx\n", i, reg, adr + i);
            value[reg] = adr + i;		// XXX address, not actual value
        }
        if (value[reg] == what) {
            return i;
        }
    }
    return 0;
}

int get_skip_sealing_patch(void *restored_external, size_t len) {

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

int get_skip_baseband_update_patch(void *restored_external, size_t len) {

    //printf("len: %zu\n", len);

	printf("getting %s()\n", __FUNCTION__);

	void *skipping_baseband = memmem(restored_external,len,"device does not have a baseband, update skipped", strlen("device does not have a baseband, update skipped"));
    if (!skipping_baseband) {
    	exception();
    }

	printf("[*] device does not have a baseband, update skipped string at 0x%llx\n", (int64_t) GET_OFFSET(len, skipping_baseband));

    addr_t ref_skipping_baseband = xref64(restored_external,0,len,(addr_t)GET_OFFSET(len, skipping_baseband));

    if(!ref_skipping_baseband) {
    	exception();
    }

    printf("[*] device does not have a baseband, update skipped xref at 0x%llx\n", (int64_t) ref_skipping_baseband);

    addr_t ref_ref_skipping_baseband = cbz_ref64_back(restored_external, ref_skipping_baseband, ref_skipping_baseband);


    if(!ref_ref_skipping_baseband) {
    	exception();
    }

    printf("[*] device does not have a baseband, update skipped branch to xref at 0x%llx\n", (int64_t) ref_ref_skipping_baseband);

    printf("[*] Assembling arm64 branch\n");

    uintptr_t ref1 = (uintptr_t)ref_ref_skipping_baseband;

    uintptr_t ref2 = (uintptr_t)ref_skipping_baseband;

    uint32_t our_branch = arm64_branch_instruction(ref1, ref2);

    *(uint32_t *) (restored_external + ref_ref_skipping_baseband) = our_branch;

    return 0;

}


int main(int argc, char* argv[]) {

    int skip_bbupdate = 0;

	if (argc < 3) {
		printf("Incorrect usage!\n");
		printf("Usage: %s [restored_external] [Patched restored_external] [-b]\n  -b    skip baseband update\n", argv[0]);
		return -1;
	}
	if (argc == 4 && !strcmp(argv[3], "-b")) {
        skip_bbupdate = 1;
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

    printf("file size: %lu\n", len);

    if (skip_bbupdate)
        get_skip_baseband_update_patch(restored_external,len);
    else
        get_skip_sealing_patch(restored_external,len);


    printf("[*] Writing out patched file to %s\n", out);

    fp = fopen(out, "wb+");

    fwrite(restored_external, 1, len, fp);
    fflush(fp);
    fclose(fp);
    
    free(restored_external);

    
    return 0;

}
