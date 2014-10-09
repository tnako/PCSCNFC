#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include "pcsclite.h"
#include "winscard.h"


#define SV_SOCK_PATH "/tmp/nfcIn"

#define TYPE_NEW  0x17
#define TYPE_DONE 0xFA


#define CHECKRV(f, rv) \
    if (SCARD_S_SUCCESS != rv) \
    { \
	printf(f ": %s\n", pcsc_stringify_error(rv)); \
	sleep(1); \
	continue; \
    }

#define CHECK(f, ret, checkVar) \
    if (ret != checkVar) \
    { \
	printf(f ": %s\n", strerror(errno)); \
	sleep(1); \
	continue; \
    }


const char cmd1[] = { 0xFF, 0xCA, 0x00, 0x00, 0x00 };

int socketFD = 0;

static int CreateSocket()
{
    if (socketFD == 0) {
        socketFD = socket(AF_UNIX, SOCK_STREAM, 0);
        if(socketFD < 0) {
            socketFD = 0;
            return -1;
        }
    } else {
        close(socketFD);
        socketFD = 0;
        return -2;
    }

    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SV_SOCK_PATH, sizeof(addr.sun_path)-1);

    if (connect(socketFD, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1){
        close(socketFD);
        socketFD = 0;
        return -3;
    }

    return 1;
}

int main(void)
{

    SCARDCONTEXT hContext = 0;
    LPTSTR mszReaders;
    SCARDHANDLE hCard;
    SCARD_READERSTATE rgReaderStates[1];
    SCARD_IO_REQUEST pioSendPci;

    unsigned long dwReaders, dwActiveProtocol, dwRecvLength;
    unsigned char pbRecvBuffer[258], socketBuffer[256], socketBufferLength;
    long rv, ret;
    ssize_t len_write;

    signal(SIGPIPE, SIG_IGN);

    while (1) {
	ret = CreateSocket();
	CHECK("CreateSocket", ret, 1);

        if (hContext == 0) {
            rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
            CHECKRV("SCardEstablishContext", rv);
        }

        dwReaders = SCARD_AUTOALLOCATE;

        rv = SCardListReaders(hContext, NULL, (LPTSTR)&mszReaders, &dwReaders);
        CHECKRV("SCardListReaders1", rv);

        rgReaderStates[0].szReader = &mszReaders[0];
        rgReaderStates[0].dwCurrentState = SCARD_STATE_EMPTY;

        printf("Waiting for card insertion -> ");
        fflush(stdout);
        rv = SCardGetStatusChange(hContext, INFINITE, rgReaderStates, 1);
        CHECKRV("SCardGetStatusChange", rv);

        rv = SCardConnect(hContext, mszReaders, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);
        CHECKRV("SCardConnect", rv);

        switch(dwActiveProtocol)
        {
        case SCARD_PROTOCOL_T0:
            pioSendPci = *SCARD_PCI_T0;
            break;

        case SCARD_PROTOCOL_T1:
            pioSendPci = *SCARD_PCI_T1;
            break;
        }

        dwRecvLength = sizeof(pbRecvBuffer);
        rv = SCardTransmit(hCard, &pioSendPci, (LPCBYTE)cmd1, sizeof(cmd1), NULL, (LPBYTE)pbRecvBuffer, &dwRecvLength);
        CHECKRV("SCardTransmit", rv);

	socketBufferLength = dwRecvLength;

        if (dwRecvLength > 2) {
            dwRecvLength -= 2;

            printf("UID: ");
            for (unsigned int i = 0; i < dwRecvLength; i++) {
                printf("%02X ", pbRecvBuffer[i]);
            }
            printf("-> ");

            socketBuffer[0] = socketBufferLength-1;
            socketBuffer[1] = TYPE_NEW;
            memcpy(socketBuffer + 2, pbRecvBuffer, socketBufferLength - 2);

            len_write = write(socketFD, socketBuffer, socketBufferLength);
	    CHECK("write TYPE_NEW", write, socketBufferLength);
        } else {
            printf("no response\n");
        }

        fflush(stdout);

        rgReaderStates[0].dwCurrentState = SCARD_STATE_PRESENT;

        printf("Waiting for card remove -> ");
        fflush(stdout);
        rv = SCardGetStatusChange(hContext, INFINITE, rgReaderStates, 1);
        CHECKRV("SCardGetStatusChange", rv);

        printf("OK!\n");

	if (socketBufferLength > 2) {
		socketBuffer[1] = TYPE_DONE;
		len_write = write(socketFD, socketBuffer, socketBufferLength);
		CHECK("write TYPE_NEW", write, socketBufferLength);

	}

        rv = SCardDisconnect(hCard, SCARD_LEAVE_CARD);
        CHECKRV("SCardDisconnect", rv);

        rv = SCardFreeMemory(hContext, mszReaders);
        CHECKRV("SCardFreeMemory", rv);

        rv = SCardReleaseContext(hContext);
        CHECKRV("SCardReleaseContext", rv);

        hContext = 0;

	close(socketFD);
	socketFD = 0;
    }

    return 0;
}
