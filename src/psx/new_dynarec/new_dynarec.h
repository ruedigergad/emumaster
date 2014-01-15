#ifndef NEWDYNAREC_H
#define NEWDYNAREC_H

#if defined(__cplusplus)
extern "C" {
#endif

extern int pcaddr;
extern int pending_exception;

void new_dynarec_init();
void new_dynarec_cleanup();
void new_dynarec_clear_full();
void new_dyna_start();

void invalidate_all_pages();
void invalidate_block(unsigned int block);

#if defined(__cplusplus)
}
#endif

#endif // NEWDYNAREC_H
