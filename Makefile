K_SRC_DIR = .
K_SRCS = kernelstart.c memory.c process.c Queue.c syscalls.c trap.c idle.c kernelbrk.c template.c re0sp.c re1sp.c
K_INCS = kernel.h memory.h process.h Queue.h syscalls.h trap.h idle.h

KERNEL_SRCS = $(K_SRCS:%=$(K_SRC_DIR)/%)
KERNEL_OBJS = $(KERNEL_SRCS:%.c=%.o)
KERNEL_INCS = $(K_INCS:%=$(K_SRC_DIR)/%)
U_SRCS = init.c
USER_SRCS = $(U_SRCS:%=$(K_SRC_DIR)/%)
USER_OBJS = $(USER_SRCS:%.c=%.o)
USER_APPS = $(U_SRCS:%.c=%)

YALNIX_OUTPUT = yalnix

CC = gcc
DDIR58 = $(YALNIX_FRAMEWORK)
LIBDIR = $(DDIR58)/lib
INCDIR = $(DDIR58)/include
ETCDIR = $(DDIR58)/etc

KERNEL_LIBS = $(LIBDIR)/libkernel.a $(LIBDIR)/libhardware.so
KERNEL_LDFLAGS = -L$(LIBDIR) -L/usr/lib/i386-linux-gnu -lkernel -lelf -Wl,-T,$(ETCDIR)/kernel.x -Wl,-R$(LIBDIR) -lhardware
USER_LDFLAGS = -static -Wl,-T,$(ETCDIR)/user.x -u exit -u __brk -u __sbrk -u __mmap -u __default_morecore -L$(LIBDIR) -lyuser
USER_LIBS = $(LIBDIR)/libyuser.a
CPPFLAGS = -D_FILE_OFFSET_BITS=64 -m32 -fno-builtin -I. -I$(INCDIR) -g -DLINUX -fno-stack-protector

all: $(YALNIX_OUTPUT) $(USER_APPS)

$(YALNIX_OUTPUT): $(KERNEL_OBJS) $(KERNEL_LIBS) $(KERNEL_INCS)
	$(CC) -m32 -o $@ $(KERNEL_OBJS) $(KERNEL_LDFLAGS)

$(USER_APPS): %: %.o $(USER_LIBS)
	$(CC) -m32 -o $@ $< $(USER_LDFLAGS)

clean:
	rm -f *.o *~ TTYLOG* TRACE $(YALNIX_OUTPUT) $(USER_APPS) core.* ~/core

count:
	wc $(KERNEL_SRCS) $(USER_SRCS)

list:
	ls -l *.c *.h

kill:
	killall yalnixtty yalnixnet yalnix

.PHONY: all clean count list kill
