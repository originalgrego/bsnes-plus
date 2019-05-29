#include <snes.hpp>
#include "../../../ui-qt/utility/utility.hpp"

#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>

#include <iostream>
#include <exception>

#define BUF_SIZE 1024
TCHAR SHARED_MEM_NAME[] = TEXT("Global\\MyFileMappingObject");

HANDLE createSharedMem() {
    return CreateFileMapping(INVALID_HANDLE_VALUE,    // use paging file
                             NULL,                    // default security
                             PAGE_READWRITE,          // read/write access
                             0,                       // maximum object size (high-order DWORD)
                             BUF_SIZE,                    // maximum object size (low-order DWORD)
                             SHARED_MEM_NAME);                 // name of mapping object
}

HANDLE connectToSharedMem() {
    return OpenFileMapping(FILE_MAP_ALL_ACCESS,   // read/write access
                    FALSE,                 // do not inherit the name
                    SHARED_MEM_NAME);               // name of mapping object
}
  
void* getView(HANDLE sharedMemoryHandle) {
    return (void*) MapViewOfFile(sharedMemoryHandle,   // handle to map object
                         FILE_MAP_ALL_ACCESS, // read/write permission
                         0,
                         0,
                         BUF_SIZE);
}

#define C0NET_CPP
namespace SNES {
    
    class FatFifo {
        
        public:
            
            FatFifo(uint8 * dataPointerRef, uint8 * statusPointerRef) {
                dataPointer = dataPointerRef;
                statusPointer = statusPointerRef;
                index = 0;
            }
            
            uint8 getNextIndex() {
                uint8 newIndex = index + 1;
                if (newIndex > 255) {
                    newIndex = 0;
                }
                return newIndex;
            }

        protected:
            
            uint8 * dataPointer;
            uint8 * statusPointer;
            uint8 index;
    };
    
    class FatFifoReader: public FatFifo {

        public:
            
            FatFifoReader(uint8* dataPointerRef, uint8* statusPointerRef) :
            FatFifo(dataPointerRef, statusPointerRef) { }

            bool isEmpty() {
                return statusPointer[index] == 0;
            }
            
            uint8 read() {
                uint8 value = dataPointer[index];
                if (!isEmpty()) {
                    statusPointer[index] = 0;
                    index = getNextIndex();
                }
                return value;
            }
    };
    
    class FatFifoWriter: public FatFifo {
        
        public: 

            FatFifoWriter(uint8* dataPointerRef, uint8* statusPointerRef) :
            FatFifo(dataPointerRef, statusPointerRef) {
                for (int x = 0; x < 256; x ++) {
                    statusPointer[x] = 0;
                }
            }

            bool isFull() {
                return statusPointer[index] == 1;
            }
            
            void write(uint8 value) {
                if (!isFull()) {
                    dataPointer[index] = value;
                    statusPointer[index] = 1;
                    index = getNextIndex();
                }
            }
    };
    
    C0Net c0Net;
    FatFifoReader * fifoReader = NULL;
    FatFifoWriter * fifoWriter = NULL;

    void C0Net::init() {
      reset();
      isClient = false;
      isServer = false;
    }

    void C0Net::enable() {
      memory::mmio.map(0x21C0, 0x21EF, *this);
    }

    void C0Net::power() {
      reset();
    }

    void C0Net::reset() {}
    
    void C0Net::toggleClient() {
        isClient = !isClient;
        if (isClient) {
            HANDLE sharedMem = connectToSharedMem();
            if (sharedMem == NULL) {
				isClient = false;
				tryServer();
				return;
            }
            void* view = getView(sharedMem);
            uint8* data = (uint8*)view;
            fifoWriter = new FatFifoWriter(&data[512], &data[768]);
            fifoReader = new FatFifoReader(data, &data[256]);
        }
    }

    void C0Net::toggleServer() {
		toggleClient();
    }
	
	void C0Net::tryServer() {
        isServer = !isServer;
        if (isServer) {
            HANDLE sharedMem = createSharedMem();
            if (sharedMem == NULL) {
				isServer = false;
                return;
            }
            void* view = getView(sharedMem);
            uint8* data = (uint8*)view;
            fifoWriter = new FatFifoWriter(data, &data[256]);
            fifoReader = new FatFifoReader(&data[512], &data[768]);
        }	
	}
    
    uint8 C0Net::mmio_read(unsigned addr) {
        switch(addr) {
            case 0x21C0: {
                return fifoReader != NULL ? fifoReader->read() : 0;
            }
            case 0x21E0: {
                uint8 value = 0;
                if (fifoReader == NULL || fifoReader->isEmpty()) {
                    value += 1;
                } 
                if (fifoWriter == NULL || fifoWriter->isFull()) {
                    value += 2;
                }
                if (isServer) {
                    value += 4;
                }
                value += 24;
                return  value;
            }
        }
    }

    void C0Net::mmio_write(unsigned addr, uint8 data) {
        if (fifoWriter != NULL) {
            fifoWriter->write(data);
        }
    }
}
