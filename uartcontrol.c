#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vs1000.h>
#include <audio.h>
#include <mappertiny.h>
#include <minifat.h>
#include <codec.h>
#include <vsNand.h>
#include <player.h>
#include <usblowlib.h>

#include <dev1000.h>
#include "uart.h"

#define TYPE_TEXT_FILE /* 'T' and 't' commands */

extern struct FsPhysical *ph;
extern struct FsMapper *map;
extern struct Codec *cod;
extern struct CodecServices cs;

#if 0
const struct KeyMapping myKeyMap[] = {
  {KEY_POWER, ke_next},
  {KEY_LONG_PRESS|KEY_POWER, ke_next},
  {KEY_1, ke_next},
  {KEY_LONG_PRESS|KEY_1, ke_next},
};

void MyKeyEventHandler(enum keyEvent event) { /*140 words*/
    if (event == ke_next) {
	if (player.pauseOn) {
	    /* If pressed in pause mode, keep playing the same */
	    if (player.currentFile >= player.totalFiles)
		player.currentFile = 0;
	    player.nextFile = player.currentFile;
	} else {
	    /* If pressed during play, set the next file */
	    player.nextFile = ++player.currentFile;
	}
	player.pauseOn = 0;
	cs.cancel = 1;
	return;
    }
    RealKeyEventHandler(event);
}
#endif

/* Our own loadcheck which will use fixed 3.0x clock for player
   and 4.0x for USB. */
void MyLoadCheck(struct CodecServices *cs, s_int16 n) {
  if (cs == NULL) {
    if (n) {
	if (clockX != 8) {
	    clockX = 8; /* USB requires 4.0x! */
	    SetRate(hwSampleRate);
	}
	return;
    }
    if (clockX != 6) {
	clockX = 6; /* Otherwise 3.0x */
	SetRate(hwSampleRate);
    }
    return;
  }
  /* cs is always non-NULL here */
  if (cs->gain != player.volumeOffset) {
    player.volumeOffset = cs->gain; /* vorbis-gain */
    PlayerVolume();
  }
}

void MyUserInterfaceIdleHook(void) { /*94 words*/
    if (UartFill()/*PERIP(UART_STATUS) & UART_ST_RXFULL*/) {
	int cmd = UartGetByte();
	if (cmd == 'C') {
	    cs.cancel = 1;
	    player.pauseOn = 0;
	    putch('c');
	} else if (cmd == '+') {
	    KeyEventHandler(ke_volumeUp2);
	    putch(player.volume);
	} else if (cmd == '-') {
	    KeyEventHandler(ke_volumeDown2);
	    putch(player.volume);
	} else if (cmd == 'p') {
	    /* pause */
	    KeyEventHandler(ke_pauseToggle);
	    putch(player.pauseOn ? 'p' : 'P');
	} else {
	    putch(cmd);
	}
    }
    if (uiTrigger) {
	uiTrigger = 0;
	/* ~16 times per second */
    }
}


#ifdef FLAT_DIR
void IterateFilesCallback(register __b0 u_int16 *name) {
    register int i;
    for (i=0;i<32/2;i++) {
	putch(*name >> 8);
	putch(*name++);
    }
}
#else/*FLAT_DIR*/


#define MAXDIRDEPTH 32
int dirDepth = 0;
u_int32 dirRecurse[MAXDIRDEPTH];
//u_int16 dirLocation[MAXDIRDEPTH];

#define MAXDIRS 16
int dirs = 0;
u_int32 dirCluster[MAXDIRS];
u_int16 dirNames[MAXDIRS][12/2];

void DirEntry(u_int16 *entry) {
//    PrintEntry(entry);
//    puts(" DIR");
#if 1
    register int i;
    putch('D');
    for (i=0;i<32/2;i++) {
	putch(*entry >> 8);
	putch(*entry++);
    }
    putch('\n');
    entry -= 32/2;
#endif

    if (dirs < MAXDIRS) {
	/* Must save start cluster to be able to enter the directory. */
	memcpy(dirNames[dirs], entry, 12/2);
	dirCluster[dirs] =
	    ((u_int32)SwapWord(entry[20/2]) << 16) | SwapWord(entry[26/2]);
	dirs++;
    }
}


