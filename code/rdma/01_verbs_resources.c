/*
 * 01_verbs_resources.c
 *
 * 第一个 RDMA verbs 程序:创建 RDMA 编程的五大核心对象。
 *   Context(设备句柄) -> PD(保护域) -> MR(内存区域) / CQ(完成队列) -> QP(队列对)
 *
 * 本程序只做"资源创建 + 打印 + 清理",不做数据收发。
 * 目的是先把 libibverbs 的对象模型和 API 调用顺序摸熟。
 *
 * 编译:  gcc -Wall -Wextra -g -o 01_verbs_resources 01_verbs_resources.c -libverbs
 * 运行:  ./01_verbs_resources rxe0   (不带参数则用第一个 RDMA 设备)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <infiniband/verbs.h>

#define BUF_SIZE 4096

int main(int argc, char *argv[])
{
    struct ibv_device **dev_list = NULL;
    struct ibv_device  *ib_dev   = NULL;
    struct ibv_context *ctx      = NULL;   /* 设备句柄 */
    struct ibv_pd      *pd       = NULL;   /* 保护域 */
    struct ibv_mr      *mr       = NULL;   /* 内存区域 */
    struct ibv_cq      *cq       = NULL;   /* 完成队列 */
    struct ibv_qp      *qp       = NULL;   /* 队列对 */
    struct ibv_qp_init_attr qp_attr;
    char *buf = NULL;
    int num_devices = 0;
    int ret = 0;
    int i;

    /* 步骤 1:列出系统里所有 RDMA 设备 */
    dev_list = ibv_get_device_list(&num_devices);
    if (!dev_list || num_devices == 0) {
        fprintf(stderr, "找不到任何 RDMA 设备。soft-RoCE(rxe0)起来了吗?\n");
        return 1;
    }

    /* 选设备:命令行给了名字就按名字找,否则用列表里第一个 */
    if (argc >= 2) {
        for (i = 0; i < num_devices; i++) {
            if (strcmp(ibv_get_device_name(dev_list[i]), argv[1]) == 0) {
                ib_dev = dev_list[i];
                break;
            }
        }
        if (!ib_dev) {
            fprintf(stderr, "找不到指定设备:%s\n", argv[1]);
            ret = 1;
            goto free_list;
        }
    } else {
        ib_dev = dev_list[0];
    }
    printf("[1] 使用 RDMA 设备:%s\n", ibv_get_device_name(ib_dev));

    /* 步骤 2:打开设备,得到 context —— 之后所有操作都要用它 */
    ctx = ibv_open_device(ib_dev);
    if (!ctx) {
        fprintf(stderr, "ibv_open_device 失败\n");
        ret = 1;
        goto free_list;
    }
    printf("[2] 设备已打开,拿到 context\n");

    /* 步骤 3:分配 PD(保护域)—— 后面 MR/QP 都要挂在某个 PD 下 */
    pd = ibv_alloc_pd(ctx);
    if (!pd) {
        fprintf(stderr, "ibv_alloc_pd 失败\n");
        ret = 1;
        goto close_dev;
    }
    printf("[3] PD(保护域)已分配\n");

    /* 步骤 4:准备一块内存,注册成 MR,授权网卡直接读写它 */
    buf = malloc(BUF_SIZE);
    if (!buf) {
        fprintf(stderr, "malloc 失败\n");
        ret = 1;
        goto dealloc_pd;
    }
    memset(buf, 0, BUF_SIZE);

    mr = ibv_reg_mr(pd, buf, BUF_SIZE,
                    IBV_ACCESS_LOCAL_WRITE |
                    IBV_ACCESS_REMOTE_WRITE |
                    IBV_ACCESS_REMOTE_READ);
    if (!mr) {
        fprintf(stderr, "ibv_reg_mr 失败(内存或权限问题)\n");
        ret = 1;
        goto free_buf;
    }
    printf("[4] MR 已注册:addr=%p, length=%d, lkey=0x%x, rkey=0x%x\n",
           buf, BUF_SIZE, mr->lkey, mr->rkey);

    /* 步骤 5:创建 CQ(完成队列),容量 128 个完成事件 */
    cq = ibv_create_cq(ctx, 128, NULL, NULL, 0);
    if (!cq) {
        fprintf(stderr, "ibv_create_cq 失败\n");
        ret = 1;
        goto dereg_mr;
    }
    printf("[5] CQ(完成队列)已创建,可容纳 128 个完成事件\n");

    /* 步骤 6:创建 QP(队列对),类型 RC(可靠连接) */
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.send_cq          = cq;   /* 发送完成事件进哪个 CQ */
    qp_attr.recv_cq          = cq;   /* 接收完成事件进哪个 CQ(这里发/收共用一个) */
    qp_attr.qp_type          = IBV_QPT_RC;
    qp_attr.cap.max_send_wr  = 16;   /* 发送队列最多挂 16 个未完成请求 */
    qp_attr.cap.max_recv_wr  = 16;   /* 接收队列最多挂 16 个 */
    qp_attr.cap.max_send_sge = 1;    /* 每个请求最多 1 个分散/聚集段 */
    qp_attr.cap.max_recv_sge = 1;

    qp = ibv_create_qp(pd, &qp_attr);
    if (!qp) {
        fprintf(stderr, "ibv_create_qp 失败\n");
        ret = 1;
        goto destroy_cq;
    }
    printf("[6] QP(队列对)已创建:qp_num=0x%x,当前状态 = RESET\n", qp->qp_num);

    printf("\n成功!五大核心对象(Context/PD/MR/CQ/QP)全部就绪。\n");
    printf("下一课:把 QP 从 RESET 推到 INIT -> RTR -> RTS,然后真正收发数据。\n");

    /* 清理:严格按创建的逆序销毁 */
    ibv_destroy_qp(qp);
destroy_cq:
    ibv_destroy_cq(cq);
dereg_mr:
    ibv_dereg_mr(mr);
free_buf:
    free(buf);
dealloc_pd:
    ibv_dealloc_pd(pd);
close_dev:
    ibv_close_device(ctx);
free_list:
    ibv_free_device_list(dev_list);
    return ret;
}
