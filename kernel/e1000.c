#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int
e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //
  // 传输代码必须在TX（传输）环的描述符中放置指向数据包数据的指针。
  // struct tx_desc描述了描述符的格式。您需要确保每个mbuf最终被释放，
  // 但只能在E1000完成数据包传输之后（E1000在描述符中设置E1000_TXD_STAT_DD位以指示此情况）。
  acquire(&e1000_lock);

  // 首先，通过读取E1000_TDT控制寄存器，向E1000询问等待下一个数据包的TX环索引。
  uint32 TX_index= regs[E1000_TDT];// TX环索引
  struct tx_desc* desc = &tx_ring[TX_index];// E1000_TDT索引的描述符

  // 然后检查环是否溢出
  if((desc->status&E1000_TXD_STAT_DD)==0){
    // 如果E1000_TXD_STAT_DD未在E1000_TDT索引的描述符中设置，则E1000尚未完成先前相应的传输请求，因此返回错误。
    release(&e1000_lock);
    return -1;
  }

  // 否则，使用mbuffree()释放从该描述符传输的最后一个mbuf（如果有）
  if(tx_mbufs[TX_index]){
    mbuffree(tx_mbufs[TX_index]);
  }

  // 然后填写描述符。
  desc->length=m->len;
  desc->addr=(uint64)m->head;
  // 设置必要的cmd标志（请参阅E1000手册的第3.3节），并保存指向mbuf的指针，以便稍后释放。
  /*
  数据包结束标志（End Of Packet）
  当设置为1时，表示该描述符是构成数据包的最后一个描述符。一个或多个描述符可以用来组成一个数据包。

  报告状态（Report Status）
  当设置为1时，以太网控制器需要报告状态信息。
  软件可以利用这个功能，在内存中检查传输描述符以确定哪些描述符已经完成，并且数据包已经被缓冲在传输FIFO中。
  软件通过查看描述符状态字节并检查“Descriptor Done”（DD）位来实现这一点。
  */
  desc->cmd=E1000_TXD_CMD_RS|E1000_TXD_CMD_EOP;
  tx_mbufs[TX_index] = 0;// 保存指向mbuf的指针
  // 最后，通过将一加到E1000_TDT再对TX_RING_SIZE取模来更新环位置。
  regs[E1000_TDT]=(TX_index+1)%TX_RING_SIZE;
  release(&e1000_lock);
  return 0;
}

static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //
  // 当E1000从以太网接收到每个包时，它首先将包DMA到下一个RX(接收)环描述符指向的mbuf，然后产生一个中断。
  // e1000_recv()代码必须扫描RX环，并通过调用net_rx()将每个新数据包的mbuf发送到网络栈（在net.c中）。
  // 然后，您需要分配一个新的mbuf并将其放入描述符中，以便当E1000再次到达RX环中的该点时，
  // 它会找到一个新的缓冲区，以便DMA新数据包。
  while(1){
  // 首先通过提取E1000_RDT控制寄存器并加一对RX_RING_SIZE取模，向E1000询问下一个等待接收数据包（如果有）所在的环索引。
  uint32 RX_index=(regs[E1000_RDT]+1)%RX_RING_SIZE;// 环索引
  // 然后通过检查描述符status部分中的E1000_RXD_STAT_DD位来检查新数据包是否可用。如果不可用，请停止。
  struct rx_desc* desc = &rx_ring[RX_index];// E1000_TDT索引的描述符
  if(!(desc->status&E1000_RXD_STAT_DD)){
    return;
  }
  // 否则，将mbuf的m->len更新为描述符中报告的长度。使用net_rx()将mbuf传送到网络栈。
  rx_mbufs[RX_index]->len=desc->length;
  net_rx(rx_mbufs[RX_index]);

  // 然后使用mbufalloc()分配一个新的mbuf，以替换刚刚给net_rx()的mbuf。将其数据指针（m->head）编程到描述符中。将描述符的状态位清除为零。
  rx_mbufs[RX_index]=mbufalloc(0);
  if (!rx_mbufs[RX_index])
      panic("e1000");
  desc->addr=(uint64)rx_mbufs[RX_index]->head;
  desc->status=0;

  // 最后，将E1000_RDT寄存器更新为最后处理的环描述符的索引
  regs[E1000_RDT]=RX_index;
  }
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