#define MAXFILES 64
#ifdef MAXFILES
int files = 0;
u_int32 fileSize[MAXFILES];
u_int32 fileCluster[MAXFILES];
u_int16 fileNames[MAXFILES][12/2];
#endif

void FileEntry(u_int16 *entry) {
//    PrintEntry(entry);
//    puts(" FILE");
#if 1
    register int i;
    putch('F');
    for (i=0;i<32/2;i++) {
	putch(*entry >> 8);
	putch(*entry++);
    }
    putch('\n');
    entry -= 32/2;
#endif

#ifdef MAXFILES
    if (files < MAXFILES) {
	/* Must save file size and file start cluster to be able to
	   open the file at a later time. The files can not
	   be opened by a number because the numbering is different,
	   and they can't be opened by name because there can be
	   several files with the same name in different directories.
	*/
	memcpy(fileNames[files], entry, 12/2);
	fileSize[files] =
	    ((u_int32)SwapWord(entry[30/2]) << 16) | SwapWord(entry[28/2]);
	fileCluster[files] =
	    ((u_int32)SwapWord(entry[20/2]) << 16) | SwapWord(entry[26/2]);
	files++;
    }
#endif
}

/*
  To open a file:

  minifatInfo.fileSize = fileSize[n];
  minifatInfo.filePos = 0;
  FatFragmentList(&minifatFragments[0], fileCluster[n]);
*/


auto void FatListDir(u_int32 cluster) {
    register __c1 int i;
    register u_int32 currentSector;
    register __i2 __y struct FRAGMENT *curFragment = &minifatFragments[2];

#ifdef FAT_LFN_SIZE
    minifatInfo.longFileName[0] = 0;
#endif
    if (cluster == 0) {
	/* Start at the start of root directory. */
	if (minifatInfo.IS_FAT_32) {
	    /*TODO: use root dir pointer instead of 2! */
	    FatFragmentList(curFragment, 2);
	} else {
	    curFragment->start = minifatInfo.rootStart | LAST_FRAGMENT;
	    curFragment->size  = (minifatInfo.BPB_RootEntCnt >> 4);
	}
    } else {
	FatFragmentList(curFragment, cluster);
    }

    currentSector = curFragment->start & 0x7fffffffUL;
    i = 0;
    while (1) {
	register __d1 u_int16 fn;
	if (i == 0) {
	    ReadDiskSector(minifatBuffer,
			   minifatInfo.currentSector = currentSector);
	}
	/* We are now looking at FAT directory structure. */
	/* Is current file a regular file? */

	fn = FatGetByte(i+0); /* first char of filename */
	if (fn == 0)
	    return;

	if (fn != 0xe5) {
	    __y u_int16 attrib = FatGetByte(i+11/*Attr*/);

#ifdef FAT_LFN_SIZE
	    /* long filename */
	    if ((attrib & 0x3f) == 0x0f) {
		register u_int16 idx = ((fn & 0x3f) - 1) * 13;
		static const short places[] = {
		    1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30, -1
		};
		register const short *p = &places[0];
		while (*p >= 0 && idx < FAT_LFN_SIZE-1 /* space for NUL */) {
#ifdef FATINFO_IN_Y
#define WRITEPACKED MemWritePackedY
#else
#define WRITEPACKED MemWritePacked
#endif
		    WRITEPACKED(minifatInfo.longFileName, idx++,
				FatGetByte(i + *p++));
		}
		/* last long entry ? */
		if ( fn & 0x40 /*LAST_LONG_ENTRY*/ ) {
		    if (idx >= FAT_LFN_SIZE)
			idx = FAT_LFN_SIZE-1;
#ifdef __VSDSP__
		    WRITEPACKED(minifatInfo.longFileName, idx, '\0');
#else
		    ((char *)minifatInfo.longFileName)[idx] = '\0';
#endif
		}
	    } else
#endif/*FAT_LFN_SIZE*/
	    {
		/* Is it a subdirectory? */
		if ((attrib & 0x10) && fn != '.'
		    /* Subdirectories are not supported for FAT12 */
		    && minifatInfo.FilSysType != 0x3231
		    ) {
		    /* do not process the subdirectory recursively */

		    DirEntry(minifatBuffer + i/2);

		} else if ((attrib & 0xde) == 0
#if 0 /*matching / all files*/
			   && FatCheckFileType(FatGetLong(i+8))
#endif
		    ) {

		    /* It is a regular file. */
		    FileEntry(minifatBuffer + i /2);

		    /* ------------ FILE FOUND ------------- */

		    //minifatInfo.fileSize = FatGetLong(i+28);
		    //minifatInfo.filePos = 0;
		    //FatFragmentList(&minifatFragments[0],
		    //((u_int32)FatGetWord(i+20) << 16) +
		    //FatGetWord(i+26));
		} /* normal file */
#ifdef FAT_LFN_SIZE
		minifatInfo.longFileName[0] = 0;
#endif
	    }
	} /* 0xe5 deleted entry */
	i = (i + 32) & 511;
	if (i == 0) {
	    /* if end of directory block, get the next one */
	    currentSector++;
	    if (--(curFragment->size) == 0) {
		if ((s_int32)curFragment->start < 0)
		    return;
		curFragment++;
		currentSector = curFragment->start & 0x7fffffffUL;
	    }
	}
    }
}
#endif/*elseFLAT_DIR*/


