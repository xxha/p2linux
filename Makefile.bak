#============================================================================
#  Validation Suite Makefile
#   
#  Copyright (C) 2000  Monta Vista Software Inc.
# 
#  Author : Gary S. Robertson
# 
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#============================================================================
#
#============================================================================
#
# FILE NAME :  validate -
#              Wind River pSOS+ (R) on Linux pthreads validation suite test
#
# pSOS and pSOS+ are registered trademarks of Wind River Systems, Inc.
#
#============================================================================

#----------------------------------------------------------------------------
# COMPILE macros
#----------------------------------------------------------------------------

.c.o:
	$(CC) $(CFLAGS) -c $*.c

CFLAGS	= -g -Wall -O2 -I. -D_GNU_SOURCE -D_REENTRANT

#----------------------------------------------------------------------------
# Make the program...
#----------------------------------------------------------------------------
OBJS =  \
	task.o queue.o vqueue.o event.o memblk.o timer.o sema4.o validate.o

PROG = validate

all:	$(PROG)

$(PROG): $(OBJS) Makefile
	$(CC) $(CFLAGS) $(OBJS) -o $(PROG) -lpthread

#----------------------------------------------------------------------------
# Compile modules w/ Inference rules
#----------------------------------------------------------------------------
clean:
	rm -f *.o $(PROG)

depend:
	makedepend -s "# DO NOT DELETE" -- *.c

# DO NOT DELETE THIS LINE -- make depend depends on it.

