PROG ?= web_service
CFLAGS ?= -W -Wall -DMG_ENABLE_LINES=1 -L../Librador_API/___librador/librador_shared_library -lrador -I../Librador_API/___librador/librador_shared_library -I/usr/include/c++/7 -I/usr/include/x86_64-linux-gnu/c++/7
SSL = ?

ifeq "$(SSL)" "MBEDTLS"
CFLAGS += -DMG_ENABLE_MBEDTLS=1 -lmbedtls -lmbedcrypto -lmbedx509
endif

ifeq "$(SSL)" "OPENSSL"
CFLAGS += -DMG_ENABLE_OPENSSL=1 -lssl -lcrypto
endif

all: $(PROG)
	$(DEBUGGER) ./$(PROG) $(ARGS)

$(PROG): main.cpp
	$(CC) /mnt/c/dev/mongoose/mongoose.c main.cpp -I/mnt/c/dev/mongoose $(CFLAGS) $(EFLAGS) -o $(PROG)

clean:
	rm -rf $(PROG) *.o *.dSYM *.gcov *.gcno *.gcda *.obj *.exe *.ilk *.pdb
