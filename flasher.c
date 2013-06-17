#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vs1000.h>
#include <audio.h>

#include <minifat.h>
#include <vsNand.h>

extern struct FsPhysical *ph;
extern struct FsNandPhys fsNandPhys;
extern u_int16 mallocAreaX[0x1000];

__y const char hex[] = "0123456789abcdef";
void puthex(u_int16 a) {
  char tmp[6];
  tmp[0] = hex[(a>>12)&15];
  tmp[1] = hex[(a>>8)&15];
  tmp[2] = hex[(a>>4)&15];
  tmp[3] = hex[(a>>0)&15];
  tmp[4] = ' ';
  tmp[5] = '\0';
  fputs(tmp,stdout);
}

void PrintSector(void) {
    int i;
    for (i=0;i<256;i++) {
	puthex(minifatBuffer[i]);
	if ((i & 7)==7)
	    puts("");
    }
}

struct KeyMapping {
  u_int16 key;
  enum keyEvent event;
};
extern const struct KeyMapping sixKeyMap[];
extern const struct KeyMapping *currentKeyMap;
void UserInterfaceIdleHook(void);

void main(void) {
    FILE *fp;
    long size;

    SetHookFunction((u_int16)IdleHook, UserInterfaceIdleHook);

    applAddr = NULL;
    currentKeyMap = sixKeyMap;
    ph = &fsNandPhys;
    //fsNandPhys.nandType = 0; /* 16MB small-page */
    fsNandPhys.waitns = 100;

    USEX(SCI_STATUS) |= SCISTF_ANADRV_PDOWN; /* quiet it.. */

    ph->Read(ph, 0, 1, mallocAreaX, NULL); /* read first sector */

    PERIP(INT_ENABLEL) &= ~INTF_RX; /* prevent jump to monitor when IO done!!*/
    if (fp = fopen("boot.img", "rb")) {
	int len;
	if (mallocAreaX[0] == (('V'<<8)|'L') &&
	    mallocAreaX[1] == (('S'<<8)|'I')) {
	    /* NAND info not needed, discard it */
	    puts("info ok");
	    fseek(fp, 10, SEEK_SET); /* 10 bytes = 5 words */
	} else {
	    puts("info from file");
	    /* NAND info not present, read it */
	    fread(mallocAreaX, 5, 1, fp);
	}
	memset(mallocAreaX+5, 0xffff, 0x1000-5);

	fsNandPhys.nandType = mallocAreaX[2];
	puthex(fsNandPhys.nandType);
	puts("=type");
	puthex(mallocAreaX[0]);
	puthex(mallocAreaX[1]);
	puthex(mallocAreaX[2]);
	puthex(mallocAreaX[3]);
	puthex(mallocAreaX[4]);

	size = fread(mallocAreaX+5, 1, 0x1000-5, fp) + 6;
	puthex(size);
	puts("=size");

	puts("Erasing");
	ph->Erase(ph, 0); /* erase first XX sectors */

	puthex(mallocAreaX[5]);
	puts("=blocks");
	puthex((size+255)/256);
	puts("=Writing");
	puthex(ph->Write(ph, 0, (size+255)/256, mallocAreaX, NULL));
	puts("fclose()");
	fclose(fp);
    } else {
	puts("boot.img not found!");
    }
    puts("restart");
    fflush(stdout);
 PERIP(INT_ENABLEL) |= INTF_RX; 
}
