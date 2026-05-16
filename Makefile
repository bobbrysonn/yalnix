K_SRC_DIR = .
K_SRCS = kernelstart.c memory.c process.c Queue.c syscalls.c trap.c idle.c kernelbrk.c template.c init.c re0sp.c re1sp.c
K_INCS = kernel.h memory.h process.h Queue.h syscalls.h trap.h idle.h init.h

KERNEL_SRCS = $(K_SRCS:%=$(K_SRC_DIR)/%)
KERNEL_OBJS = $(KERNEL_SRCS:%.c=%.o)
KERNEL_INCS = $(K_INCS:%=$(K_SRC_DIR)/%)

YALNIX_OUTPUT = yalnix

CC = gcc
DDIR58 = $(YALNIX_FRAMEWORK)
LIBDIR = $(DDIR58)/lib
INCDIR = $(DDIR58)/include
ETCDIR = $(DDIR58)/etc

KERNEL_LIBS = $(LIBDIR)/libkernel.a $(LIBDIR)/libhardware.so
KERNEL_LDFLAGS = -L$(LIBDIR) -L/usr/lib/i386-linux-gnu -lkernel -lelf -Wl,-T,$(ETCDIR)/kernel.x -Wl,-R$(LIBDIR) -lhardware
CPPFLAGS = -D_FILE_OFFSET_BITS=64 -m32 -fno-builtin -I. -I$(INCDIR) -g -DLINUX -fno-stack-protector

all: $(YALNIX_OUTPUT)

$(YALNIX_OUTPUT): $(KERNEL_OBJS) $(KERNEL_LIBS) $(KERNEL_INCS)
	$(CC) -m32 -o $@ $(KERNEL_OBJS) $(KERNEL_LDFLAGS)

clean:
	rm -f *.o *~ TTYLOG* TRACE $(YALNIX_OUTPUT) core.* ~/core

count:
	wc $(KERNEL_SRCS)

list:
	ls -l *.c *.h

kill:
	killall yalnixtty yalnixnet yalnix

.PHONY: all clean count list kill
