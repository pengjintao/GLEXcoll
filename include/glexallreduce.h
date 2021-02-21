#ifndef GLEXALLREDUCE_H
#define GLEXALLREDUCE_H

#ifdef __cplusplus
#define BEGIN_C_DECLS \
    extern "C"        \
    {
#define END_C_DECLS }
#else /* !__cplusplus */
#define BEGIN_C_DECLS
#define END_C_DECLS
#endif /* __cplusplus */
BEGIN_C_DECLS

enum SoftWare_Allreduce_Algorithm_Power_of_2_type
{
    Recursize_doubling_Slicing,
    Recursize_doubling_OMP,
    K_nomial_tree_OMP
};

extern void GLEXCOLL_Allreduce_NonOffload(void *sendbuf, int count, void *recvbuf);
void GLEXCOLL_InitAllreduce();
extern char * allreduce_sendbuf,*allreduce_recvbuf;
extern int allreduce_recursive_doubling_stepn;
extern int allreduce_send_recv_pair;
END_C_DECLS

#endif