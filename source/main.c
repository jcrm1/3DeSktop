#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

#include <fcntl.h>

#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <3ds.h>

#define QOI_IMPLEMENTATION
#include "qoi.h"

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000
#define BUF_SIZE (800000)

typedef struct ip_address {
  char *address;
  unsigned short port;
} ip_address_t;

static u32 *SOC_buffer = NULL;
s32 sock = -1;

__attribute__((format(printf,1,2)))
void failExit(const char *fmt, ...);


//---------------------------------------------------------------------------------
void socShutdown() {
//---------------------------------------------------------------------------------
	printf("waiting for socExit...\n");
	socExit();

}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
	int ret;

	struct sockaddr_in server;

	gfxInitDefault();

	// register gfxExit to be run when app quits
	// this can help simplify error handling
	atexit(gfxExit);

	consoleInit(GFX_BOTTOM, NULL);
	gfxSetDoubleBuffering(GFX_TOP, false);
	u8 *frameBuf = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);

	printf ("\nlibctru sockets demo\n");

	// allocate buffer for SOC service
	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);

	if(SOC_buffer == NULL) {
		failExit("memalign: failed to allocate\n");
	}

	// Now intialise soc:u service
	if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
    	failExit("socInit: 0x%08X\n", (unsigned int)ret);
	}
	atexit(socShutdown);

	memset(&server, 0, sizeof (server));

	static SwkbdState swkbd;
	swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 1, 21);
	swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
	swkbdSetFeatures(&swkbd, SWKBD_FIXED_WIDTH);
	swkbdSetNumpadKeys(&swkbd, L'.', L':');
	swkbdSetHintText(&swkbd, "000.000.000.000:00000");
	char ipBuf[22];
	swkbdInputText(&swkbd, ipBuf, sizeof(ipBuf));
	ip_address_t *ip_address = (ip_address_t *) malloc(sizeof(ip_address_t));
  ip_address->address = (char *) malloc(sizeof(char) * 16);
  int status = sscanf(ipBuf, "%15[0-9.]:%hd", ip_address->address, &(ip_address->port));
  if (status != 2) {
    failExit("Improper address format (IP address:port) status: %i\n", status);
    return -1;
  }
	gfxFlushBuffers();
	printf("Connecting to %s:%d\n", ip_address->address, ip_address->port);

	server.sin_family = AF_INET;
	server.sin_port = htons(ip_address->port);
	server.sin_addr.s_addr = inet_addr(ip_address->address);

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	unsigned char *data = malloc(BUF_SIZE);
	unsigned char *agg_data = malloc(BUF_SIZE);
	u32 kDown = hidKeysDown();
	if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
		failExit("connect: %s\n", strerror(errno));
	}
	printf("Connected\n");
	int numBytesRead = 0;
	int totalBytesRead = 0;
	int sizeOfFrame = -1;
	qoi_desc desc = {240, 400, 3, QOI_SRGB};
	int flag = 1;
	while (aptMainLoop() && flag == 1) {
		gspWaitForVBlank();
		hidScanInput();
		kDown = hidKeysDown();
		if (kDown & KEY_START) break;
		if (kDown & KEY_SELECT) flag = 0;
		if (sizeOfFrame == -1) {
			read(sock, &sizeOfFrame, 4);
			if (sizeOfFrame > BUF_SIZE) {
				failExit("Frame size too large: %i\n", sizeOfFrame);
			}
		}
		if (sizeOfFrame - totalBytesRead == 0) {
			failExit("Did not reset totalBytesRead S %i T %i\n", sizeOfFrame, totalBytesRead);
		}
		numBytesRead = read(sock, data, sizeOfFrame - totalBytesRead);
		if (numBytesRead == 0) {
			failExit("No bytes read\n");
		}
		memcpy(agg_data + totalBytesRead, data, numBytesRead);
		totalBytesRead += numBytesRead;
		if (sizeOfFrame == totalBytesRead) {
			void *qoi = qoi_decode(agg_data, sizeOfFrame, &desc, 3);
			if (qoi == NULL) {
				failExit("qoi_decode failed\n");
			}
			memcpy(frameBuf, qoi, 400 * 240 * 3);
			free(qoi);
			totalBytesRead = 0;
			sizeOfFrame = -1;
		}

	}

	close(sock);
	free(data);
	free(agg_data);
	free(ip_address->address);
	free(ip_address);
	return 0;
}

//---------------------------------------------------------------------------------
void failExit(const char *fmt, ...) {
//---------------------------------------------------------------------------------

	if(sock>0) close(sock);

	va_list ap;

	printf(CONSOLE_RED);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf(CONSOLE_RESET);
	printf("\nPress B to exit\n");

	while (aptMainLoop()) {
		gspWaitForVBlank();
		hidScanInput();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_B) exit(0);
	}
}