void main(void) {
#if 1 /*Perform some extra inits because we are started from SPI boot. */
    InitAudio(); /* goto 3.0x..4.0x */
    PERIP(INT_ENABLEL) = INTF_RX | INTF_TIM0;
    PERIP(INT_ENABLEH) = INTF_DAC;
    ph = FsPhNandCreate(0);
#endif

#if 0
    /* Set differential audio mode, i.e. invert the other channel. */
    audioPtr.leftVol = -audioPtr.rightVol;
#endif

    /* Set the leds after nand-boot! */
    PERIP(GPIO1_ODATA) |=  LED1|LED2;      /* POWER led on */
    PERIP(GPIO1_DDR)   |=  LED1|LED2; /* SI and SO to outputs */
    PERIP(GPIO1_MODE)  &= ~(LED1|LED2); /* SI and SO to GPIO */

    /* set cs.sampleRate to non-zero because we may use cs.Output() */
    SetRate(cs.sampleRate = 8000U);

    /* Install new keymap and our key event handler */
//    currentKeyMap = myKeyMap;
//    SetHookFunction((u_int16)KeyEventHandler, MyKeyEventHandler);

    /* No automatic poweroff from low-power state */
//    SetHookFunction((u_int16)PowerOff, NullHook);
    SetHookFunction((u_int16)IdleHook, NullHook);     /* no default keyscan */
    SetHookFunction((u_int16)USBSuspend, NullHook);   /* no low-power state */
    SetHookFunction((u_int16)LoadCheck, MyLoadCheck); /* fixed clock */

    /* Replicate main loop */
    player.volume = 17;
    player.volumeOffset = -24;
    player.pauseOn = 0;
    //player.randomOn = 0;
    keyOld = KEY_POWER;
    keyOldTime = -32767; /* ignores the first release of KEY_POWER */

    PlayerVolume();

    //if (!map)
    {
	map = FsMapTnCreate(ph, 0); /* tiny mapper */
    }
    Disable();
    uartRxWrPtr = uartRxRdPtr = uartRxBuffer;
    PERIP(INT_ENABLEL) |= INTF_RX;
    Enable();
//	    PERIP(UART_DATA); /*read out any remaining char*/
    while (UartFill()/*PERIP(UART_STATUS) & UART_ST_RXFULL*/) {
	UartGetByte();
    }

    while (1) {
	if (USBIsAttached()) {
	    putch('a'); /*Attached*/
	    putch('\n');
    SetHookFunction((u_int16)LoadCheck, RealLoadCheck); /* fixed clock */
	    MassStorage();
    SetHookFunction((u_int16)LoadCheck, MyLoadCheck); /* fixed clock */
	    putch('d'); /*Detached*/
	    putch('\n');
	}

	/* Try to init FAT. */
	if (InitFileSystem() == 0) {
	    static u_int16 buffer[80] = "\0", inlen = 0;

	    dirRecurse[dirDepth] = 0;

	    putch('f');
	    putch('a');
	    putch('t');
	    putch('\n');
	    putch(minifatInfo.totSize>>24);
	    putch(minifatInfo.totSize>>16);
	    putch(minifatInfo.totSize>>8);
	    putch(minifatInfo.totSize>>0);

#if 1
#ifdef FLAT_DIR
	    minifatInfo.supportedSuffixes = NULL; /*all files*/
	    IterateFiles();
#else
#ifdef MAXFILES
	    files =
#endif
		dirs = 0;
	    FatListDir(dirRecurse[dirDepth]);
#endif
#endif
	    while (1) {

		player.currentFile = -1;
		if (UartFill()/*PERIP(UART_STATUS) & UART_ST_RXFULL*/) {
		    int cmd = UartGetByte();//PERIP(UART_DATA);
		    if (cmd == '\n' || inlen == sizeof(buffer)-1) {
			u_int16 fName[8/2];
			fName[0] = (buffer[1]<<8) | buffer[2];
			fName[1] = (buffer[3]<<8) | buffer[4];
			fName[2] = (buffer[5]<<8) | buffer[6];
			fName[3] = (buffer[7]<<8) | buffer[8];

			if (!memcmp(buffer, "OFF\0", 4)) {
			    putch('o');
			    RealPowerOff();
			}
			if (buffer[0] == 'L') {
			    /* List files TODO: per directory */
#ifdef FLAT_DIR
			    minifatInfo.supportedSuffixes = NULL; /*all files*/
			    IterateFiles();
#else
#ifdef MAXFILES
			    files =
#endif
				dirs = 0;
			    FatListDir(dirRecurse[dirDepth]);
#endif
			}
			if (buffer[0] == 'C') { /* cd to directory name */
			    int i, got = 0;
			    for (i=0;i<dirs;i++) {
				if (!memcmp(dirNames[i], fName, 8/2)) {
				    /* Match */
				    putch(i);
#ifdef MAXFILES
				    files =
#endif
					dirs = 0;
				    //dirLocation[dirDepth] = selectedLine;
				    dirRecurse[++dirDepth] = dirCluster[i];
				    FatListDir(dirCluster[i]);
				    got = 1;
				    break;
				}
			    }
			    if (!got)
				putch(-1);
			}
			if (buffer[0] == 'c') { /* cd to directory N */
			    u_int16 selectedLine =
				strtol((void *)&buffer[1],NULL,10);
			    if (selectedLine < dirs) {
#ifdef MAXFILES
				files =
#endif
				    dirs = 0;
				//dirLocation[dirDepth] = selectedLine;
				dirRecurse[++dirDepth] =
				    dirCluster[selectedLine];
				FatListDir(dirCluster[selectedLine]);
				//selectedLine = 0;
				putch(selectedLine);
			    } else {
				putch(-1);
			    }
			}
			if (buffer[0] == '.') { /* cd to parent */
			    if (dirDepth) {
				putch('.');
#ifdef MAXFILES
				files =
#endif
				    dirs = 0;
				FatListDir(dirRecurse[--dirDepth]);
				//selectedLine = dirLocation[dirDepth];
			    } else {
				putch(-1);
			    }
			}
			if (buffer[0] == 'P') { /* Play by name */
			    u_int16 num =
				OpenFileNamed(fName, FAT_MKID('O','G','G'));
			    if (num != 0xffffU) {
				player.ffCount = 0;
				player.pauseOn = 0;
				cs.cancel = 0;
				cs.goTo = -1; /* start playing from the start*/
				cs.fileSize = cs.fileLeft =
				    minifatInfo.fileSize;
				cs.fastForward = 1; /* play speed to normal */

				SetHookFunction((u_int16)IdleHook,
						MyUserInterfaceIdleHook);
				putch(num);
				PlayCurrentFile();
				SetHookFunction((u_int16)IdleHook, NullHook);
				putch('w');
				putch('\n');
			    } else {
				putch(-1);
			    }
			}
#ifdef TYPE_TEXT_FILE
			if (buffer[0] == 'T') { /* Type by name */
			    u_int16 num =
				OpenFileNamed(fName, FAT_MKID(buffer[9],
							      buffer[10],
							      buffer[11]));
			    if (num != 0xffffU) {
				int l;
				putch(num);

				while (l = ReadFile(buffer, 0,
						    2*sizeof(buffer))) {
				    register int i;
				    for (i=0; i<l/2; i++) {
					putch(buffer[i]>>8);
					putch(buffer[i]);
				    }
				    if (l & 1)
					putch(buffer[l/2]>>8);
				}
				putch('w');
				putch('\n');
			    } else {
				putch(-1);
			    }
			}
#endif/*TYPE_TEXT_FILE*/
#ifdef MAXFILES
			/* Only possible if we have collected the names */
			if (buffer[0] == 'p') { /* Play by number */
			    u_int16 num = strtol((void *)&buffer[1],NULL,10);
			    putch(num);
			    if (num < files) {

				minifatInfo.fileSize = fileSize[num];
				minifatInfo.filePos = 0;
				FatFragmentList(&minifatFragments[0],
						fileCluster[num]);

				player.ffCount = 0;
				player.pauseOn = 0;
				cs.cancel = 0;
				cs.goTo = -1; /* start playing from the start*/
				cs.fileSize = cs.fileLeft =
				    minifatInfo.fileSize;
				cs.fastForward = 1; /* play speed to normal */

				SetHookFunction((u_int16)IdleHook,
						MyUserInterfaceIdleHook);
				putch(num);
				PlayCurrentFile();
				SetHookFunction((u_int16)IdleHook, NullHook);
				putch('w');
				putch('\n');
			    } else {
				putch(-1);
			    }
			}
#ifdef TYPE_TEXT_FILE
			if (buffer[0] == 't') { /* Type by number */
			    u_int16 num = strtol((void *)&buffer[1],NULL,10);
			    putch(num);
			    if (num < files) {
				int l;
				minifatInfo.fileSize = fileSize[num];
				minifatInfo.filePos = 0;
				FatFragmentList(&minifatFragments[0],
						fileCluster[num]);
				putch(num);
				while (l = ReadFile(buffer, 0,
						    2*sizeof(buffer))) {
				    register int i;
				    for (i=0; i<l/2; i++) {
					putch(buffer[i]>>8);
					putch(buffer[i]);
				    }
				    if (l & 1)
					putch(buffer[l/2]>>8);
				}
				putch('w');
				putch('\n');
			    } else {
				putch(-1);
			    }
			}
#endif/*TYPE_TEXT_FILE*/
#endif/*MAXFILES*/
			inlen = 0;
			buffer[0] = 0;
		    } else {
			buffer[inlen++] = cmd;
			buffer[inlen] = '\0';
		    }
		}
#if 1
		memset(tmpBuf, 0, sizeof(tmpBuf)); /* silence */
		AudioOutputSamples(tmpBuf, sizeof(tmpBuf)/2); /* no pause */
		//cs.Output(&cs, tmpBuf, sizeof(tmpBuf)/2);
#endif
	    }
	} else {
	    putch('n');
	    putch('o');
	    putch('t');
	    putch('\n');

	    /* If not a valid FAT (perhaps because MMC/SD is not inserted),
	       just send some samples to audio buffer and try again. */
	noFSnorFiles:
	    LoadCheck(&cs, 32); /* decrease or increase clock */
	    memset(tmpBuf, 0, sizeof(tmpBuf));
	    AudioOutputSamples(tmpBuf, sizeof(tmpBuf)/2);
	    /* When no samples fit, calls the user interface
	       -- handles volume control and power-off. */
	}
    }
}
