#ifndef DOSSHIM_IO_H
#define DOSSHIM_IO_H
/* The simulation is always a console. */
#define isatty(fd) 1
#endif