demo.o: /usr/include/stdio.h /usr/include/features.h /usr/include/sys/cdefs.h
demo.o: /usr/include/gnu/stubs.h
demo.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
demo.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
demo.o: /usr/include/bits/typesizes.h /usr/include/libio.h
demo.o: /usr/include/_G_config.h /usr/include/wchar.h
demo.o: /usr/include/bits/wchar.h /usr/include/gconv.h
demo.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
demo.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
demo.o: /usr/include/stdlib.h /usr/include/unistd.h
demo.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
demo.o: not_quite_p_os.h p2pthread.h /usr/include/pthread.h
demo.o: /usr/include/sched.h /usr/include/time.h /usr/include/bits/sched.h
demo.o: /usr/include/signal.h /usr/include/bits/sigset.h
demo.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/initspin.h
demo.o: /usr/include/bits/sigthread.h
event.o: /usr/include/errno.h /usr/include/features.h
event.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
event.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
event.o: /usr/include/asm/errno.h /usr/include/unistd.h
event.o: /usr/include/bits/posix_opt.h /usr/include/bits/types.h
event.o: /usr/include/bits/wordsize.h
event.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
event.o: /usr/include/bits/typesizes.h /usr/include/bits/confname.h
event.o: /usr/include/stdlib.h /usr/include/stdio.h /usr/include/libio.h
event.o: /usr/include/_G_config.h /usr/include/wchar.h
event.o: /usr/include/bits/wchar.h /usr/include/gconv.h
event.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
event.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
event.o: /usr/include/string.h /usr/include/signal.h
event.o: /usr/include/bits/sigset.h /usr/include/sys/time.h
event.o: /usr/include/time.h /usr/include/bits/time.h
event.o: /usr/include/sys/select.h /usr/include/bits/select.h p2pthread.h
event.o: /usr/include/pthread.h /usr/include/sched.h
event.o: /usr/include/bits/sched.h /usr/include/bits/pthreadtypes.h
event.o: /usr/include/bits/initspin.h /usr/include/bits/sigthread.h
memblk.o: /usr/include/errno.h /usr/include/features.h
memblk.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
memblk.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
memblk.o: /usr/include/asm/errno.h /usr/include/unistd.h
memblk.o: /usr/include/bits/posix_opt.h /usr/include/bits/types.h
memblk.o: /usr/include/bits/wordsize.h
memblk.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
memblk.o: /usr/include/bits/typesizes.h /usr/include/bits/confname.h
memblk.o: /usr/include/stdlib.h /usr/include/stdio.h /usr/include/libio.h
memblk.o: /usr/include/_G_config.h /usr/include/wchar.h
memblk.o: /usr/include/bits/wchar.h /usr/include/gconv.h
memblk.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
memblk.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
memblk.o: /usr/include/string.h /usr/include/signal.h
memblk.o: /usr/include/bits/sigset.h /usr/include/sys/time.h
memblk.o: /usr/include/time.h /usr/include/bits/time.h
memblk.o: /usr/include/sys/select.h /usr/include/bits/select.h p2pthread.h
memblk.o: /usr/include/pthread.h /usr/include/sched.h
memblk.o: /usr/include/bits/sched.h /usr/include/bits/pthreadtypes.h
memblk.o: /usr/include/bits/initspin.h /usr/include/bits/sigthread.h
queue.o: /usr/include/errno.h /usr/include/features.h
queue.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
queue.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
queue.o: /usr/include/asm/errno.h /usr/include/unistd.h
queue.o: /usr/include/bits/posix_opt.h /usr/include/bits/types.h
queue.o: /usr/include/bits/wordsize.h
queue.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
queue.o: /usr/include/bits/typesizes.h /usr/include/bits/confname.h
queue.o: /usr/include/stdlib.h /usr/include/stdio.h /usr/include/libio.h
queue.o: /usr/include/_G_config.h /usr/include/wchar.h
queue.o: /usr/include/bits/wchar.h /usr/include/gconv.h
queue.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
queue.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
queue.o: /usr/include/string.h /usr/include/signal.h
queue.o: /usr/include/bits/sigset.h /usr/include/sys/time.h
queue.o: /usr/include/time.h /usr/include/bits/time.h
queue.o: /usr/include/sys/select.h /usr/include/bits/select.h p2pthread.h
queue.o: /usr/include/pthread.h /usr/include/sched.h
queue.o: /usr/include/bits/sched.h /usr/include/bits/pthreadtypes.h
queue.o: /usr/include/bits/initspin.h /usr/include/bits/sigthread.h
sema4.o: /usr/include/errno.h /usr/include/features.h
sema4.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
sema4.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
sema4.o: /usr/include/asm/errno.h /usr/include/unistd.h
sema4.o: /usr/include/bits/posix_opt.h /usr/include/bits/types.h
sema4.o: /usr/include/bits/wordsize.h
sema4.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
sema4.o: /usr/include/bits/typesizes.h /usr/include/bits/confname.h
sema4.o: /usr/include/stdlib.h /usr/include/stdio.h /usr/include/libio.h
sema4.o: /usr/include/_G_config.h /usr/include/wchar.h
sema4.o: /usr/include/bits/wchar.h /usr/include/gconv.h
sema4.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
sema4.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
sema4.o: /usr/include/string.h /usr/include/signal.h
sema4.o: /usr/include/bits/sigset.h /usr/include/semaphore.h
sema4.o: /usr/include/sys/types.h /usr/include/time.h
sema4.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
sema4.o: /usr/include/sys/time.h /usr/include/bits/time.h
sema4.o: /usr/include/sys/select.h /usr/include/bits/select.h p2pthread.h
sema4.o: /usr/include/pthread.h /usr/include/sched.h
sema4.o: /usr/include/bits/initspin.h /usr/include/bits/sigthread.h
task.o: /usr/include/errno.h /usr/include/features.h /usr/include/sys/cdefs.h
task.o: /usr/include/gnu/stubs.h /usr/include/bits/errno.h
task.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
task.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
task.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
task.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
task.o: /usr/include/bits/typesizes.h /usr/include/bits/confname.h
task.o: /usr/include/sched.h /usr/include/time.h /usr/include/bits/sched.h
task.o: /usr/include/sys/mman.h /usr/include/bits/mman.h
task.o: /usr/include/stdlib.h /usr/include/stdio.h /usr/include/libio.h
task.o: /usr/include/_G_config.h /usr/include/wchar.h
task.o: /usr/include/bits/wchar.h /usr/include/gconv.h
task.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
task.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
task.o: /usr/include/signal.h /usr/include/bits/sigset.h
task.o: /usr/include/sys/time.h /usr/include/bits/time.h
task.o: /usr/include/sys/select.h /usr/include/bits/select.h
task.o: /usr/include/string.h p2pthread.h /usr/include/pthread.h
task.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/initspin.h
task.o: /usr/include/bits/sigthread.h
test.o: /usr/include/stdio.h /usr/include/features.h /usr/include/sys/cdefs.h
test.o: /usr/include/gnu/stubs.h
test.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
test.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
test.o: /usr/include/bits/typesizes.h /usr/include/libio.h
test.o: /usr/include/_G_config.h /usr/include/wchar.h
test.o: /usr/include/bits/wchar.h /usr/include/gconv.h
test.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
test.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
test.o: p2linux.h
timer.o: /usr/include/errno.h /usr/include/features.h
timer.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
timer.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
timer.o: /usr/include/asm/errno.h /usr/include/sched.h
timer.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
timer.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
timer.o: /usr/include/bits/typesizes.h /usr/include/time.h
timer.o: /usr/include/bits/sched.h /usr/include/unistd.h
timer.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
timer.o: /usr/include/stdlib.h /usr/include/stdio.h /usr/include/libio.h
timer.o: /usr/include/_G_config.h /usr/include/wchar.h
timer.o: /usr/include/bits/wchar.h /usr/include/gconv.h
timer.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
timer.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
timer.o: /usr/include/signal.h /usr/include/bits/sigset.h
timer.o: /usr/include/sys/time.h /usr/include/bits/time.h
timer.o: /usr/include/sys/select.h /usr/include/bits/select.h p2pthread.h
timer.o: /usr/include/pthread.h /usr/include/bits/pthreadtypes.h
timer.o: /usr/include/bits/initspin.h /usr/include/bits/sigthread.h
validate.o: /usr/include/stdio.h /usr/include/features.h
validate.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
validate.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
validate.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
validate.o: /usr/include/bits/typesizes.h /usr/include/libio.h
validate.o: /usr/include/_G_config.h /usr/include/wchar.h
validate.o: /usr/include/bits/wchar.h /usr/include/gconv.h
validate.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
validate.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
validate.o: /usr/include/stdlib.h /usr/include/string.h /usr/include/unistd.h
validate.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
validate.o: not_quite_p_os.h p2pthread.h /usr/include/pthread.h
validate.o: /usr/include/sched.h /usr/include/time.h
validate.o: /usr/include/bits/sched.h /usr/include/signal.h
validate.o: /usr/include/bits/sigset.h /usr/include/bits/pthreadtypes.h
validate.o: /usr/include/bits/initspin.h /usr/include/bits/sigthread.h
vqueue.o: /usr/include/errno.h /usr/include/features.h
vqueue.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
vqueue.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
vqueue.o: /usr/include/asm/errno.h /usr/include/unistd.h
vqueue.o: /usr/include/bits/posix_opt.h /usr/include/bits/types.h
vqueue.o: /usr/include/bits/wordsize.h
vqueue.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
vqueue.o: /usr/include/bits/typesizes.h /usr/include/bits/confname.h
vqueue.o: /usr/include/stdlib.h /usr/include/stdio.h /usr/include/libio.h
vqueue.o: /usr/include/_G_config.h /usr/include/wchar.h
vqueue.o: /usr/include/bits/wchar.h /usr/include/gconv.h
vqueue.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
vqueue.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
vqueue.o: /usr/include/string.h /usr/include/signal.h
vqueue.o: /usr/include/bits/sigset.h /usr/include/sys/time.h
vqueue.o: /usr/include/time.h /usr/include/bits/time.h
vqueue.o: /usr/include/sys/select.h /usr/include/bits/select.h p2pthread.h
vqueue.o: /usr/include/pthread.h /usr/include/sched.h
vqueue.o: /usr/include/bits/sched.h /usr/include/bits/pthreadtypes.h
vqueue.o: /usr/include/bits/initspin.h /usr/include/bits/sigthread.h
