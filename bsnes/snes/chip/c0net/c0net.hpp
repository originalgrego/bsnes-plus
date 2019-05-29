#ifndef C0NET_H
#define C0NET_H

    class C0Net : public MMIO {
        public:
            void init();
            void enable();
            void power();
            void reset();
            void toggleClient();
            void toggleServer();
			void tryServer();

            uint8 mmio_read(unsigned addr);
            void mmio_write(unsigned addr, uint8 data);

        private:
            bool isClient;
            bool isServer;
    };

    extern C0Net c0Net;
    
#endif	// C0NET_H

