/* orinoco.c 0.11b	- (formerly known as dldwd_cs.c and orinoco_cs.c)
 *
 * A driver for Hermes or Prism 2 chipset based PCMCIA wireless
 * adaptors, with Lucent/Agere, Intersil or Symbol firmware.
 *
 * Copyright (C) 2000 David Gibson, Linuxcare Australia <hermes@gibson.dropbear.id.au>
 *	With some help from :
 * Copyright (C) 2001 Jean Tourrilhes, HP Labs <jt@hpl.hp.com>
 * Copyright (C) 2001 Benjamin Herrenschmidt <benh@kernel.crashing.org>
 *
 * Based on dummy_cs.c 1.27 2000/06/12 21:27:25
 *
 * Portions based on wvlan_cs.c 1.0.6, Copyright Andreas Neuhaus <andy@fasta.fh-dortmund.de>
 *      http://www.fasta.fh-dortmund.de/users/andy/wvlan/
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License
 * at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and
 * limitations under the License.
 *
 * The initial developer of the original code is David A. Hinds
 * <dahinds@users.sourceforge.net>.  Portions created by David
 * A. Hinds are Copyright (C) 1999 David A. Hinds.  All Rights
 * Reserved.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the MPL or the GPL.  */

/*
 * v0.01 -> v0.02 - 21/3/2001 - Jean II
 *	o Allow to use regular ethX device name instead of dldwdX
 *	o Warning on IBSS with ESSID=any for firmware 6.06
 *	o Put proper range.throughput values (optimistic)
 *	o IWSPY support (IOCTL and stat gather in Rx path)
 *	o Allow setting frequency in Ad-Hoc mode
 *	o Disable WEP setting if !has_wep to work on old firmware
 *	o Fix txpower range
 *	o Start adding support for Samsung/Compaq firmware
 *
 * v0.02 -> v0.03 - 23/3/2001 - Jean II
 *	o Start adding Symbol support - need to check all that
 *	o Fix Prism2/Symbol WEP to accept 128 bits keys
 *	o Add Symbol WEP (add authentication type)
 *	o Add Prism2/Symbol rate
 *	o Add PM timeout (holdover duration)
 *	o Enable "iwconfig eth0 key off" and friends (toggle flags)
 *	o Enable "iwconfig eth0 power unicast/all" (toggle flags)
 *	o Try with an intel card. It report firmware 1.01, behave like
 *	  an antiquated firmware, however on windows it says 2.00. Yuck !
 *	o Workaround firmware bug in allocate buffer (Intel 1.01)
 *	o Finish external renaming to orinoco...
 *	o Testing with various Wavelan firmwares
 *
 * v0.03 -> v0.04 - 30/3/2001 - Jean II
 *	o Update to Wireless 11 -> add retry limit/lifetime support
 *	o Tested with a D-Link DWL 650 card, fill in firmware support
 *	o Warning on Vcc mismatch (D-Link 3.3v card in Lucent 5v only slot)
 *	o Fixed the Prims2 WEP bugs that I introduced in v0.03 :-(
 *	  It work on D-Link *only* after a tcpdump. Weird...
 *	  And still doesn't work on Intel card. Grrrr...
 *	o Update the mode after a setport3
 *	o Add preamble setting for Symbol cards (not yet enabled)
 *	o Don't complain as much about Symbol cards...
 *
 * v0.04 -> v0.04b - 22/4/2001 - David Gibson
 *      o Removed the 'eth' parameter - always use ethXX as the
 *        interface name instead of dldwdXX.  The other was racy
 *        anyway.
 *	o Clean up RID definitions in hermes.h, other cleanups
 *
 * v0.04b -> v0.04c - 24/4/2001 - Jean II
 *	o Tim Hurley <timster@seiki.bliztech.com> reported a D-Link card
 *	  with vendor 02 and firmware 0.08. Added in the capabilities...
 *	o Tested Lucent firmware 7.28, everything works...
 *
 * v0.04c -> v0.05 - 3/5/2001 - Benjamin Herrenschmidt
 *	o Spin-off Pcmcia code. This file is renamed orinoco.c,
 *	  and orinoco_cs.c now contains only the Pcmcia specific stuff
 *	o Add Airport driver support on top of orinoco.c (see airport.c)
 *
 * v0.05 -> v0.05a - 4/5/2001 - Jean II
 *	o Revert to old Pcmcia code to fix breakage of Ben's changes...
 *
 * v0.05a -> v0.05b - 4/5/2001 - Jean II
 *	o add module parameter 'ignore_cis_vcc' for D-Link @ 5V
 *	o D-Link firmware doesn't support multicast. We just print a few
 *	  error messages, but otherwise everything works...
 *	o For David : set/getport3 works fine, just upgrade iwpriv...
 *
 * v0.05b -> v0.05c - 5/5/2001 - Benjamin Herrenschmidt
 *	o Adapt airport.c to latest changes in orinoco.c
 *	o Remove deferred power enabling code
 *
 * v0.05c -> v0.05d - 5/5/2001 - Jean II
 *	o Workaround to SNAP decapsulate frame from LinkSys AP
 *	  original patch from : Dong Liu <dliu@research.bell-labs.com>
 *	  (note : the memcmp bug was mine - fixed)
 *	o Remove set_retry stuff, no firmware support it (bloat--).
 *
 * v0.05d -> v0.06 - 25/5/2001 - Jean II
 *		Original patch from "Hong Lin" <alin@redhat.com>,
 *		"Ian Kinner" <ikinner@redhat.com>
 *		and "David Smith" <dsmith@redhat.com>
 *	o Init of priv->tx_rate_ctrl in firmware specific section.
 *	o Prism2/Symbol rate, upto should be 0xF and not 0x15. Doh !
 *	o Spectrum card always need cor_reset (for every reset)
 *	o Fix cor_reset to not loose bit 7 in the register
 *	o flush_stale_links to remove zombie Pcmcia instances
 *	o Ack previous hermes event before reset
 *		Me (with my little hands)
 *	o Allow orinoco.c to call cor_reset via priv->card_reset_handler
 *	o Add priv->need_card_reset to toggle this feature
 *	o Fix various buglets when setting WEP in Symbol firmware
 *	  Now, encryption is fully functional on Symbol cards. Youpi !
 *
 * v0.06 -> v0.06b - 25/5/2001 - Jean II
 *	o IBSS on Symbol use port_mode = 4. Please don't ask...
 *
 * v0.06b -> v0.06c - 29/5/2001 - Jean II
 *	o Show first spy address in /proc/net/wireless for IBSS mode as well
 *
 * v0.06c -> v0.06d - 6/7/2001 - David Gibson
 *      o Change a bunch of KERN_INFO messages to KERN_DEBUG, as per Linus'
 *        wishes to reduce the number of unecessary messages.
 *	o Removed bogus message on CRC error.
 *	o Merged fixeds for v0.08 Prism 2 firmware from William Waghorn
 *	  <willwaghorn@yahoo.co.uk>
 *	o Slight cleanup/re-arrangement of firmware detection code.
 *
 * v0.06d -> v0.06e - 1/8/2001 - David Gibson
 *	o Removed some redundant global initializers (orinoco_cs.c).
 *	o Added some module metadataa
 *
 * v0.06e -> v0.06f - 14/8/2001 - David Gibson
 *	o Wording fix to license
 *	o Added a 'use_alternate_encaps' module parameter for APs which need an
 *	  oui of 00:00:00.  We really need a better way of handling this, but
 *	  the module flag is better than nothing for now.
 *
 * v0.06f -> v0.07 - 20/8/2001 - David Gibson
 *	o Removed BAP error retries from hermes_bap_seek().  For Tx we now
 *	  let the upper layers handle the retry, we retry explicitly in the
 *	  Rx path, but don't make as much noise about it.
 *	o Firmware detection cleanups.
 *
 * v0.07 -> v0.07a - 1/10/3001 - Jean II
 *	o Add code to read Symbol firmware revision, inspired by latest code
 *	  in Spectrum24 by Lee John Keyser-Allen - Thanks Lee !
 *	o Thanks to Jared Valentine <hidden@xmission.com> for "providing" me
 *	  a 3Com card with a recent firmware, fill out Symbol firmware
 *	  capabilities of latest rev (2.20), as well as older Symbol cards.
 *	o Disable Power Management in newer Symbol firmware, the API 
 *	  has changed (documentation needed).
 *
 * v0.07a -> v0.08 - 3/10/2001 - David Gibson
 *	o Fixed a possible buffer overrun found by the Stanford checker (in
 *	  dldwd_ioctl_setiwencode()).  Can only be called by root anyway, so not
 *	  a big problem.
 *	o Turned has_big_wep on for Intersil cards.  That's not true for all of
 *	  them but we should at least let the capable ones try.
 *	o Wait for BUSY to clear at the beginning of hermes_bap_seek().  I
 *	  realised that my assumption that the driver's serialization
 *	  would prevent the BAP being busy on entry was possibly false, because
 *	  things other than seeks may make the BAP busy.
 *	o Use "alternate" (oui 00:00:00) encapsulation by default.
 *	  Setting use_old_encaps will mimic the old behaviour, but I think we
 *	  will be able to eliminate this.
 *	o Don't try to make __initdata const (the version string).  This can't
 *	  work because of the way the __initdata sectioning works.
 *	o Added MODULE_LICENSE tags.
 *	o Support for PLX (transparent PCMCIA->PCI brdge) cards.
 *	o Changed to using the new type-facist min/max.
 *
 * v0.08 -> v0.08a - 9/10/2001 - David Gibson
 *	o Inserted some missing acknowledgements/info into the Changelog.
 *	o Fixed some bugs in the normalisation of signel level reporting.
 *	o Fixed bad bug in WEP key handling on Intersil and Symbol firmware,
 *	  which led to an instant crash on big-endian machines.
 *
 * v0.08a -> v0.08b - 20/11/2001 - David Gibson
 *	o Lots of cleanup and bugfixes in orinoco_plx.c
 *	o Cleanup to handling of Tx rate setting.
 *	o Removed support for old encapsulation method.
 *	o Removed old "dldwd" names.
 *	o Split RID constants into a new file hermes_rid.h
 *	o Renamed RID constants to match linux-wlan-ng and prism2.o
 *	o Bugfixes in hermes.c
 *	o Poke the PLX's INTCSR register, so it actually starts
 *	  generating interrupts.  These cards might actually work now.
 *	o Update to wireless extensions v12 (Jean II)
 *	o Support for tallies and inquire command (Jean II)
 *	o Airport updates for newer PPC kernels (BenH)
 *
 * v0.08b -> v0.09 - 21/12/2001 - David Gibson
 *	o Some new PCI IDs for PLX cards.
 *	o Removed broken attempt to do ALLMULTI reception.  Just use
 *	  promiscuous mode instead
 *	o Preliminary work for list-AP (Jean II)
 *	o Airport updates from (BenH)
 *	o Eliminated racy hw_ready stuff
 *	o Fixed generation of fake events in irq handler.  This should
 *	  finally kill the EIO problems (Jean II & dgibson)
 *	o Fixed breakage of bitrate set/get on Agere firmware (Jean II)
 *
 * v0.09 -> v0.09a - 2/1/2002 - David Gibson
 *	o Fixed stupid mistake in multicast list handling, triggering
 *	  a BUG()
 *
 * v0.09a -> v0.09b - 16/1/2002 - David Gibson
 *	o Fixed even stupider mistake in new interrupt handling, which
 *	  seriously broke things on big-endian machines.
 *	o Removed a bunch of redundant includes and exports.
 *	o Removed a redundant MOD_{INC,DEC}_USE_COUNT pair in airport.c
 *	o Don't attempt to do hardware level multicast reception on
 *	  Intersil firmware, just go promisc instead.
 *	o Typo fixed in hermes_issue_cmd()
 *	o Eliminated WIRELESS_SPY #ifdefs
 *	o Status code reported on Tx exceptions
 *	o Moved netif_wake_queue() from ALLOC interrupts to TX and TXEXC
 *	  interrupts, which should fix the timeouts we're seeing.
 *
 * v0.09b -> v0.10 - 25 Feb 2002 - David Gibson
 *	o Removed nested structures used for header parsing, so the
 *	  driver should now work without hackery on ARM
 *	o Fix for WEP handling on Intersil (Hawk Newton)
 *	o Eliminated the /proc/hermes/ethXX/regs debugging file.  It
 *	  was never very useful.
 *	o Make Rx errors less noisy.
 *
 * v0.10 -> v0.11 - 5 Apr Mar 2002 - David Gibson
 *	o Laid the groundwork in hermes.[ch] for devices which map
 *	  into PCI memory space rather than IO space.
 *	o Fixed bug in multicast handling (cleared multicast list when
 *	  leaving promiscuous mode).
 *	o Relegated Tx error messages to debug.
 *	o Cleaned up / corrected handling of allocation lengths.
 *	o Set OWNSSID in IBSS mode for WinXP interoperability (jimc).
 *	o Change to using alloc_etherdev() for structure allocations. 
 *	o Check for and drop undersized packets.
 *	o Fixed a race in stopping/waking the queue.  This should fix
 *	  the timeout problems (Pavel Roskin)
 *	o Reverted to netif_wake_queue() on the ALLOC event.
 *	o Fixes for recent Symbol firmwares which lack AP density
 *	  (Pavel Roskin).
 *
 * v0.11 -> v0.11b - 29 Apr 2002 - David Gibson
 *	o Handle different register spacing, necessary for Prism 2.5
 *	  PCI adaptors (Steve Hill).
 *	o Cleaned up initialization of card structures in orinoco_cs
 *	  and airport.  Removed card->priv field.
 *	o Make response structure optional for hermes_docmd_wait()
 *	  Pavel Roskin)
 *	o Added PCI id for Nortel emobility to orinoco_plx.c.
 *	o Cleanup to handling of Symbol's allocation bug. (Pavel Roskin)
 *	o Cleanups to firmware capability detection.
 *	o Arrange for orinoco_pci.c to override firmware detection.
 *	  We should be able to support the PCI Intersil cards now.
 *	o Cleanup handling of reset_cor and hard_reset (Pavel Roskin).
 *	o Remove erroneous use of USER_BAP in the TxExc handler (Jouni
 *	  Malinen).
 *	o Makefile changes for better integration into David Hinds
 *	  pcmcia-cs package.
 *
 * v0.11a -> v0.11b - 1 May 2002 - David Gibson
 *	o Better error reporting in orinoco_plx_init_one()
 *	o Fixed multiple bad kfree() bugs introduced by the
 *	  alloc_orinocodev() changes.
 *
 * TODO
 *	o New wireless extensions API
 *	o Handle de-encapsulation within network layer, provide 802.11
 *	  headers
 *	o Fix possible races in SPY handling.
 *	o Disconnect wireless extensions from fundamental configuration.
 *
 *	o Convert /proc debugging stuff to seqfile
 *	o Use multiple Tx buffers
 */
/* Notes on locking:
 *
 * The basic principle of operation is that everything except the
 * interrupt handler is serialized through a single spinlock in the
 * struct orinoco_private structure, using orinoco_lock() and
 * orinoco_unlock() (which in turn use spin_lock_bh() and
 * spin_unlock_bh()).
 *
 * The kernel's IRQ handling stuff ensures that the interrupt handler
 * does not re-enter itself. The interrupt handler is written such
 * that everything it does is safe without a lock: chiefly this means
 * that the Rx path uses one of the Hermes chipset's BAPs while
 * everything else uses the other.
 *
 * Actually, strictly speaking, the updating of the statistics from
 * the interrupt handler isn't safe without a lock.  However the worst
 * that can happen is that we perturb the packet/byte counts slightly.
 * We could fix this to use atomic types, but it's probably not worth
 * it.
 *
 * The big exception is that that we don't want the irq handler
 * running when we actually reset or shut down the card, because
 * strange things might happen (probably the worst would be one packet
 * of garbage, but you can't be too careful). For this we use
 * __orinoco_stop_irqs() which will set a flag to disable the interrupt
 * handler, and wait for any outstanding instances of the handler to
 * complete. THIS WILL LOSE INTERRUPTS! so it shouldn't be used except
 * for resets, where losing a few interrupts is acceptable. */

#include <linux/config.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <linux/list.h>

#include "hermes.h"
#include "hermes_rid.h"
#include "orinoco.h"
#include "ieee802_11.h"

/* Wireless extensions backwards compatibility */
#ifndef SIOCIWFIRSTPRIV
#define SIOCIWFIRSTPRIV		SIOCDEVPRIVATE
#endif /* SIOCIWFIRSTPRIV */

/* We do this this way to avoid ifdefs in the actual code */
#ifdef WIRELESS_SPY
#define SPY_NUMBER(priv)	(priv->spy_number)
#else
#define SPY_NUMBER(priv)	0
#endif /* WIRELESS_SPY */

static char version[] __initdata = "orinoco.c 0.11b (David Gibson <hermes@gibson.dropbear.id.au> and others)";
MODULE_AUTHOR("David Gibson <hermes@gibson.dropbear.id.au>");
MODULE_DESCRIPTION("Driver for Lucent Orinoco, Prism II based and similar wireless cards");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual MPL/GPL");
#endif

/* Level of debugging. Used in the macros in orinoco.h */
#ifdef ORINOCO_DEBUG
int orinoco_debug = ORINOCO_DEBUG;
MODULE_PARM(orinoco_debug, "i");
EXPORT_SYMBOL(orinoco_debug);
#endif

#define ORINOCO_MIN_MTU		256
#define ORINOCO_MAX_MTU		(IEEE802_11_DATA_LEN - ENCAPS_OVERHEAD)

#define SYMBOL_MAX_VER_LEN	(14)
#define USER_BAP		0
#define IRQ_BAP			1
#define MAX_IRQLOOPS_PER_IRQ	10
#define MAX_IRQLOOPS_PER_JIFFY	(20000/HZ)	/* Based on a guestimate of how many events the
						   device can legitimately generate */
#define SMALL_KEY_SIZE		5
#define LARGE_KEY_SIZE		13
#define TX_NICBUF_SIZE_BUG	1585		/* Bug in Symbol firmware */

#define DUMMY_FID		0xFFFF

/*#define MAX_MULTICAST(priv)	(priv->firmware_type == FIRMWARE_TYPE_AGERE ? \
  HERMES_MAX_MULTICAST : 0)*/
#define MAX_MULTICAST(priv)	(HERMES_MAX_MULTICAST)

/********************************************************************/
/* Data tables                                                      */
/********************************************************************/

/* The frequency of each channel in MHz */
const long channel_frequency[] = {
	2412, 2417, 2422, 2427, 2432, 2437, 2442,
	2447, 2452, 2457, 2462, 2467, 2472, 2484
};
#define NUM_CHANNELS ( sizeof(channel_frequency) / sizeof(channel_frequency[0]) )

/* This tables gives the actual meanings of the bitrate IDs returned by the firmware. */
struct {
	int bitrate; /* in 100s of kilbits */
	int automatic;
	u16 agere_txratectrl;
	u16 intersil_txratectrl;
} bitrate_table[] = {
	{110, 1,  3, 15}, /* Entry 0 is the default */
	{10,  0,  1,  1},
	{10,  1,  1,  1},
	{20,  0,  2,  2},
	{20,  1,  6,  3},
	{55, 0,  4,  4},
	{55, 1,  7,  7},
	{110, 0,  5,  8},
};
#define BITRATE_TABLE_SIZE (sizeof(bitrate_table) / sizeof(bitrate_table[0]))

struct header_struct {
	/* 802.3 */
	u8 dest[ETH_ALEN];
	u8 src[ETH_ALEN];
	u16 len;
	/* 802.2 */
	u8 dsap;
	u8 ssap;
	u8 ctrl;
	/* SNAP */
	u8 oui[3];
	u16 ethertype;
} __attribute__ ((packed));

/* 802.2 LLC/SNAP header used for Ethernet encapsulation over 802.11 */
u8 encaps_hdr[] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};

#define ENCAPS_OVERHEAD		(sizeof(encaps_hdr) + 2)

/*
 * Function prototypes
 */

static void orinoco_stat_gather(struct net_device *dev,
			      struct sk_buff *skb,
			      struct hermes_rx_descriptor *desc);

static struct net_device_stats *orinoco_get_stats(struct net_device *dev);
static struct iw_statistics *orinoco_get_wireless_stats(struct net_device *dev);

/* Hardware control routines */

static int __orinoco_hw_set_bitrate(struct orinoco_private *priv);
static int __orinoco_hw_setup_wep(struct orinoco_private *priv);
static int orinoco_hw_get_bssid(struct orinoco_private *priv, char buf[ETH_ALEN]);
static int orinoco_hw_get_essid(struct orinoco_private *priv, int *active,
			      char buf[IW_ESSID_MAX_SIZE+1]);
static long orinoco_hw_get_freq(struct orinoco_private *priv);
static int orinoco_hw_get_bitratelist(struct orinoco_private *priv, int *numrates,
				    s32 *rates, int max);
static void __orinoco_set_multicast_list(struct net_device *dev);

/* Interrupt handling routines */
static void __orinoco_ev_tick(struct orinoco_private *priv, hermes_t *hw);
static void __orinoco_ev_wterr(struct orinoco_private *priv, hermes_t *hw);
static void __orinoco_ev_infdrop(struct orinoco_private *priv, hermes_t *hw);
static void __orinoco_ev_info(struct orinoco_private *priv, hermes_t *hw);
static void __orinoco_ev_rx(struct orinoco_private *priv, hermes_t *hw);
static void __orinoco_ev_txexc(struct orinoco_private *priv, hermes_t *hw);
static void __orinoco_ev_tx(struct orinoco_private *priv, hermes_t *hw);
static void __orinoco_ev_alloc(struct orinoco_private *priv, hermes_t *hw);

static int orinoco_ioctl_getiwrange(struct net_device *dev, struct iw_point *rrq);
static int orinoco_ioctl_setiwencode(struct net_device *dev, struct iw_point *erq);
static int orinoco_ioctl_getiwencode(struct net_device *dev, struct iw_point *erq);
static int orinoco_ioctl_setessid(struct net_device *dev, struct iw_point *erq);
static int orinoco_ioctl_getessid(struct net_device *dev, struct iw_point *erq);
static int orinoco_ioctl_setnick(struct net_device *dev, struct iw_point *nrq);
static int orinoco_ioctl_getnick(struct net_device *dev, struct iw_point *nrq);
static int orinoco_ioctl_setfreq(struct net_device *dev, struct iw_freq *frq);
static int orinoco_ioctl_getsens(struct net_device *dev, struct iw_param *srq);
static int orinoco_ioctl_setsens(struct net_device *dev, struct iw_param *srq);
static int orinoco_ioctl_setrts(struct net_device *dev, struct iw_param *rrq);
static int orinoco_ioctl_setfrag(struct net_device *dev, struct iw_param *frq);
static int orinoco_ioctl_getfrag(struct net_device *dev, struct iw_param *frq);
static int orinoco_ioctl_setrate(struct net_device *dev, struct iw_param *frq);
static int orinoco_ioctl_getrate(struct net_device *dev, struct iw_param *frq);
static int orinoco_ioctl_setpower(struct net_device *dev, struct iw_param *prq);
static int orinoco_ioctl_getpower(struct net_device *dev, struct iw_param *prq);
static int orinoco_ioctl_setport3(struct net_device *dev, struct iwreq *wrq);
static int orinoco_ioctl_getport3(struct net_device *dev, struct iwreq *wrq);

/* /proc debugging stuff */
static int orinoco_proc_init(void);
static void orinoco_proc_cleanup(void);

/*
 * Inline functions
 */
static inline void
orinoco_lock(struct orinoco_private *priv)
{
	spin_lock_bh(&priv->lock);
}

static inline void
orinoco_unlock(struct orinoco_private *priv)
{
	spin_unlock_bh(&priv->lock);
}

static inline int
orinoco_irqs_allowed(struct orinoco_private *priv)
{
	return test_bit(ORINOCO_STATE_DOIRQ, &priv->state);
}

static inline void
__orinoco_stop_irqs(struct orinoco_private *priv)
{
	hermes_t *hw = &priv->hw;

	hermes_set_irqmask(hw, 0);
	clear_bit(ORINOCO_STATE_DOIRQ, &priv->state);
	while (test_bit(ORINOCO_STATE_INIRQ, &priv->state))
		;
}

static inline void
__orinoco_start_irqs(struct orinoco_private *priv, u16 irqmask)
{
	hermes_t *hw = &priv->hw;

	TRACE_ENTER(priv->ndev->name);

	__cli(); /* FIXME: is this necessary? */
	set_bit(ORINOCO_STATE_DOIRQ, &priv->state);
	hermes_set_irqmask(hw, irqmask);
	__sti();

	TRACE_EXIT(priv->ndev->name);
}

static inline void
set_port_type(struct orinoco_private *priv)
{
	switch (priv->iw_mode) {
	case IW_MODE_INFRA:
		priv->port_type = 1;
		priv->allow_ibss = 0;
		break;
	case IW_MODE_ADHOC:
		if (priv->prefer_port3) {
			priv->port_type = 3;
			priv->allow_ibss = 0;
		} else {
			priv->port_type = priv->ibss_port;
			priv->allow_ibss = 1;
		}
		break;
	default:
		printk(KERN_ERR "%s: Invalid priv->iw_mode in set_port_type()\n",
		       priv->ndev->name);
	}
}

static inline int
is_snap(struct header_struct *hdr)
{
	return (hdr->dsap == 0xAA) && (hdr->ssap == 0xAA) && (hdr->ctrl == 0x3);
}

static void
orinoco_set_multicast_list(struct net_device *dev)
{
	struct orinoco_private *priv = dev->priv;

	orinoco_lock(priv);
	__orinoco_set_multicast_list(dev);
	orinoco_unlock(priv);
}

/*
 * Hardware control routines
 */

void
orinoco_shutdown(struct orinoco_private *priv)
{
	int err = 0;

	TRACE_ENTER(priv->ndev->name);

	orinoco_lock(priv);
	__orinoco_stop_irqs(priv);

	err = hermes_reset(&priv->hw);
	if (err && err != -ENODEV) /* If the card is gone, we don't care about shutting it down */
		printk(KERN_ERR "%s: Error %d shutting down Hermes chipset\n", priv->ndev->name, err);

	orinoco_unlock(priv);

	TRACE_EXIT(priv->ndev->name);
}

int
orinoco_reset(struct orinoco_private *priv)
{
	struct net_device *dev = priv->ndev;
	hermes_t *hw = &priv->hw;
	int err = 0;
	struct hermes_idstring idbuf;

	TRACE_ENTER(priv->ndev->name);

	/* Stop other people bothering us */
	orinoco_lock(priv);
	__orinoco_stop_irqs(priv);

	/* Check if we need a card reset */
	if (priv->hard_reset)
		priv->hard_reset(priv);

	/* Do standard firmware reset if we can */
	err = hermes_reset(hw);
	if (err)
		goto out;

	err = hermes_allocate(hw, priv->nicbuf_size, &priv->txfid);
	if (err == -EIO) {
		/* Try workaround for old Symbol firmware bug */
		priv->nicbuf_size = TX_NICBUF_SIZE_BUG;
		err = hermes_allocate(hw, priv->nicbuf_size, &priv->txfid);
		if (err)
			goto out;
	}

	/* Now set up all the parameters on the card */
	
	/* Set up the link mode */
	
	err = hermes_write_wordrec(hw, USER_BAP, HERMES_RID_CNFPORTTYPE, priv->port_type);
	if (err)
		goto out;
	if (priv->has_ibss) {
		err = hermes_write_wordrec(hw, USER_BAP, HERMES_RID_CNFCREATEIBSS,
					   priv->allow_ibss);
		if (err)
			goto out;
		if((strlen(priv->desired_essid) == 0) && (priv->allow_ibss)
		   && (!priv->has_ibss_any)) {
			printk(KERN_WARNING "%s: This firmware requires an \
ESSID in IBSS-Ad-Hoc mode.\n", dev->name);
			/* With wvlan_cs, in this case, we would crash.
			 * hopefully, this driver will behave better...
			 * Jean II */
		}
	}

	/* Set the desired ESSID */
	idbuf.len = cpu_to_le16(strlen(priv->desired_essid));
	memcpy(&idbuf.val, priv->desired_essid, sizeof(idbuf.val));
	/* WinXP wants partner to configure OWNSSID even in IBSS mode. (jimc) */
	err = hermes_write_ltv(hw, USER_BAP, HERMES_RID_CNFOWNSSID,
			       HERMES_BYTES_TO_RECLEN(strlen(priv->desired_essid)+2),
			       &idbuf);
	if (err)
		goto out;
	err = hermes_write_ltv(hw, USER_BAP, HERMES_RID_CNFDESIREDSSID,
			       HERMES_BYTES_TO_RECLEN(strlen(priv->desired_essid)+2),
			       &idbuf);
	if (err)
		goto out;

	/* Set the station name */
	idbuf.len = cpu_to_le16(strlen(priv->nick));
	memcpy(&idbuf.val, priv->nick, sizeof(idbuf.val));
	err = hermes_write_ltv(hw, USER_BAP, HERMES_RID_CNFOWNNAME,
			       HERMES_BYTES_TO_RECLEN(strlen(priv->nick)+2),
			       &idbuf);
	if (err)
		goto out;

	/* Set the channel/frequency */
	err = hermes_write_wordrec(hw, USER_BAP, HERMES_RID_CNFOWNCHANNEL, priv->channel);
	if (err)
		goto out;

	/* Set AP density */
	if (priv->has_sensitivity) {
		err = hermes_write_wordrec(hw, USER_BAP, HERMES_RID_CNFSYSTEMSCALE,
					   priv->ap_density);
		if (err)
			priv->has_sensitivity = 0;
	}

	/* Set RTS threshold */
	err = hermes_write_wordrec(hw, USER_BAP, HERMES_RID_CNFRTSTHRESHOLD, priv->rts_thresh);
	if (err)
		goto out;

	/* Set fragmentation threshold or MWO robustness */
	if (priv->has_mwo)
		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFMWOROBUST_AGERE,
					   priv->mwo_robust);
	else
		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFFRAGMENTATIONTHRESHOLD,
					   priv->frag_thresh);
	if (err)
		goto out;

	/* Set bitrate */
	err = __orinoco_hw_set_bitrate(priv);
	if (err)
		goto out;

	/* Set power management */
	if (priv->has_pm) {
		err = hermes_write_wordrec(hw, USER_BAP, HERMES_RID_CNFPMENABLED,
					   priv->pm_on);
		if (err)
			goto out;
		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFMULTICASTRECEIVE,
					   priv->pm_mcast);
		if (err)
			goto out;
		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFMAXSLEEPDURATION,
					   priv->pm_period);
		if (err)
			goto out;
		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFPMHOLDOVERDURATION,
					   priv->pm_timeout);
		if (err)
			goto out;
	}

	/* Set preamble - only for Symbol so far... */
	if (priv->has_preamble) {
		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFPREAMBLE_SYMBOL,
					   priv->preamble);
		if (err) {
			printk(KERN_WARNING "%s: Can't set preamble!\n", dev->name);
			goto out;
		}
	}

	/* Set up encryption */
	if (priv->has_wep) {
		err = __orinoco_hw_setup_wep(priv);
		if (err) {
			printk(KERN_ERR "%s: Error %d activating WEP.\n",
			       dev->name, err);
			goto out;
		}
	}

	/* Set promiscuity / multicast*/
	priv->promiscuous = 0;
	priv->mc_count = 0;
	__orinoco_set_multicast_list(dev);
	
	__orinoco_start_irqs(priv, HERMES_EV_RX | HERMES_EV_ALLOC |
			   HERMES_EV_TX | HERMES_EV_TXEXC |
			   HERMES_EV_WTERR | HERMES_EV_INFO |
			   HERMES_EV_INFDROP);

	err = hermes_enable_port(hw, 0);
	if (err)
		goto out;

 out:
	orinoco_unlock(priv);

	TRACE_EXIT(priv->ndev->name);

	return err;
}

static int __orinoco_hw_set_bitrate(struct orinoco_private *priv)
{
	hermes_t *hw = &priv->hw;
	int err = 0;

	TRACE_ENTER(priv->ndev->name);

	if (priv->bitratemode >= BITRATE_TABLE_SIZE) {
		printk(KERN_ERR "%s: BUG: Invalid bitrate mode %d\n",
		       priv->ndev->name, priv->bitratemode);
		return -EINVAL;
	}

	switch (priv->firmware_type) {
	case FIRMWARE_TYPE_AGERE:
		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFTXRATECONTROL,
					   bitrate_table[priv->bitratemode].agere_txratectrl);
		break;
	case FIRMWARE_TYPE_INTERSIL:
	case FIRMWARE_TYPE_SYMBOL:
		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFTXRATECONTROL,
					   bitrate_table[priv->bitratemode].intersil_txratectrl);
		break;
	default:
		BUG();
	}

	TRACE_EXIT(priv->ndev->name);

	return err;
}


static int __orinoco_hw_setup_wep(struct orinoco_private *priv)
{
	hermes_t *hw = &priv->hw;
	int err = 0;
	int	master_wep_flag;
	int	auth_flag;

	TRACE_ENTER(priv->ndev->name);

	switch (priv->firmware_type) {
	case FIRMWARE_TYPE_AGERE: /* Agere style WEP */
		if (priv->wep_on) {
			err = hermes_write_wordrec(hw, USER_BAP,
						   HERMES_RID_CNFTXKEY_AGERE,
						   priv->tx_key);
			if (err)
				return err;
			
			err = HERMES_WRITE_RECORD(hw, USER_BAP,
						  HERMES_RID_CNFWEPKEYS_AGERE,
						  &priv->keys);
			if (err)
				return err;
		}
		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFWEPENABLED_AGERE,
					   priv->wep_on);
		if (err)
			return err;
		break;

	case FIRMWARE_TYPE_INTERSIL: /* Intersil style WEP */
	case FIRMWARE_TYPE_SYMBOL: /* Symbol style WEP */
		master_wep_flag = 0;		/* Off */
		if (priv->wep_on) {
			int keylen;
			int i;

			/* Fudge around firmware weirdness */
			keylen = le16_to_cpu(priv->keys[priv->tx_key].len);
			
			/* Write all 4 keys */
			for(i = 0; i < ORINOCO_MAX_KEYS; i++) {
/*  				int keylen = le16_to_cpu(priv->keys[i].len); */
				
				if (keylen > LARGE_KEY_SIZE) {
					printk(KERN_ERR "%s: BUG: Key %d has oversize length %d.\n",
					       priv->ndev->name, i, keylen);
					return -E2BIG;
				}

				err = hermes_write_ltv(hw, USER_BAP,
						       HERMES_RID_CNFDEFAULTKEY0 + i,
						       HERMES_BYTES_TO_RECLEN(keylen),
						       priv->keys[i].data);
				if (err)
					return err;
			}

			/* Write the index of the key used in transmission */
			err = hermes_write_wordrec(hw, USER_BAP, HERMES_RID_CNFWEPDEFAULTKEYID,
						   priv->tx_key);
			if (err)
				return err;
			
			if (priv->wep_restrict) {
				auth_flag = 2;
				master_wep_flag = 3;
			} else {
				/* Authentication is where Intersil and Symbol
				 * firmware differ... */
				auth_flag = 1;
				if (priv->firmware_type == FIRMWARE_TYPE_SYMBOL)
					master_wep_flag = 3; /* Symbol */ 
				else 
					master_wep_flag = 1; /* Intersil */
			}


			err = hermes_write_wordrec(hw, USER_BAP,
						   HERMES_RID_CNFAUTHENTICATION, auth_flag);
			if (err)
				return err;
		}
		
		/* Master WEP setting : on/off */
		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFWEPFLAGS_INTERSIL,
					   master_wep_flag);
		if (err)
			return err;	

		break;

	default:
		if (priv->wep_on) {
			printk(KERN_ERR "%s: WEP enabled, although not supported!\n",
			       priv->ndev->name);
			return -EINVAL;
		}
	}

	TRACE_EXIT(priv->ndev->name);

	return 0;
}

static int orinoco_hw_get_bssid(struct orinoco_private *priv, char buf[ETH_ALEN])
{
	hermes_t *hw = &priv->hw;
	int err = 0;

	orinoco_lock(priv);

	err = hermes_read_ltv(hw, USER_BAP, HERMES_RID_CURRENTBSSID,
			      ETH_ALEN, NULL, buf);

	orinoco_unlock(priv);

	return err;
}

static int orinoco_hw_get_essid(struct orinoco_private *priv, int *active,
			      char buf[IW_ESSID_MAX_SIZE+1])
{
	hermes_t *hw = &priv->hw;
	int err = 0;
	struct hermes_idstring essidbuf;
	char *p = (char *)(&essidbuf.val);
	int len;

	TRACE_ENTER(priv->ndev->name);

	orinoco_lock(priv);

	if (strlen(priv->desired_essid) > 0) {
		/* We read the desired SSID from the hardware rather
		   than from priv->desired_essid, just in case the
		   firmware is allowed to change it on us. I'm not
		   sure about this */
		/* My guess is that the OWNSSID should always be whatever
		 * we set to the card, whereas CURRENT_SSID is the one that
		 * may change... - Jean II */
		u16 rid;

		*active = 1;

		rid = (priv->port_type == 3) ? HERMES_RID_CNFOWNSSID :
			HERMES_RID_CNFDESIREDSSID;
		
		err = hermes_read_ltv(hw, USER_BAP, rid, sizeof(essidbuf),
				      NULL, &essidbuf);
		if (err)
			goto fail_unlock;
	} else {
		*active = 0;

		err = hermes_read_ltv(hw, USER_BAP, HERMES_RID_CURRENTSSID,
				      sizeof(essidbuf), NULL, &essidbuf);
		if (err)
			goto fail_unlock;
	}

	len = le16_to_cpu(essidbuf.len);

	memset(buf, 0, IW_ESSID_MAX_SIZE+1);
	memcpy(buf, p, len);
	buf[len] = '\0';

 fail_unlock:
	orinoco_unlock(priv);

	TRACE_EXIT(priv->ndev->name);

	return err;       
}

static long orinoco_hw_get_freq(struct orinoco_private *priv)
{
	
	hermes_t *hw = &priv->hw;
	int err = 0;
	u16 channel;
	long freq = 0;

	orinoco_lock(priv);
	
	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CURRENTCHANNEL, &channel);
	if (err)
		goto out;

	if ( (channel < 1) || (channel > NUM_CHANNELS) ) {
		struct net_device *dev = priv->ndev;

		printk(KERN_WARNING "%s: Channel out of range (%d)!\n", dev->name, channel);
		err = -EBUSY;
		goto out;

	}
	freq = channel_frequency[channel-1] * 100000;

 out:
	orinoco_unlock(priv);

	if (err > 0)
		err = -EBUSY;
	return err ? err : freq;
}

static int orinoco_hw_get_bitratelist(struct orinoco_private *priv, int *numrates,
				    s32 *rates, int max)
{
	hermes_t *hw = &priv->hw;
	struct hermes_idstring list;
	unsigned char *p = (unsigned char *)&list.val;
	int err = 0;
	int num;
	int i;

	orinoco_lock(priv);
	err = hermes_read_ltv(hw, USER_BAP, HERMES_RID_SUPPORTEDDATARATES,
			      sizeof(list), NULL, &list);
	orinoco_unlock(priv);

	if (err)
		return err;
	
	num = le16_to_cpu(list.len);
	*numrates = num;
	num = min(num, max);

	for (i = 0; i < num; i++) {
		rates[i] = (p[i] & 0x7f) * 500000; /* convert to bps */
	}

	return 0;
}

#if 0
#ifndef ORINOCO_DEBUG
static inline void show_rx_frame(struct orinoco_rxframe_hdr *frame) {}
#else
static void show_rx_frame(struct orinoco_rxframe_hdr *frame)
{
	printk(KERN_DEBUG "RX descriptor:\n");
	printk(KERN_DEBUG "  status      = 0x%04x\n", frame->desc.status);
	printk(KERN_DEBUG "  time        = 0x%08x\n", frame->desc.time);
	printk(KERN_DEBUG "  silence     = 0x%02x\n", frame->desc.silence);
	printk(KERN_DEBUG "  signal      = 0x%02x\n", frame->desc.signal);
	printk(KERN_DEBUG "  rate        = 0x%02x\n", frame->desc.rate);
	printk(KERN_DEBUG "  rxflow      = 0x%02x\n", frame->desc.rxflow);
	printk(KERN_DEBUG "  reserved    = 0x%08x\n", frame->desc.reserved);

	printk(KERN_DEBUG "IEEE 802.11 header:\n");
	printk(KERN_DEBUG "  frame_ctl   = 0x%04x\n",
	       frame->p80211.frame_ctl);
	printk(KERN_DEBUG "  duration_id = 0x%04x\n",
	       frame->p80211.duration_id);
	printk(KERN_DEBUG "  addr1       = %02x:%02x:%02x:%02x:%02x:%02x\n",
	       frame->p80211.addr1[0], frame->p80211.addr1[1],
	       frame->p80211.addr1[2], frame->p80211.addr1[3],
	       frame->p80211.addr1[4], frame->p80211.addr1[5]);
	printk(KERN_DEBUG "  addr2       = %02x:%02x:%02x:%02x:%02x:%02x\n",
	       frame->p80211.addr2[0], frame->p80211.addr2[1],
	       frame->p80211.addr2[2], frame->p80211.addr2[3],
	       frame->p80211.addr2[4], frame->p80211.addr2[5]);
	printk(KERN_DEBUG "  addr3       = %02x:%02x:%02x:%02x:%02x:%02x\n",
	       frame->p80211.addr3[0], frame->p80211.addr3[1],
	       frame->p80211.addr3[2], frame->p80211.addr3[3],
	       frame->p80211.addr3[4], frame->p80211.addr3[5]);
	printk(KERN_DEBUG "  seq_ctl     = 0x%04x\n",
	       frame->p80211.seq_ctl);
	printk(KERN_DEBUG "  addr4       = %02x:%02x:%02x:%02x:%02x:%02x\n",
	       frame->p80211.addr4[0], frame->p80211.addr4[1],
	       frame->p80211.addr4[2], frame->p80211.addr4[3],
	       frame->p80211.addr4[4], frame->p80211.addr4[5]);
	printk(KERN_DEBUG "  data_len    = 0x%04x\n",
	       frame->p80211.data_len);

	printk(KERN_DEBUG "IEEE 802.3 header:\n");
	printk(KERN_DEBUG "  dest        = %02x:%02x:%02x:%02x:%02x:%02x\n",
	       frame->p8023.h_dest[0], frame->p8023.h_dest[1],
	       frame->p8023.h_dest[2], frame->p8023.h_dest[3],
	       frame->p8023.h_dest[4], frame->p8023.h_dest[5]);
	printk(KERN_DEBUG "  src         = %02x:%02x:%02x:%02x:%02x:%02x\n",
	       frame->p8023.h_source[0], frame->p8023.h_source[1],
	       frame->p8023.h_source[2], frame->p8023.h_source[3],
	       frame->p8023.h_source[4], frame->p8023.h_source[5]);
	printk(KERN_DEBUG "  len         = 0x%04x\n", frame->p8023.h_proto);

	printk(KERN_DEBUG "IEEE 802.2 LLC/SNAP header:\n");
	printk(KERN_DEBUG "  DSAP        = 0x%02x\n", frame->p8022.dsap);
	printk(KERN_DEBUG "  SSAP        = 0x%02x\n", frame->p8022.ssap);
	printk(KERN_DEBUG "  ctrl        = 0x%02x\n", frame->p8022.ctrl);
	printk(KERN_DEBUG "  OUI         = %02x:%02x:%02x\n",
	       frame->p8022.oui[0], frame->p8022.oui[1], frame->p8022.oui[2]);
	printk(KERN_DEBUG "  ethertype  = 0x%04x\n", frame->ethertype);
}
#endif
#endif

/*
 * Interrupt handler
 */
void orinoco_interrupt(int irq, void * dev_id, struct pt_regs *regs)
{
	struct orinoco_private *priv = (struct orinoco_private *) dev_id;
	hermes_t *hw = &priv->hw;
	struct net_device *dev = priv->ndev;
	int count = MAX_IRQLOOPS_PER_IRQ;
	u16 evstat, events;
	/* These are used to detect a runaway interrupt situation */
	/* If we get more than MAX_IRQLOOPS_PER_JIFFY iterations in a jiffy,
	 * we panic and shut down the hardware */
	static int last_irq_jiffy = 0; /* jiffies value the last time we were called */
	static int loops_this_jiffy = 0;

	if (test_and_set_bit(ORINOCO_STATE_INIRQ, &priv->state))
		BUG();

	if (! orinoco_irqs_allowed(priv)) {
		clear_bit(ORINOCO_STATE_INIRQ, &priv->state);
		return;
	}

	DEBUG(3, "%s: orinoco_interrupt()\n", priv->ndev->name);

	evstat = hermes_read_regn(hw, EVSTAT);
	events = evstat & hw->inten;
	
/*  	if (! events) { */
/*  		printk(KERN_WARNING "%s: Null event\n", dev->name); */
/*  	} */

	if (jiffies != last_irq_jiffy)
		loops_this_jiffy = 0;
	last_irq_jiffy = jiffies;

	while (events && count--) {
		DEBUG(3, "__orinoco_interrupt(): count=%d EVSTAT=0x%04x\n",
		      count, evstat);
		
		if (++loops_this_jiffy > MAX_IRQLOOPS_PER_JIFFY) {
			printk(KERN_CRIT "%s: IRQ handler is looping too \
much! Shutting down.\n",
			       dev->name);
			/* Perform an emergency shutdown */
			clear_bit(ORINOCO_STATE_DOIRQ, &priv->state);
			hermes_set_irqmask(hw, 0);
			break;
		}

		/* Check the card hasn't been removed */
		if (! hermes_present(hw)) {
			DEBUG(0, "orinoco_interrupt(): card removed\n");
			break;
		}

		if (events & HERMES_EV_TICK)
			__orinoco_ev_tick(priv, hw);
		if (events & HERMES_EV_WTERR)
			__orinoco_ev_wterr(priv, hw);
		if (events & HERMES_EV_INFDROP)
			__orinoco_ev_infdrop(priv, hw);
		if (events & HERMES_EV_INFO)
			__orinoco_ev_info(priv, hw);
		if (events & HERMES_EV_RX)
			__orinoco_ev_rx(priv, hw);
		if (events & HERMES_EV_TXEXC)
			__orinoco_ev_txexc(priv, hw);
		if (events & HERMES_EV_TX)
			__orinoco_ev_tx(priv, hw);
		if (events & HERMES_EV_ALLOC)
			__orinoco_ev_alloc(priv, hw);
		
		hermes_write_regn(hw, EVACK, events);

		evstat = hermes_read_regn(hw, EVSTAT);
		events = evstat & hw->inten;
	};

	clear_bit(ORINOCO_STATE_INIRQ, &priv->state);
}

static void __orinoco_ev_tick(struct orinoco_private *priv, hermes_t *hw)
{
	printk(KERN_DEBUG "%s: TICK\n", priv->ndev->name);
}

static void __orinoco_ev_wterr(struct orinoco_private *priv, hermes_t *hw)
{
	/* This seems to happen a fair bit under load, but ignoring it
	   seems to work fine...*/
	DEBUG(1, "%s: MAC controller error (WTERR). Ignoring.\n",
	      priv->ndev->name);
}

static void __orinoco_ev_infdrop(struct orinoco_private *priv, hermes_t *hw)
{
	printk(KERN_WARNING "%s: Information frame lost.\n", priv->ndev->name);
}

static void __orinoco_ev_info(struct orinoco_private *priv, hermes_t *hw)
{
	struct net_device *dev = priv->ndev;
	u16 infofid;
	struct {
		u16 len;
		u16 type;
	} __attribute__ ((packed)) info;
	int err;

	/* This is an answer to an INQUIRE command that we did earlier,
	 * or an information "event" generated by the card
	 * The controller return to us a pseudo frame containing
	 * the information in question - Jean II */
	infofid = hermes_read_regn(hw, INFOFID);
	DEBUG(3, "%s: __orinoco_ev_info(): INFOFID=0x%04x\n", dev->name,
	      infofid);

	/* Read the info frame header - don't try too hard */
	err = hermes_bap_pread(hw, IRQ_BAP, &info, sizeof(info),
			       infofid, 0);
	if (err) {
		printk(KERN_ERR "%s: error %d reading info frame. "
		       "Frame dropped.\n", dev->name, err);
		return;
	}
	
	switch (le16_to_cpu(info.type)) {
	case HERMES_INQ_TALLIES: {
		struct hermes_tallies_frame tallies;
		struct iw_statistics *wstats = &priv->wstats;
		int len = le16_to_cpu(info.len) - 1;
		
		if (len > (sizeof(tallies) / 2)) {
			DEBUG(1, "%s: tallies frame too long.\n", dev->name);
			len = sizeof(tallies) / 2;
		}
		
		/* Read directly the data (no seek) */
		hermes_read_words(hw, HERMES_DATA1, (void *) &tallies, len);
		
		/* Increment our various counters */
		/* wstats->discard.nwid - no wrong BSSID stuff */
		wstats->discard.code +=
			le16_to_cpu(tallies.RxWEPUndecryptable);
		if (len == (sizeof(tallies) / 2))  
			wstats->discard.code +=
				le16_to_cpu(tallies.RxDiscards_WEPICVError) +
				le16_to_cpu(tallies.RxDiscards_WEPExcluded);
		wstats->discard.misc +=
			le16_to_cpu(tallies.TxDiscardsWrongSA);
#if WIRELESS_EXT > 11
		wstats->discard.fragment +=
			le16_to_cpu(tallies.RxMsgInBadMsgFragments);
		wstats->discard.retries +=
			le16_to_cpu(tallies.TxRetryLimitExceeded);
		/* wstats->miss.beacon - no match */
#if ORINOCO_DEBUG > 3
		/* Hack for debugging - should not be taken as an example */
		wstats->discard.nwid += le16_to_cpu(tallies.TxUnicastFrames);
		wstats->miss.beacon += le16_to_cpu(tallies.RxUnicastFrames);
#endif
#endif /* WIRELESS_EXT > 11 */
	}
	break;
	default:
		DEBUG(1, "%s: Unknown information frame received (type %04x).\n",
		      priv->ndev->name, le16_to_cpu(info.type));
		/* We don't actually do anything about it */
		break;
	}
}

static void __orinoco_ev_rx(struct orinoco_private *priv, hermes_t *hw)
{
	struct net_device *dev = priv->ndev;
	struct net_device_stats *stats = &priv->stats;
	struct iw_statistics *wstats = &priv->wstats;
	struct sk_buff *skb = NULL;
	u16 rxfid, status;
	int length, data_len, data_off;
	char *p;
	struct hermes_rx_descriptor desc;
	struct header_struct hdr;
	struct ethhdr *eh;
	int err;

	rxfid = hermes_read_regn(hw, RXFID);
	DEBUG(3, "__orinoco_ev_rx(): RXFID=0x%04x\n", rxfid);

	err = hermes_bap_pread(hw, IRQ_BAP, &desc, sizeof(desc),
			       rxfid, 0);
	if (err) {
		printk(KERN_ERR "%s: error %d reading Rx descriptor. "
		       "Frame dropped.\n", dev->name, err);
		stats->rx_errors++;
		goto drop;
	}

	status = le16_to_cpu(desc.status);
	
	if (status & HERMES_RXSTAT_ERR) {
		if (status & HERMES_RXSTAT_UNDECRYPTABLE) {
			wstats->discard.code++;
			DEBUG(1, "%s: Undecryptable frame on Rx. Frame dropped.\n",
			       dev->name);
		} else {
			stats->rx_crc_errors++;
			DEBUG(1, "%s: Bad CRC on Rx. Frame dropped.\n", dev->name);
		}
		stats->rx_errors++;
		goto drop;
	}

	/* For now we ignore the 802.11 header completely, assuming
           that the card's firmware has handled anything vital */

	err = hermes_bap_pread(hw, IRQ_BAP, &hdr, sizeof(hdr),
			       rxfid, HERMES_802_3_OFFSET);
	if (err) {
		printk(KERN_ERR "%s: error %d reading frame header. "
		       "Frame dropped.\n", dev->name, err);
		stats->rx_errors++;
		goto drop;
	}

	length = ntohs(hdr.len);
	
	/* Sanity checks */
	if (length < sizeof(struct header_struct)) {
		printk(KERN_WARNING "%s: Undersized frame received (%d bytes)\n",
		       dev->name, length);
		stats->rx_length_errors++;
		stats->rx_errors++;
		goto drop;
	}
	if (length > IEEE802_11_DATA_LEN) {
		printk(KERN_WARNING "%s: Oversized frame received (%d bytes)\n",
		       dev->name, length);
		stats->rx_length_errors++;
		stats->rx_errors++;
		goto drop;
	}

	/* We need space for the packet data itself, plus an ethernet
	   header, plus 2 bytes so we can align the IP header on a
	   32bit boundary, plus 1 byte so we can read in odd length
	   packets from the card, which has an IO granularity of 16
	   bits */  
	skb = dev_alloc_skb(length+ETH_HLEN+2+1);
	if (!skb) {
		printk(KERN_WARNING "%s: Can't allocate skb for Rx\n",
		       dev->name);
		stats->rx_dropped++;
		goto drop;
	}

	skb_reserve(skb, 2); /* This way the IP header is aligned */

	/* Handle decapsulation
	 * In most cases, the firmware tell us about SNAP frames.
	 * For some reason, the SNAP frames sent by LinkSys APs
	 * are not properly recognised by most firmwares.
	 * So, check ourselves (note : only 3 bytes out of 6).
	 */
	if(((status & HERMES_RXSTAT_MSGTYPE) == HERMES_RXSTAT_1042) ||
	   ((status & HERMES_RXSTAT_MSGTYPE) == HERMES_RXSTAT_TUNNEL) ||
	   is_snap(&hdr)) {
		/* These indicate a SNAP within 802.2 LLC within
		   802.11 frame which we'll need to de-encapsulate to
		   the original EthernetII frame. */
		   
		if (length < ENCAPS_OVERHEAD) {
			stats->rx_length_errors++;
			stats->rx_dropped++;
			goto drop;
		}
		

		/* Remove SNAP header, reconstruct EthernetII frame */
		data_len = length - ENCAPS_OVERHEAD;
		data_off = HERMES_802_3_OFFSET + sizeof(hdr);

		eh = (struct ethhdr *)skb_put(skb, ETH_HLEN);

		memcpy(eh, &hdr, 2 * ETH_ALEN);
		eh->h_proto = hdr.ethertype;
	} else {
		/* All other cases indicate a genuine 802.3 frame.  No
		   decapsulation needed.  We just throw the whole
		   thing in, and hope the protocol layer can deal with
		   it as 802.3 */
		data_len = length;
		data_off = HERMES_802_3_OFFSET;
		/* FIXME: we re-read from the card data we already read here */
	}

	p = skb_put(skb, data_len);
	err = hermes_bap_pread(hw, IRQ_BAP, p, RUP_EVEN(data_len),
			       rxfid, data_off);
	if (err) {
		printk(KERN_ERR "%s: error %d reading frame header. "
		       "Frame dropped.\n", dev->name, err);
		stats->rx_errors++;
		goto drop;
	}

	dev->last_rx = jiffies;
	skb->dev = dev;
	skb->protocol = eth_type_trans(skb, dev);
	skb->ip_summed = CHECKSUM_NONE;
	
	/* Process the wireless stats if needed */
	orinoco_stat_gather(dev, skb, &desc);

	/* Pass the packet to the networking stack */
	netif_rx(skb);
	stats->rx_packets++;
	stats->rx_bytes += length;

	return;

 drop:	
	if (skb)
		dev_kfree_skb_irq(skb);
	return;
}

static void __orinoco_ev_txexc(struct orinoco_private *priv, hermes_t *hw)
{
	struct net_device *dev = priv->ndev;
	struct net_device_stats *stats = &priv->stats;
	u16 fid = hermes_read_regn(hw, TXCOMPLFID);
	struct hermes_tx_descriptor desc;
	int err = 0;

	if (fid == DUMMY_FID)
		return; /* Nothing's really happened */

	err = hermes_bap_pread(hw, IRQ_BAP, &desc, sizeof(desc), fid, 0);
	if (err) {
		printk(KERN_WARNING "%s: Unable to read descriptor on Tx error "
		       "(FID=%04X error %d)\n",
		       dev->name, fid, err);
	} else {
		DEBUG(1, "%s: Tx error, status %d\n",
		      dev->name, le16_to_cpu(desc.status));
	}
	
	stats->tx_errors++;

	hermes_write_regn(hw, TXCOMPLFID, DUMMY_FID);
}

static void __orinoco_ev_tx(struct orinoco_private *priv, hermes_t *hw)
{
/*  	struct net_device *dev = priv->ndev; */
	struct net_device_stats *stats = &priv->stats;
/*  	u16 fid = hermes_read_regn(hw, TXCOMPLFID); */

/*  	DEBUG(2, "%s: Transmit completed (FID=%04X)\n", priv->ndev->name, fid); */

	stats->tx_packets++;

	hermes_write_regn(hw, TXCOMPLFID, DUMMY_FID);
}

static void __orinoco_ev_alloc(struct orinoco_private *priv, hermes_t *hw)
{
	struct net_device *dev = priv->ndev;
	u16 fid = hermes_read_regn(hw, ALLOCFID);

	DEBUG(3, "%s: Allocation complete FID=0x%04x\n", priv->ndev->name, fid);

	if (fid != priv->txfid) {
		if (fid != DUMMY_FID)
			printk(KERN_WARNING "%s: Allocate event on unexpected fid (%04X)\n",
			       dev->name, fid);
		return;
	} else {
		netif_wake_queue(dev);
	}

	hermes_write_regn(hw, ALLOCFID, DUMMY_FID);
}

struct sta_id {
	u16 id, vendor, major, minor;
} __attribute__ ((packed));

static int determine_firmware_type(struct net_device *dev, struct sta_id *sta_id)
{
	u32 firmver = ((u32)sta_id->major << 16) | sta_id->minor;
	
	if (sta_id->vendor == 1)
		return FIRMWARE_TYPE_AGERE;
	else if ((sta_id->vendor == 2) &&
		   ((firmver == 0x10001) || (firmver == 0x20001)))
		return FIRMWARE_TYPE_SYMBOL;
	else
		return FIRMWARE_TYPE_INTERSIL;
}

static void determine_firmware(struct net_device *dev)
{
	struct orinoco_private *priv = dev->priv;
	hermes_t *hw = &priv->hw;
	int err;
	struct sta_id sta_id;
	u32 firmver;
	char tmp[SYMBOL_MAX_VER_LEN+1];

	/* Get the firmware version */
	err = HERMES_READ_RECORD(hw, USER_BAP, HERMES_RID_STAID, &sta_id);
	if (err) {
		printk(KERN_WARNING "%s: Error %d reading firmware info. Wildly guessing capabilities...\n",
		       dev->name, err);
		memset(&sta_id, 0, sizeof(sta_id));
	}
	le16_to_cpus(&sta_id.id);
	le16_to_cpus(&sta_id.vendor);
	le16_to_cpus(&sta_id.major);
	le16_to_cpus(&sta_id.minor);

	firmver = ((u32)sta_id.major << 16) | sta_id.minor;

	printk(KERN_DEBUG "%s: Station identity %04x:%04x:%04x:%04x\n",
	       dev->name, sta_id.id, sta_id.vendor,
	       sta_id.major, sta_id.minor);

	if (! priv->firmware_type)
		priv->firmware_type = determine_firmware_type(dev, &sta_id);

	/* Default capabilities */
	priv->has_sensitivity = 1;
	priv->has_mwo = 0;
	priv->has_preamble = 0;
	priv->has_port3 = 1;
	priv->has_ibss = 1;
	priv->has_ibss_any = 0;
	priv->has_wep = 0;
	priv->has_big_wep = 0;
	priv->broken_cor_reset = 0;

	/* Determine capabilities from the firmware version */
	switch (priv->firmware_type) {
	case FIRMWARE_TYPE_AGERE:
		/* Lucent Wavelan IEEE, Lucent Orinoco, Cabletron RoamAbout,
		   ELSA, Melco, HP, IBM, Dell 1150, Compaq 110/210 */
		printk(KERN_DEBUG "%s: Looks like a Lucent/Agere firmware "
		       "version %d.%02d\n", dev->name,
		       sta_id.major, sta_id.minor);

		priv->has_ibss = (firmver >= 0x60006);
		priv->has_ibss_any = (firmver >= 0x60010);
		priv->has_wep = (firmver >= 0x40020);
		priv->has_big_wep = 1; /* FIXME: this is wrong - how do we tell
					  Gold cards from the others? */
		priv->has_mwo = (firmver >= 0x60000);
		priv->has_pm = (firmver >= 0x40020); /* Don't work in 7.52 ? */
		priv->ibss_port = 1;

		/* FIXME: Which firmware really do have a broken reset */
		priv->broken_cor_reset = (firmver < 0x60000);
		/* Tested with Agere firmware :
		 *	1.16 ; 4.08 ; 4.52 ; 6.04 ; 6.16 ; 7.28 => Jean II
		 * Tested CableTron firmware : 4.32 => Anton */
		break;
	case FIRMWARE_TYPE_SYMBOL:
		/* Symbol , 3Com AirConnect, Intel, Ericsson WLAN */
		/* Intel MAC : 00:02:B3:* */
		/* 3Com MAC : 00:50:DA:* */
		memset(tmp, 0, sizeof(tmp));
		/* Get the Symbol firmware version */
		err = hermes_read_ltv(hw, USER_BAP,
				      HERMES_RID_SECONDARYVERSION_SYMBOL,
				      SYMBOL_MAX_VER_LEN, NULL, &tmp);
		if (err) {
			printk(KERN_WARNING
			       "%s: Error %d reading Symbol firmware info. Wildly guessing capabilities...\n",
			       dev->name, err);
			firmver = 0;
			tmp[0] = '\0';
		} else {
			/* The firmware revision is a string, the format is
			 * something like : "V2.20-01".
			 * Quick and dirty parsing... - Jean II
			 */
			firmver = ((tmp[1] - '0') << 16) | ((tmp[3] - '0') << 12)
				| ((tmp[4] - '0') << 8) | ((tmp[6] - '0') << 4)
				| (tmp[7] - '0');

			tmp[SYMBOL_MAX_VER_LEN] = '\0';
		}

		printk(KERN_DEBUG "%s: Looks like a Symbol firmware "
		       "version [%s] (parsing to %X)\n", dev->name,
		       tmp, firmver);

		priv->has_ibss = (firmver >= 0x20000);
		priv->has_wep = (firmver >= 0x15012);
		priv->has_big_wep = (firmver >= 0x20000);
		priv->has_pm = (firmver >= 0x20000) && (firmver < 0x22000);
		priv->has_preamble = (firmver >= 0x20000);
		priv->ibss_port = 4;
		/* Tested with Intel firmware : 0x20015 => Jean II */
		/* Tested with 3Com firmware : 0x15012 & 0x22001 => Jean II */
		break;
	case FIRMWARE_TYPE_INTERSIL:
		/* D-Link, Linksys, Adtron, ZoomAir, and many others...
		 * Samsung, Compaq 100/200 and Proxim are slightly
		 * different and less well tested */
		/* D-Link MAC : 00:40:05:* */
		/* Addtron MAC : 00:90:D1:* */
		printk(KERN_DEBUG "%s: Looks like an Intersil firmware "
		       "version %d.%02d\n", dev->name,
		       sta_id.major, sta_id.minor);

		priv->has_ibss = (firmver >= 0x00007); /* FIXME */
		priv->has_big_wep = priv->has_wep = (firmver >= 0x00008);
		priv->has_pm = (firmver >= 0x00007);

		if (firmver >= 0x00008)
			priv->ibss_port = 0;
		else {
			printk(KERN_NOTICE "%s: Intersil firmware earlier "
			       "than v0.08 - several features not supported\n",
			       dev->name);
			priv->ibss_port = 1;
		}
		break;
	default:
		break;
	}
}

/*
 * struct net_device methods
 */

static int
orinoco_init(struct net_device *dev)
{
	struct orinoco_private *priv = dev->priv;
	hermes_t *hw = &priv->hw;
	int err = 0;
	struct hermes_idstring nickbuf;
	u16 reclen;
	int len;

	TRACE_ENTER("orinoco");

	orinoco_lock(priv);

	priv->nicbuf_size = IEEE802_11_FRAME_LEN + ETH_HLEN;

	/* Do standard firmware reset */
	err = hermes_reset(hw);
	if (err != 0) {
		printk(KERN_ERR "%s: failed to reset hardware (err = %d)\n",
		       dev->name, err);
		goto out;
	}

	determine_firmware(dev);

	if (priv->has_port3)
		printk(KERN_DEBUG "%s: Ad-hoc demo mode supported\n", dev->name);
	if (priv->has_ibss)
		printk(KERN_DEBUG "%s: IEEE standard IBSS ad-hoc mode supported\n",
		       dev->name);
	if (priv->has_wep) {
		printk(KERN_DEBUG "%s: WEP supported, ", dev->name);
		if (priv->has_big_wep)
			printk("104-bit key\n");
		else
			printk("40-bit key\n");
	}

	/* Get the MAC address */
	err = hermes_read_ltv(hw, USER_BAP, HERMES_RID_CNFOWNMACADDR,
			      ETH_ALEN, NULL, dev->dev_addr);
	if (err) {
		printk(KERN_WARNING "%s: failed to read MAC address!\n",
		       dev->name);
		goto out;
	}

	printk(KERN_DEBUG "%s: MAC address %02X:%02X:%02X:%02X:%02X:%02X\n",
	       dev->name, dev->dev_addr[0], dev->dev_addr[1],
	       dev->dev_addr[2], dev->dev_addr[3], dev->dev_addr[4],
	       dev->dev_addr[5]);

	/* Get the station name */
	err = hermes_read_ltv(hw, USER_BAP, HERMES_RID_CNFOWNNAME,
			      sizeof(nickbuf), &reclen, &nickbuf);
	if (err) {
		printk(KERN_ERR "%s: failed to read station name\n",
		       dev->name);
		goto out;
	}
	if (nickbuf.len)
		len = min(IW_ESSID_MAX_SIZE, (int)le16_to_cpu(nickbuf.len));
	else
		len = min(IW_ESSID_MAX_SIZE, 2 * reclen);
	memcpy(priv->nick, &nickbuf.val, len);
	priv->nick[len] = '\0';

	printk(KERN_DEBUG "%s: Station name \"%s\"\n", dev->name, priv->nick);

	/* Get allowed channels */
	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CHANNELLIST,
				  &priv->channel_mask);
	if (err) {
		printk(KERN_ERR "%s: failed to read channel list!\n",
		       dev->name);
		goto out;
	}

	/* Get initial AP density */
	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CNFSYSTEMSCALE,
				  &priv->ap_density);
	if (err || priv->ap_density < 1 || priv->ap_density > 3) {
		priv->has_sensitivity = 0;
	}

	/* Get initial RTS threshold */
	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CNFRTSTHRESHOLD,
				  &priv->rts_thresh);
	if (err) {
		printk(KERN_ERR "%s: failed to read RTS threshold!\n", dev->name);
		goto out;
	}

	/* Get initial fragmentation settings */
	if (priv->has_mwo)
		err = hermes_read_wordrec(hw, USER_BAP,
					  HERMES_RID_CNFMWOROBUST_AGERE,
					  &priv->mwo_robust);
	else
		err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CNFFRAGMENTATIONTHRESHOLD,
					  &priv->frag_thresh);
	if (err) {
		printk(KERN_ERR "%s: failed to read fragmentation settings!\n", dev->name);
		goto out;
	}

	/* Power management setup */
	if (priv->has_pm) {
		priv->pm_on = 0;
		priv->pm_mcast = 1;
		err = hermes_read_wordrec(hw, USER_BAP,
					  HERMES_RID_CNFMAXSLEEPDURATION,
					  &priv->pm_period);
		if (err) {
			printk(KERN_ERR "%s: failed to read power management period!\n",
			       dev->name);
			goto out;
		}
		err = hermes_read_wordrec(hw, USER_BAP,
					  HERMES_RID_CNFPMHOLDOVERDURATION,
					  &priv->pm_timeout);
		if (err) {
			printk(KERN_ERR "%s: failed to read power management timeout!\n",
			       dev->name);
			goto out;
		}
	}

	/* Preamble setup */
	if (priv->has_preamble) {
		err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CNFPREAMBLE_SYMBOL,
					  &priv->preamble);
		if (err)
			goto out;
	}
		
	/* Set up the default configuration */
	priv->iw_mode = IW_MODE_INFRA;
	/* By default use IEEE/IBSS ad-hoc mode if we have it */
	priv->prefer_port3 = priv->has_port3 && (! priv->has_ibss);
	set_port_type(priv);
	priv->channel = 10; /* default channel, more-or-less arbitrary */

	priv->promiscuous = 0;
	priv->wep_on = 0;
	priv->tx_key = 0;

	printk(KERN_DEBUG "%s: ready\n", dev->name);

 out:
	orinoco_unlock(priv);

	TRACE_EXIT("orinoco");

	return err;
}

struct net_device_stats *
orinoco_get_stats(struct net_device *dev)
{
	struct orinoco_private *priv = (struct orinoco_private *)dev->priv;
	
	return &priv->stats;
}

struct iw_statistics *
orinoco_get_wireless_stats(struct net_device *dev)
{
	struct orinoco_private *priv = (struct orinoco_private *)dev->priv;
	hermes_t *hw = &priv->hw;
	struct iw_statistics *wstats = &priv->wstats;
	int err = 0;

	if (! netif_device_present(dev))
		return NULL; /* FIXME: We may be able to do better than this */

	orinoco_lock(priv);

	if (priv->iw_mode == IW_MODE_ADHOC) {
		memset(&wstats->qual, 0, sizeof(wstats->qual));
		/* If a spy address is defined, we report stats of the
		 * first spy address - Jean II */
		if (SPY_NUMBER(priv)) {
			wstats->qual.qual = priv->spy_stat[0].qual;
			wstats->qual.level = priv->spy_stat[0].level;
			wstats->qual.noise = priv->spy_stat[0].noise;
			wstats->qual.updated = priv->spy_stat[0].updated;
		}
	} else {
		struct {
			u16 qual, signal, noise;
		} __attribute__ ((packed)) cq;

		err = HERMES_READ_RECORD(hw, USER_BAP,
					 HERMES_RID_COMMSQUALITY, &cq);
		
		DEBUG(3, "%s: Global stats = %X-%X-%X\n", dev->name,
		      cq.qual, cq.signal, cq.noise);

		wstats->qual.qual = (int)le16_to_cpu(cq.qual);
		wstats->qual.level = (int)le16_to_cpu(cq.signal) - 0x95;
		wstats->qual.noise = (int)le16_to_cpu(cq.noise) - 0x95;
		wstats->qual.updated = 7;
	}

	/* We can't really wait for the tallies inquiry command to
	 * complete, so we just use the previous results and trigger
	 * a new tallies inquiry command for next time - Jean II */
	/* FIXME: Hmm.. seems a bit ugly, I wonder if there's a way to
	   do better - dgibson */
	err = hermes_inquire(hw, HERMES_INQ_TALLIES);
               
	orinoco_unlock(priv);

	if (err)
		return NULL;
		
	return wstats;
}

static inline void orinoco_spy_gather(struct net_device *dev, u_char *mac,
				    int level, int noise)
{
	struct orinoco_private *priv = (struct orinoco_private *)dev->priv;
	int i;

	/* Gather wireless spy statistics: for each packet, compare the
	 * source address with out list, and if match, get the stats... */
	for (i = 0; i < priv->spy_number; i++)
		if (!memcmp(mac, priv->spy_address[i], ETH_ALEN)) {
			priv->spy_stat[i].level = level - 0x95;
			priv->spy_stat[i].noise = noise - 0x95;
			priv->spy_stat[i].qual = (level > noise) ? (level - noise) : 0;
			priv->spy_stat[i].updated = 7;
		}
}

void
orinoco_stat_gather(struct net_device *dev,
		    struct sk_buff *skb,
		    struct hermes_rx_descriptor *desc)
{
	struct orinoco_private *priv = (struct orinoco_private *)dev->priv;

	/* Using spy support with lots of Rx packets, like in an
	 * infrastructure (AP), will really slow down everything, because
	 * the MAC address must be compared to each entry of the spy list.
	 * If the user really asks for it (set some address in the
	 * spy list), we do it, but he will pay the price.
	 * Note that to get here, you need both WIRELESS_SPY
	 * compiled in AND some addresses in the list !!!
	 */
	/* Note : gcc will optimise the whole section away if
	 * WIRELESS_SPY is not defined... - Jean II */
	if (SPY_NUMBER(priv)) {
		orinoco_spy_gather(dev, skb->mac.raw + ETH_ALEN,
				   desc->signal, desc->silence);
	}
}

static int
orinoco_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct orinoco_private *priv = (struct orinoco_private *)dev->priv;
	struct net_device_stats *stats = &priv->stats;
	hermes_t *hw = &priv->hw;
	int err = 0;
	u16 txfid = priv->txfid;
	char *p;
	struct ethhdr *eh;
	int len, data_len, data_off;
	struct hermes_tx_descriptor desc;

	if (! netif_running(dev)) {
		printk(KERN_ERR "%s: Tx on stopped device!\n",
		       dev->name);
		return 1;

	}
	
	if (netif_queue_stopped(dev)) {
		printk(KERN_ERR "%s: Tx while transmitter busy!\n", 
		       dev->name);
		return 1;
	}
	
	orinoco_lock(priv);

	/* Length of the packet body */
	/* FIXME: what if the skb is smaller than this? */
	len = max_t(int,skb->len - ETH_HLEN, ETH_ZLEN);

	eh = (struct ethhdr *)skb->data;

	memset(&desc, 0, sizeof(desc));
 	desc.tx_control = cpu_to_le16(HERMES_TXCTRL_TX_OK | HERMES_TXCTRL_TX_EX);
	err = hermes_bap_pwrite(hw, USER_BAP, &desc, sizeof(desc), txfid, 0);
	if (err) {
		printk(KERN_ERR "%s: Error %d writing Tx descriptor to BAP\n",
		       dev->name, err);
		stats->tx_errors++;
		goto fail;
	}

	/* Encapsulate Ethernet-II frames */
	if (ntohs(eh->h_proto) > 1500) { /* Ethernet-II frame */
		struct header_struct hdr;
		data_len = len;
		data_off = HERMES_802_3_OFFSET + sizeof(hdr);
		p = skb->data + ETH_HLEN;

		/* 802.3 header */
		memcpy(hdr.dest, eh->h_dest, ETH_ALEN);
		memcpy(hdr.src, eh->h_source, ETH_ALEN);
		hdr.len = htons(data_len + ENCAPS_OVERHEAD);
		
		/* 802.2 header */
		memcpy(&hdr.dsap, &encaps_hdr, sizeof(encaps_hdr));
			
		hdr.ethertype = eh->h_proto;
		err  = hermes_bap_pwrite(hw, USER_BAP, &hdr, sizeof(hdr),
					 txfid, HERMES_802_3_OFFSET);
		if (err) {
			printk(KERN_ERR "%s: Error %d writing packet header to BAP\n",
			       dev->name, err);
			stats->tx_errors++;
			goto fail;
		}
	} else { /* IEEE 802.3 frame */
		data_len = len + ETH_HLEN;
		data_off = HERMES_802_3_OFFSET;
		p = skb->data;
	}

	/* Round up for odd length packets */
	err = hermes_bap_pwrite(hw, USER_BAP, p, RUP_EVEN(data_len), txfid, data_off);
	if (err) {
		printk(KERN_ERR "%s: Error %d writing packet to BAP\n",
		       dev->name, err);
		stats->tx_errors++;
		goto fail;
	}

	/* Finally, we actually initiate the send */
	netif_stop_queue(dev);

	err = hermes_docmd_wait(hw, HERMES_CMD_TX | HERMES_CMD_RECL, txfid, NULL);
	if (err) {
		netif_start_queue(dev);
		printk(KERN_ERR "%s: Error %d transmitting packet\n", dev->name, err);
		stats->tx_errors++;
		goto fail;
	}

	dev->trans_start = jiffies;
	stats->tx_bytes += data_off + data_len;

	orinoco_unlock(priv);

	dev_kfree_skb(skb);

	return 0;
 fail:

	orinoco_unlock(priv);
	return err;
}

static void
orinoco_tx_timeout(struct net_device *dev)
{
	struct orinoco_private *priv = (struct orinoco_private *)dev->priv;
	struct net_device_stats *stats = &priv->stats;
	struct hermes *hw = &priv->hw;
	int err = 0;

	printk(KERN_WARNING "%s: Tx timeout! Resetting card. ALLOCFID=%04x, TXCOMPLFID=%04x, EVSTAT=%04x\n", dev->name, hermes_read_regn(hw, ALLOCFID), hermes_read_regn(hw, TXCOMPLFID), hermes_read_regn(hw, EVSTAT));

	stats->tx_errors++;

	err = orinoco_reset(priv);
	if (err)
		printk(KERN_ERR "%s: Error %d resetting card on Tx timeout!\n",
		       dev->name, err);
	else {
		dev->trans_start = jiffies;
		netif_wake_queue(dev);
	}
}

static int
orinoco_change_mtu(struct net_device *dev, int new_mtu)
{
	struct orinoco_private *priv = dev->priv;
	TRACE_ENTER(dev->name);

	if ( (new_mtu < ORINOCO_MIN_MTU) || (new_mtu > ORINOCO_MAX_MTU) )
		return -EINVAL;

	if ( (new_mtu + ENCAPS_OVERHEAD + IEEE802_11_HLEN) >
	     (priv->nicbuf_size - ETH_HLEN) )
		return -EINVAL;

	dev->mtu = new_mtu;

	TRACE_EXIT(dev->name);

	return 0;
}

static void
__orinoco_set_multicast_list(struct net_device *dev)
{
	struct orinoco_private *priv = dev->priv;
	hermes_t *hw = &priv->hw;
	int err = 0;
	int promisc, mc_count;

	/* We'll wait until it's ready. Anyway, the network doesn't call us
	 * here until we are open - Jean II */
	/* FIXME: do we need this test at all? */
	if (! netif_device_present(dev)) {
		printk(KERN_WARNING "%s: __orinoco_set_multicast_list() called while device "
		       "not present.\n", dev->name);
		return;
	}

	TRACE_ENTER(dev->name);

	/* The Hermes doesn't seem to have an allmulti mode, so we go
	 * into promiscuous mode and let the upper levels deal. */
	if ( (dev->flags & IFF_PROMISC) || (dev->flags & IFF_ALLMULTI) ||
	     (dev->mc_count > MAX_MULTICAST(priv)) ) {
		promisc = 1;
		mc_count = 0;
	} else {
		promisc = 0;
		mc_count = dev->mc_count;
	}

	if (promisc != priv->promiscuous) {
		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFPROMISCUOUSMODE,
					   promisc);
		if (err) {
			printk(KERN_ERR "%s: Error %d setting PROMISCUOUSMODE to 1.\n",
			       dev->name, err);
		} else 
			priv->promiscuous = promisc;
	}

	if (! promisc && (mc_count || priv->mc_count) ) {
		struct dev_mc_list *p = dev->mc_list;
		hermes_multicast_t mclist;
		int i;

		for (i = 0; i < mc_count; i++) {
			/* Paranoia: */
			if (! p)
				BUG(); /* Multicast list shorter than mc_count */
			if (p->dmi_addrlen != ETH_ALEN)
				BUG(); /* Bad address size in multicast list */
			
			memcpy(mclist.addr[i], p->dmi_addr, ETH_ALEN);
			p = p->next;
		}
		
		if (p)
			printk(KERN_WARNING "Multicast list is longer than mc_count\n");

		err = hermes_write_ltv(hw, USER_BAP, HERMES_RID_CNFGROUPADDRESSES,
				       HERMES_BYTES_TO_RECLEN(priv->mc_count * ETH_ALEN),
				       &mclist);
		if (err)
			printk(KERN_ERR "%s: Error %d setting multicast list.\n",
			       dev->name, err);
		else
			priv->mc_count = mc_count;
	}

	/* Since we can set the promiscuous flag when it wasn't asked
	   for, make sure the net_device knows about it. */
	if (priv->promiscuous)
		dev->flags |= IFF_PROMISC;
	else
		dev->flags &= ~IFF_PROMISC;

	TRACE_EXIT(dev->name);
}

/********************************************************************/
/* Wireless extensions support                                      */
/********************************************************************/

static int orinoco_ioctl_getiwrange(struct net_device *dev, struct iw_point *rrq)
{
	struct orinoco_private *priv = dev->priv;
	int err = 0;
	int mode;
	struct iw_range range;
	int numrates;
	int i, k;

	TRACE_ENTER(dev->name);

	err = verify_area(VERIFY_WRITE, rrq->pointer, sizeof(range));
	if (err)
		return err;

	rrq->length = sizeof(range);

	orinoco_lock(priv);
	mode = priv->iw_mode;
	orinoco_unlock(priv);

	memset(&range, 0, sizeof(range));

	/* Much of this shamelessly taken from wvlan_cs.c. No idea
	 * what it all means -dgibson */
#if WIRELESS_EXT > 10
	range.we_version_compiled = WIRELESS_EXT;
	range.we_version_source = 11;
#endif /* WIRELESS_EXT > 10 */

	range.min_nwid = range.max_nwid = 0; /* We don't use nwids */

	/* Set available channels/frequencies */
	range.num_channels = NUM_CHANNELS;
	k = 0;
	for (i = 0; i < NUM_CHANNELS; i++) {
		if (priv->channel_mask & (1 << i)) {
			range.freq[k].i = i + 1;
			range.freq[k].m = channel_frequency[i] * 100000;
			range.freq[k].e = 1;
			k++;
		}
		
		if (k >= IW_MAX_FREQUENCIES)
			break;
	}
	range.num_frequency = k;

	range.sensitivity = 3;

	if ((mode == IW_MODE_ADHOC) && (priv->spy_number == 0)){
		/* Quality stats meaningless in ad-hoc mode */
		range.max_qual.qual = 0;
		range.max_qual.level = 0;
		range.max_qual.noise = 0;
#if WIRELESS_EXT > 11
		range.avg_qual.qual = 0;
		range.avg_qual.level = 0;
		range.avg_qual.noise = 0;
#endif /* WIRELESS_EXT > 11 */

	} else {
		range.max_qual.qual = 0x8b - 0x2f;
		range.max_qual.level = 0x2f - 0x95 - 1;
		range.max_qual.noise = 0x2f - 0x95 - 1;
#if WIRELESS_EXT > 11
		/* Need to get better values */
		range.avg_qual.qual = 0x24;
		range.avg_qual.level = 0xC2;
		range.avg_qual.noise = 0x9E;
#endif /* WIRELESS_EXT > 11 */
	}

	err = orinoco_hw_get_bitratelist(priv, &numrates,
				       range.bitrate, IW_MAX_BITRATES);
	if (err)
		return err;
	range.num_bitrates = numrates;
	
	/* Set an indication of the max TCP throughput in bit/s that we can
	 * expect using this interface. May be use for QoS stuff...
	 * Jean II */
	if(numrates > 2)
		range.throughput = 5 * 1000 * 1000;	/* ~5 Mb/s */
	else
		range.throughput = 1.5 * 1000 * 1000;	/* ~1.5 Mb/s */

	range.min_rts = 0;
	range.max_rts = 2347;
	range.min_frag = 256;
	range.max_frag = 2346;

	orinoco_lock(priv);
	if (priv->has_wep) {
		range.max_encoding_tokens = ORINOCO_MAX_KEYS;

		range.encoding_size[0] = SMALL_KEY_SIZE;
		range.num_encoding_sizes = 1;

		if (priv->has_big_wep) {
			range.encoding_size[1] = LARGE_KEY_SIZE;
			range.num_encoding_sizes = 2;
		}
	} else {
		range.num_encoding_sizes = 0;
		range.max_encoding_tokens = 0;
	}
	orinoco_unlock(priv);
		
	range.min_pmp = 0;
	range.max_pmp = 65535000;
	range.min_pmt = 0;
	range.max_pmt = 65535 * 1000;	/* ??? */
	range.pmp_flags = IW_POWER_PERIOD;
	range.pmt_flags = IW_POWER_TIMEOUT;
	range.pm_capa = IW_POWER_PERIOD | IW_POWER_TIMEOUT | IW_POWER_UNICAST_R;

	range.num_txpower = 1;
	range.txpower[0] = 15; /* 15dBm */
	range.txpower_capa = IW_TXPOW_DBM;

#if WIRELESS_EXT > 10
	range.retry_capa = IW_RETRY_LIMIT | IW_RETRY_LIFETIME;
	range.retry_flags = IW_RETRY_LIMIT;
	range.r_time_flags = IW_RETRY_LIFETIME;
	range.min_retry = 0;
	range.max_retry = 65535;	/* ??? */
	range.min_r_time = 0;
	range.max_r_time = 65535 * 1000;	/* ??? */
#endif /* WIRELESS_EXT > 10 */

	if (copy_to_user(rrq->pointer, &range, sizeof(range)))
		return -EFAULT;

	TRACE_EXIT(dev->name);

	return 0;
}

static int orinoco_ioctl_setiwencode(struct net_device *dev, struct iw_point *erq)
{
	struct orinoco_private *priv = dev->priv;
	int index = (erq->flags & IW_ENCODE_INDEX) - 1;
	int setindex = priv->tx_key;
	int enable = priv->wep_on;
	int restricted = priv->wep_restrict;
	u16 xlen = 0;
	int err = 0;
	char keybuf[ORINOCO_MAX_KEY_SIZE];
	
	if (erq->pointer) {
		/* We actually have a key to set */
		if ( (erq->length < SMALL_KEY_SIZE) || (erq->length > ORINOCO_MAX_KEY_SIZE) )
			return -EINVAL;
		
		if (copy_from_user(keybuf, erq->pointer, erq->length))
			return -EFAULT;
	}
	
	orinoco_lock(priv);
	
	if (erq->pointer) {
		if (erq->length > ORINOCO_MAX_KEY_SIZE) {
			err = -E2BIG;
			goto out;
		}
		
		if ( (erq->length > LARGE_KEY_SIZE)
		     || ( ! priv->has_big_wep && (erq->length > SMALL_KEY_SIZE))  ) {
			err = -EINVAL;
			goto out;
		}
		
		if ((index < 0) || (index >= ORINOCO_MAX_KEYS))
			index = priv->tx_key;
		
		if (erq->length > SMALL_KEY_SIZE) {
			xlen = LARGE_KEY_SIZE;
		} else if (erq->length > 0) {
			xlen = SMALL_KEY_SIZE;
		} else
			xlen = 0;
		
		/* Switch on WEP if off */
		if ((!enable) && (xlen > 0)) {
			setindex = index;
			enable = 1;
		}
	} else {
		/* Important note : if the user do "iwconfig eth0 enc off",
		 * we will arrive there with an index of -1. This is valid
		 * but need to be taken care off... Jean II */
		if ((index < 0) || (index >= ORINOCO_MAX_KEYS)) {
			if((index != -1) || (erq->flags == 0)) {
				err = -EINVAL;
				goto out;
			}
		} else {
			/* Set the index : Check that the key is valid */
			if(priv->keys[index].len == 0) {
				err = -EINVAL;
				goto out;
			}
			setindex = index;
		}
	}
	
	if (erq->flags & IW_ENCODE_DISABLED)
		enable = 0;
	/* Only for Prism2 & Symbol cards (so far) - Jean II */
	if (erq->flags & IW_ENCODE_OPEN)
		restricted = 0;
	if (erq->flags & IW_ENCODE_RESTRICTED)
		restricted = 1;

	if (erq->pointer) {
		priv->keys[index].len = cpu_to_le16(xlen);
		memset(priv->keys[index].data, 0, sizeof(priv->keys[index].data));
		memcpy(priv->keys[index].data, keybuf, erq->length);
	}
	priv->tx_key = setindex;
	priv->wep_on = enable;
	priv->wep_restrict = restricted;
	
 out:
	orinoco_unlock(priv);

	return err;
}

static int orinoco_ioctl_getiwencode(struct net_device *dev, struct iw_point *erq)
{
	struct orinoco_private *priv = dev->priv;
	int index = (erq->flags & IW_ENCODE_INDEX) - 1;
	u16 xlen = 0;
	char keybuf[ORINOCO_MAX_KEY_SIZE];

	
	orinoco_lock(priv);

	if ((index < 0) || (index >= ORINOCO_MAX_KEYS))
		index = priv->tx_key;

	erq->flags = 0;
	if (! priv->wep_on)
		erq->flags |= IW_ENCODE_DISABLED;
	erq->flags |= index + 1;
	
	/* Only for symbol cards - Jean II */
	if (priv->firmware_type != FIRMWARE_TYPE_AGERE) {
		if(priv->wep_restrict)
			erq->flags |= IW_ENCODE_RESTRICTED;
		else
			erq->flags |= IW_ENCODE_OPEN;
	}

	xlen = le16_to_cpu(priv->keys[index].len);

	erq->length = xlen;

	if (erq->pointer) {
		memcpy(keybuf, priv->keys[index].data, ORINOCO_MAX_KEY_SIZE);
	}
	
	orinoco_unlock(priv);

	if (erq->pointer) {
		if (copy_to_user(erq->pointer, keybuf, xlen))
			return -EFAULT;
	}

	return 0;
}

static int orinoco_ioctl_setessid(struct net_device *dev, struct iw_point *erq)
{
	struct orinoco_private *priv = dev->priv;
	char essidbuf[IW_ESSID_MAX_SIZE+1];

	/* Note : ESSID is ignored in Ad-Hoc demo mode, but we can set it
	 * anyway... - Jean II */

	memset(&essidbuf, 0, sizeof(essidbuf));

	if (erq->flags) {
		if (erq->length > IW_ESSID_MAX_SIZE)
			return -E2BIG;
		
		if (copy_from_user(&essidbuf, erq->pointer, erq->length))
			return -EFAULT;

		essidbuf[erq->length] = '\0';
	}

	orinoco_lock(priv);

	memcpy(priv->desired_essid, essidbuf, sizeof(priv->desired_essid));

	orinoco_unlock(priv);

	return 0;
}

static int orinoco_ioctl_getessid(struct net_device *dev, struct iw_point *erq)
{
	struct orinoco_private *priv = dev->priv;
	char essidbuf[IW_ESSID_MAX_SIZE+1];
	int active;
	int err = 0;

	TRACE_ENTER(dev->name);

	if (netif_running(dev)) {
		err = orinoco_hw_get_essid(priv, &active, essidbuf);
		if (err)
			return err;
	} else {
		orinoco_lock(priv);
		memcpy(essidbuf, priv->desired_essid, sizeof(essidbuf));
		orinoco_unlock(priv);
	}

	erq->flags = 1;
	erq->length = strlen(essidbuf) + 1;
	if (erq->pointer)
		if ( copy_to_user(erq->pointer, essidbuf, erq->length) )
			return -EFAULT;

	TRACE_EXIT(dev->name);
	
	return 0;
}

static int orinoco_ioctl_setnick(struct net_device *dev, struct iw_point *nrq)
{
	struct orinoco_private *priv = dev->priv;
	char nickbuf[IW_ESSID_MAX_SIZE+1];

	if (nrq->length > IW_ESSID_MAX_SIZE)
		return -E2BIG;

	memset(nickbuf, 0, sizeof(nickbuf));

	if (copy_from_user(nickbuf, nrq->pointer, nrq->length))
		return -EFAULT;

	nickbuf[nrq->length] = '\0';
	
	orinoco_lock(priv);

	memcpy(priv->nick, nickbuf, sizeof(priv->nick));

	orinoco_unlock(priv);

	return 0;
}

static int orinoco_ioctl_getnick(struct net_device *dev, struct iw_point *nrq)
{
	struct orinoco_private *priv = dev->priv;
	char nickbuf[IW_ESSID_MAX_SIZE+1];

	orinoco_lock(priv);
	memcpy(nickbuf, priv->nick, IW_ESSID_MAX_SIZE+1);
	orinoco_unlock(priv);

	nrq->length = strlen(nickbuf)+1;

	if (copy_to_user(nrq->pointer, nickbuf, sizeof(nickbuf)))
		return -EFAULT;

	return 0;
}

static int orinoco_ioctl_setfreq(struct net_device *dev, struct iw_freq *frq)
{
	struct orinoco_private *priv = dev->priv;
	int chan = -1;

	/* We can only use this in Ad-Hoc demo mode to set the operating
	 * frequency, or in IBSS mode to set the frequency where the IBSS
	 * will be created - Jean II */
	if (priv->iw_mode != IW_MODE_ADHOC)
		return -EOPNOTSUPP;

	if ( (frq->e == 0) && (frq->m <= 1000) ) {
		/* Setting by channel number */
		chan = frq->m;
	} else {
		/* Setting by frequency - search the table */
		int mult = 1;
		int i;

		for (i = 0; i < (6 - frq->e); i++)
			mult *= 10;

		for (i = 0; i < NUM_CHANNELS; i++)
			if (frq->m == (channel_frequency[i] * mult))
				chan = i+1;
	}

	if ( (chan < 1) || (chan > NUM_CHANNELS) ||
	     ! (priv->channel_mask & (1 << (chan-1)) ) )
		return -EINVAL;

	orinoco_lock(priv);
	priv->channel = chan;
	orinoco_unlock(priv);

	return 0;
}

static int orinoco_ioctl_getsens(struct net_device *dev, struct iw_param *srq)
{
	struct orinoco_private *priv = dev->priv;
	hermes_t *hw = &priv->hw;
	u16 val;
	int err;

	if (!priv->has_sensitivity)
		return -EOPNOTSUPP;

	orinoco_lock(priv);
	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CNFSYSTEMSCALE, &val);
	orinoco_unlock(priv);

	if (err)
		return err;

	srq->value = val;
	srq->fixed = 0; /* auto */

	return 0;
}

static int orinoco_ioctl_setsens(struct net_device *dev, struct iw_param *srq)
{
	struct orinoco_private *priv = dev->priv;
	int val = srq->value;

	if (!priv->has_sensitivity)
		return -EOPNOTSUPP;

	if ((val < 1) || (val > 3))
		return -EINVAL;
	
	orinoco_lock(priv);
	priv->ap_density = val;
	orinoco_unlock(priv);

	return 0;
}

static int orinoco_ioctl_setrts(struct net_device *dev, struct iw_param *rrq)
{
	struct orinoco_private *priv = dev->priv;
	int val = rrq->value;

	if (rrq->disabled)
		val = 2347;

	if ( (val < 0) || (val > 2347) )
		return -EINVAL;

	orinoco_lock(priv);
	priv->rts_thresh = val;
	orinoco_unlock(priv);

	return 0;
}

static int orinoco_ioctl_setfrag(struct net_device *dev, struct iw_param *frq)
{
	struct orinoco_private *priv = dev->priv;
	int err = 0;

	orinoco_lock(priv);

	if (priv->has_mwo) {
		if (frq->disabled)
			priv->mwo_robust = 0;
		else {
			if (frq->fixed)
				printk(KERN_WARNING "%s: Fixed fragmentation not \
supported on this firmware. Using MWO robust instead.\n", dev->name);
			priv->mwo_robust = 1;
		}
	} else {
		if (frq->disabled)
			priv->frag_thresh = 2346;
		else {
			if ( (frq->value < 256) || (frq->value > 2346) )
				err = -EINVAL;
			else
				priv->frag_thresh = frq->value & ~0x1; /* must be even */
		}
	}

	orinoco_unlock(priv);

	return err;
}

static int orinoco_ioctl_getfrag(struct net_device *dev, struct iw_param *frq)
{
	struct orinoco_private *priv = dev->priv;
	hermes_t *hw = &priv->hw;
	int err = 0;
	u16 val;

	orinoco_lock(priv);
	
	if (priv->has_mwo) {
		err = hermes_read_wordrec(hw, USER_BAP,
					  HERMES_RID_CNFMWOROBUST_AGERE,
					  &val);
		if (err)
			val = 0;

		frq->value = val ? 2347 : 0;
		frq->disabled = ! val;
		frq->fixed = 0;
	} else {
		err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CNFFRAGMENTATIONTHRESHOLD,
					  &val);
		if (err)
			val = 0;

		frq->value = val;
		frq->disabled = (val >= 2346);
		frq->fixed = 1;
	}

	orinoco_unlock(priv);
	
	return err;
}

static int orinoco_ioctl_setrate(struct net_device *dev, struct iw_param *rrq)
{
	struct orinoco_private *priv = dev->priv;
	int err = 0;
	int ratemode = -1;
	int bitrate; /* 100s of kilobits */
	int i;
	
	/* As the user space doesn't know our highest rate, it uses -1
	 * to ask us to set the highest rate.  Test it using "iwconfig
	 * ethX rate auto" - Jean II */
	if (rrq->value == -1)
		bitrate = 110;
	else {
		if (rrq->value % 100000)
			return -EINVAL;
		bitrate = rrq->value / 100000;
	}

	if ( (bitrate != 10) && (bitrate != 20) &&
	     (bitrate != 55) && (bitrate != 110) )
		return -EINVAL;

	for (i = 0; i < BITRATE_TABLE_SIZE; i++)
		if ( (bitrate_table[i].bitrate == bitrate) &&
		     (bitrate_table[i].automatic == ! rrq->fixed) ) {
			ratemode = i;
			break;
		}
	
	if (ratemode == -1)
		return -EINVAL;

	orinoco_lock(priv);
	priv->bitratemode = ratemode;
	orinoco_unlock(priv);

	return err;
}

static int orinoco_ioctl_getrate(struct net_device *dev, struct iw_param *rrq)
{
	struct orinoco_private *priv = dev->priv;
	hermes_t *hw = &priv->hw;
	int err = 0;
	int ratemode;
	int i;
	u16 val;

	orinoco_lock(priv);

	ratemode = priv->bitratemode;

	if ( (ratemode < 0) || (ratemode > BITRATE_TABLE_SIZE) )
		BUG();

	rrq->value = bitrate_table[ratemode].bitrate * 100000;
	rrq->fixed = ! bitrate_table[ratemode].automatic;
	rrq->disabled = 0;

	/* If the interface is running we try to find more about the
	   current mode */
	if (netif_running(dev)) {
		err = hermes_read_wordrec(hw, USER_BAP,
					  HERMES_RID_CURRENTTXRATE, &val);
		if (err)
			goto out;
		
		switch (priv->firmware_type) {
		case FIRMWARE_TYPE_AGERE: /* Lucent style rate */
			/* Note : in Lucent firmware, the return value of
			 * HERMES_RID_CURRENTTXRATE is the bitrate in Mb/s,
			 * and therefore is totally different from the
			 * encoding of HERMES_RID_CNFTXRATECONTROL.
			 * Don't forget that 6Mb/s is really 5.5Mb/s */
			if (val == 6)
				rrq->value = 5500000;
			else
				rrq->value = val * 1000000;
                        break;
		case FIRMWARE_TYPE_INTERSIL: /* Intersil style rate */
		case FIRMWARE_TYPE_SYMBOL: /* Symbol style rate */
			for (i = 0; i < BITRATE_TABLE_SIZE; i++)
				if (bitrate_table[i].intersil_txratectrl == val) {
					ratemode = i;
					break;
				}
			if (i >= BITRATE_TABLE_SIZE)
				printk(KERN_INFO "%s: Unable to determine current bitrate (0x%04hx)\n",
				       dev->name, val);

			rrq->value = bitrate_table[ratemode].bitrate * 100000;
			break;
		default:
			BUG();
		}
	}

 out:
	orinoco_unlock(priv);

	return err;
}

static int orinoco_ioctl_setpower(struct net_device *dev, struct iw_param *prq)
{
	struct orinoco_private *priv = dev->priv;
	int err = 0;


	orinoco_lock(priv);

	if (prq->disabled) {
		priv->pm_on = 0;
	} else {
		switch (prq->flags & IW_POWER_MODE) {
		case IW_POWER_UNICAST_R:
			priv->pm_mcast = 0;
			priv->pm_on = 1;
			break;
		case IW_POWER_ALL_R:
			priv->pm_mcast = 1;
			priv->pm_on = 1;
			break;
		case IW_POWER_ON:
			/* No flags : but we may have a value - Jean II */
			break;
		default:
			err = -EINVAL;
		}
		if (err)
			goto out;
		
		if (prq->flags & IW_POWER_TIMEOUT) {
			priv->pm_on = 1;
			priv->pm_timeout = prq->value / 1000;
		}
		if (prq->flags & IW_POWER_PERIOD) {
			priv->pm_on = 1;
			priv->pm_period = prq->value / 1000;
		}
		/* It's valid to not have a value if we are just toggling
		 * the flags... Jean II */
		if(!priv->pm_on) {
			err = -EINVAL;
			goto out;
		}			
	}

 out:
	orinoco_unlock(priv);

	return err;
}

static int orinoco_ioctl_getpower(struct net_device *dev, struct iw_param *prq)
{
	struct orinoco_private *priv = dev->priv;
	hermes_t *hw = &priv->hw;
	int err = 0;
	u16 enable, period, timeout, mcast;

	orinoco_lock(priv);
	
	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CNFPMENABLED, &enable);
	if (err)
		goto out;

	err = hermes_read_wordrec(hw, USER_BAP,
				  HERMES_RID_CNFMAXSLEEPDURATION, &period);
	if (err)
		goto out;

	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CNFPMHOLDOVERDURATION, &timeout);
	if (err)
		goto out;

	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CNFMULTICASTRECEIVE, &mcast);
	if (err)
		goto out;

	prq->disabled = !enable;
	/* Note : by default, display the period */
	if ((prq->flags & IW_POWER_TYPE) == IW_POWER_TIMEOUT) {
		prq->flags = IW_POWER_TIMEOUT;
		prq->value = timeout * 1000;
	} else {
		prq->flags = IW_POWER_PERIOD;
		prq->value = period * 1000;
	}
	if (mcast)
		prq->flags |= IW_POWER_ALL_R;
	else
		prq->flags |= IW_POWER_UNICAST_R;

 out:
	orinoco_unlock(priv);

	return err;
}

#if WIRELESS_EXT > 10
static int orinoco_ioctl_getretry(struct net_device *dev, struct iw_param *rrq)
{
	struct orinoco_private *priv = dev->priv;
	hermes_t *hw = &priv->hw;
	int err = 0;
	u16 short_limit, long_limit, lifetime;

	orinoco_lock(priv);
	
	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_SHORTRETRYLIMIT,
				  &short_limit);
	if (err)
		goto out;

	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_LONGRETRYLIMIT,
				  &long_limit);
	if (err)
		goto out;

	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_MAXTRANSMITLIFETIME,
				  &lifetime);
	if (err)
		goto out;

	rrq->disabled = 0;		/* Can't be disabled */

	/* Note : by default, display the retry number */
	if ((rrq->flags & IW_RETRY_TYPE) == IW_RETRY_LIFETIME) {
		rrq->flags = IW_RETRY_LIFETIME;
		rrq->value = lifetime * 1000;	/* ??? */
	} else {
		/* By default, display the min number */
		if ((rrq->flags & IW_RETRY_MAX)) {
			rrq->flags = IW_RETRY_LIMIT | IW_RETRY_MAX;
			rrq->value = long_limit;
		} else {
			rrq->flags = IW_RETRY_LIMIT;
			rrq->value = short_limit;
			if(short_limit != long_limit)
				rrq->flags |= IW_RETRY_MIN;
		}
	}

 out:
	orinoco_unlock(priv);

	return err;
}
#endif /* WIRELESS_EXT > 10 */

static int orinoco_ioctl_setibssport(struct net_device *dev, struct iwreq *wrq)
{
	struct orinoco_private *priv = dev->priv;
	int val = *( (int *) wrq->u.name );

	orinoco_lock(priv);
	priv->ibss_port = val ;

	/* Actually update the mode we are using */
	set_port_type(priv);

	orinoco_unlock(priv);
	return 0;
}

static int orinoco_ioctl_getibssport(struct net_device *dev, struct iwreq *wrq)
{
	struct orinoco_private *priv = dev->priv;
	int *val = (int *)wrq->u.name;

	orinoco_lock(priv);
	*val = priv->ibss_port;
	orinoco_unlock(priv);

	return 0;
}

static int orinoco_ioctl_setport3(struct net_device *dev, struct iwreq *wrq)
{
	struct orinoco_private *priv = dev->priv;
	int val = *( (int *) wrq->u.name );
	int err = 0;

	orinoco_lock(priv);
	switch (val) {
	case 0: /* Try to do IEEE ad-hoc mode */
		if (! priv->has_ibss) {
			err = -EINVAL;
			break;
		}
		DEBUG(2, "%s: Prefer IBSS Ad-Hoc mode\n", dev->name);
		priv->prefer_port3 = 0;
			
		break;

	case 1: /* Try to do Lucent proprietary ad-hoc mode */
		if (! priv->has_port3) {
			err = -EINVAL;
			break;
		}
		DEBUG(2, "%s: Prefer Ad-Hoc demo mode\n", dev->name);
		priv->prefer_port3 = 1;
		break;

	default:
		err = -EINVAL;
	}

	if (! err)
		/* Actually update the mode we are using */
		set_port_type(priv);

	orinoco_unlock(priv);

	return err;
}

static int orinoco_ioctl_getport3(struct net_device *dev, struct iwreq *wrq)
{
	struct orinoco_private *priv = dev->priv;
	int *val = (int *)wrq->u.name;

	orinoco_lock(priv);
	*val = priv->prefer_port3;
	orinoco_unlock(priv);

	return 0;
}

/* Spy is used for link quality/strength measurements in Ad-Hoc mode
 * Jean II */
static int orinoco_ioctl_setspy(struct net_device *dev, struct iw_point *srq)
{
	struct orinoco_private *priv = dev->priv;
	struct sockaddr address[IW_MAX_SPY];
	int number = srq->length;
	int i;
	int err = 0;

	/* Check the number of addresses */
	if (number > IW_MAX_SPY)
		return -E2BIG;

	/* Get the data in the driver */
	if (srq->pointer) {
		if (copy_from_user(address, srq->pointer,
				   sizeof(struct sockaddr) * number))
			return -EFAULT;
	}

	/* Make sure nobody mess with the structure while we do */
	orinoco_lock(priv);

	/* orinoco_lock() doesn't disable interrupts, so make sure the
	 * interrupt rx path don't get confused while we copy */
	priv->spy_number = 0;

	if (number > 0) {
		/* Extract the addresses */
		for (i = 0; i < number; i++)
			memcpy(priv->spy_address[i], address[i].sa_data,
			       ETH_ALEN);
		/* Reset stats */
		memset(priv->spy_stat, 0,
		       sizeof(struct iw_quality) * IW_MAX_SPY);
		/* Set number of addresses */
		priv->spy_number = number;
	}

	/* Time to show what we have done... */
	DEBUG(0, "%s: New spy list:\n", dev->name);
	for (i = 0; i < number; i++) {
		DEBUG(0, "%s: %d - %02x:%02x:%02x:%02x:%02x:%02x\n",
		      dev->name, i+1,
		      priv->spy_address[i][0], priv->spy_address[i][1],
		      priv->spy_address[i][2], priv->spy_address[i][3],
		      priv->spy_address[i][4], priv->spy_address[i][5]);
	}

	/* Now, let the others play */
	orinoco_unlock(priv);

	return err;
}

static int orinoco_ioctl_getspy(struct net_device *dev, struct iw_point *srq)
{
	struct orinoco_private *priv = dev->priv;
	struct sockaddr address[IW_MAX_SPY];
	struct iw_quality spy_stat[IW_MAX_SPY];
	int number;
	int i;

	orinoco_lock(priv);

	number = priv->spy_number;
	if ((number > 0) && (srq->pointer)) {
		/* Create address struct */
		for (i = 0; i < number; i++) {
			memcpy(address[i].sa_data, priv->spy_address[i],
			       ETH_ALEN);
			address[i].sa_family = AF_UNIX;
		}
		/* Copy stats */
		/* In theory, we should disable irqs while copying the stats
		 * because the rx path migh update it in the middle...
		 * Bah, who care ? - Jean II */
		memcpy(&spy_stat, priv->spy_stat,
		       sizeof(struct iw_quality) * IW_MAX_SPY);
		for (i=0; i < number; i++)
			priv->spy_stat[i].updated = 0;
	}

	orinoco_unlock(priv);

	/* Push stuff to user space */
	srq->length = number;
	if(copy_to_user(srq->pointer, address,
			 sizeof(struct sockaddr) * number))
		return -EFAULT;
	if(copy_to_user(srq->pointer + (sizeof(struct sockaddr)*number),
			&spy_stat, sizeof(struct iw_quality) * number))
		return -EFAULT;

	return 0;
}

static int netdev_ethtool_ioctl(struct net_device *dev, void *useraddr)
{
	u32 ethcmd;
		
	if (copy_from_user(&ethcmd, useraddr, sizeof(ethcmd)))
		return -EFAULT;
	
        switch (ethcmd) {
	case ETHTOOL_GDRVINFO: {
		struct ethtool_drvinfo info = {ETHTOOL_GDRVINFO};
		strncpy(info.driver, "orinoco_cs", sizeof(info.driver)-1);
		strncpy(info.version, version, sizeof(info.version)-1);
		if (copy_to_user(useraddr, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}
	
        }
	
	return -EOPNOTSUPP;
}

static int
orinoco_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct orinoco_private *priv = dev->priv;
	struct iwreq *wrq = (struct iwreq *)rq;
	int err = 0;
	int changed = 0;

	TRACE_ENTER(dev->name);

	/* In theory, we could allow most of the the SET stuff to be
	 * done In practice, the laps of time at startup when the card
	 * is not ready is very short, so why bother...  Note that
	 * netif_device_present is different from up/down (ifconfig),
	 * when the device is not yet up, it is usually already
	 * ready...  Jean II */
	if (! netif_device_present(dev))
		return -ENODEV;

	switch (cmd) {
	case SIOCGIWNAME:
		DEBUG(1, "%s: SIOCGIWNAME\n", dev->name);
		strcpy(wrq->u.name, "IEEE 802.11-DS");
		break;
		
	case SIOCGIWAP:
		DEBUG(1, "%s: SIOCGIWAP\n", dev->name);
		wrq->u.ap_addr.sa_family = ARPHRD_ETHER;
		err = orinoco_hw_get_bssid(priv, wrq->u.ap_addr.sa_data);
		break;

	case SIOCGIWRANGE:
		DEBUG(1, "%s: SIOCGIWRANGE\n", dev->name);
		err = orinoco_ioctl_getiwrange(dev, &wrq->u.data);
		break;

	case SIOCSIWMODE:
		DEBUG(1, "%s: SIOCSIWMODE\n", dev->name);
		orinoco_lock(priv);
		switch (wrq->u.mode) {
		case IW_MODE_ADHOC:
			if (! (priv->has_ibss || priv->has_port3) )
				err = -EINVAL;
			else {
				priv->iw_mode = IW_MODE_ADHOC;
				changed = 1;
			}
			break;

		case IW_MODE_INFRA:
			priv->iw_mode = IW_MODE_INFRA;
			changed = 1;
			break;

		default:
			err = -EINVAL;
			break;
		}
		set_port_type(priv);
		orinoco_unlock(priv);
		break;

	case SIOCGIWMODE:
		DEBUG(1, "%s: SIOCGIWMODE\n", dev->name);
		orinoco_lock(priv);
		wrq->u.mode = priv->iw_mode;
		orinoco_unlock(priv);
		break;

	case SIOCSIWENCODE:
		DEBUG(1, "%s: SIOCSIWENCODE\n", dev->name);
		if (! priv->has_wep) {
			err = -EOPNOTSUPP;
			break;
		}

		err = orinoco_ioctl_setiwencode(dev, &wrq->u.encoding);
		if (! err)
			changed = 1;
		break;

	case SIOCGIWENCODE:
		DEBUG(1, "%s: SIOCGIWENCODE\n", dev->name);
		if (! priv->has_wep) {
			err = -EOPNOTSUPP;
			break;
		}

		if (! capable(CAP_NET_ADMIN)) {
			err = -EPERM;
			break;
		}

		err = orinoco_ioctl_getiwencode(dev, &wrq->u.encoding);
		break;

	case SIOCSIWESSID:
		DEBUG(1, "%s: SIOCSIWESSID\n", dev->name);
		err = orinoco_ioctl_setessid(dev, &wrq->u.essid);
		if (! err)
			changed = 1;
		break;

	case SIOCGIWESSID:
		DEBUG(1, "%s: SIOCGIWESSID\n", dev->name);
		err = orinoco_ioctl_getessid(dev, &wrq->u.essid);
		break;

	case SIOCSIWNICKN:
		DEBUG(1, "%s: SIOCSIWNICKN\n", dev->name);
		err = orinoco_ioctl_setnick(dev, &wrq->u.data);
		if (! err)
			changed = 1;
		break;

	case SIOCGIWNICKN:
		DEBUG(1, "%s: SIOCGIWNICKN\n", dev->name);
		err = orinoco_ioctl_getnick(dev, &wrq->u.data);
		break;

	case SIOCGIWFREQ:
		DEBUG(1, "%s: SIOCGIWFREQ\n", dev->name);
		wrq->u.freq.m = orinoco_hw_get_freq(priv);
		wrq->u.freq.e = 1;
		break;

	case SIOCSIWFREQ:
		DEBUG(1, "%s: SIOCSIWFREQ\n", dev->name);
		err = orinoco_ioctl_setfreq(dev, &wrq->u.freq);
		if (! err)
			changed = 1;
		break;

	case SIOCGIWSENS:
		DEBUG(1, "%s: SIOCGIWSENS\n", dev->name);
		err = orinoco_ioctl_getsens(dev, &wrq->u.sens);
		break;

	case SIOCSIWSENS:
		DEBUG(1, "%s: SIOCSIWSENS\n", dev->name);
		err = orinoco_ioctl_setsens(dev, &wrq->u.sens);
		if (! err)
			changed = 1;
		break;

	case SIOCGIWRTS:
		DEBUG(1, "%s: SIOCGIWRTS\n", dev->name);
		wrq->u.rts.value = priv->rts_thresh;
		wrq->u.rts.disabled = (wrq->u.rts.value == 2347);
		wrq->u.rts.fixed = 1;
		break;

	case SIOCSIWRTS:
		DEBUG(1, "%s: SIOCSIWRTS\n", dev->name);
		err = orinoco_ioctl_setrts(dev, &wrq->u.rts);
		if (! err)
			changed = 1;
		break;

	case SIOCSIWFRAG:
		DEBUG(1, "%s: SIOCSIWFRAG\n", dev->name);
		err = orinoco_ioctl_setfrag(dev, &wrq->u.frag);
		if (! err)
			changed = 1;
		break;

	case SIOCGIWFRAG:
		DEBUG(1, "%s: SIOCGIWFRAG\n", dev->name);
		err = orinoco_ioctl_getfrag(dev, &wrq->u.frag);
		break;

	case SIOCSIWRATE:
		DEBUG(1, "%s: SIOCSIWRATE\n", dev->name);
		err = orinoco_ioctl_setrate(dev, &wrq->u.bitrate);
		if (! err)
			changed = 1;
		break;

	case SIOCGIWRATE:
		DEBUG(1, "%s: SIOCGIWRATE\n", dev->name);
		err = orinoco_ioctl_getrate(dev, &wrq->u.bitrate);
		break;

	case SIOCSIWPOWER:
		DEBUG(1, "%s: SIOCSIWPOWER\n", dev->name);
		err = orinoco_ioctl_setpower(dev, &wrq->u.power);
		if (! err)
			changed = 1;
		break;

	case SIOCGIWPOWER:
		DEBUG(1, "%s: SIOCGIWPOWER\n", dev->name);
		err = orinoco_ioctl_getpower(dev, &wrq->u.power);
		break;

	case SIOCGIWTXPOW:
		DEBUG(1, "%s: SIOCGIWTXPOW\n", dev->name);
		/* The card only supports one tx power, so this is easy */
		wrq->u.txpower.value = 15; /* dBm */
		wrq->u.txpower.fixed = 1;
		wrq->u.txpower.disabled = 0;
		wrq->u.txpower.flags = IW_TXPOW_DBM;
		break;

#if WIRELESS_EXT > 10
	case SIOCSIWRETRY:
		DEBUG(1, "%s: SIOCSIWRETRY\n", dev->name);
		err = -EOPNOTSUPP;
		break;

	case SIOCGIWRETRY:
		DEBUG(1, "%s: SIOCGIWRETRY\n", dev->name);
		err = orinoco_ioctl_getretry(dev, &wrq->u.retry);
		break;
#endif /* WIRELESS_EXT > 10 */

	case SIOCSIWSPY:
		DEBUG(1, "%s: SIOCSIWSPY\n", dev->name);

		err = orinoco_ioctl_setspy(dev, &wrq->u.data);
		break;

	case SIOCGIWSPY:
		DEBUG(1, "%s: SIOCGIWSPY\n", dev->name);

		err = orinoco_ioctl_getspy(dev, &wrq->u.data);
		break;

	case SIOCGIWPRIV:
		DEBUG(1, "%s: SIOCGIWPRIV\n", dev->name);
		if (wrq->u.data.pointer) {
			struct iw_priv_args privtab[] = {
				{ SIOCIWFIRSTPRIV + 0x0, 0, 0, "force_reset" },
				{ SIOCIWFIRSTPRIV + 0x1, 0, 0, "card_reset" },
				{ SIOCIWFIRSTPRIV + 0x2,
				  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
				  0, "set_port3" },
				{ SIOCIWFIRSTPRIV + 0x3, 0,
				  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
				  "get_port3" },
				{ SIOCIWFIRSTPRIV + 0x4,
				  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
				  0, "set_preamble" },
				{ SIOCIWFIRSTPRIV + 0x5, 0,
				  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
				  "get_preamble" },
				{ SIOCIWFIRSTPRIV + 0x6,
				  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
				  0, "set_ibssport" },
				{ SIOCIWFIRSTPRIV + 0x7, 0,
				  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
				  "get_ibssport" }
			};

			err = verify_area(VERIFY_WRITE, wrq->u.data.pointer, sizeof(privtab));
			if (err)
				break;
			
			wrq->u.data.length = sizeof(privtab) / sizeof(privtab[0]);
			if (copy_to_user(wrq->u.data.pointer, privtab, sizeof(privtab)))
				err = -EFAULT;
		}
		break;
	       
	case SIOCIWFIRSTPRIV + 0x0: /* force_reset */
	case SIOCIWFIRSTPRIV + 0x1: /* card_reset */
		DEBUG(1, "%s: SIOCIWFIRSTPRIV + 0x0 (force_reset)\n",
		      dev->name);
		if (! capable(CAP_NET_ADMIN)) {
			err = -EPERM;
			break;
		}
		
		printk(KERN_DEBUG "%s: Forcing reset!\n", dev->name);

		/* We need the xmit lock because it protects the
		   multicast list which orinoco_reset() reads */
		spin_lock_bh(&dev->xmit_lock);
		orinoco_reset(priv);
		spin_unlock_bh(&dev->xmit_lock);
		break;

	case SIOCIWFIRSTPRIV + 0x2: /* set_port3 */
		DEBUG(1, "%s: SIOCIWFIRSTPRIV + 0x2 (set_port3)\n",
		      dev->name);
		if (! capable(CAP_NET_ADMIN)) {
			err = -EPERM;
			break;
		}

		err = orinoco_ioctl_setport3(dev, wrq);
		if (! err)
			changed = 1;
		break;

	case SIOCIWFIRSTPRIV + 0x3: /* get_port3 */
		DEBUG(1, "%s: SIOCIWFIRSTPRIV + 0x3 (get_port3)\n",
		      dev->name);
		err = orinoco_ioctl_getport3(dev, wrq);
		break;

	case SIOCIWFIRSTPRIV + 0x4: /* set_preamble */
		DEBUG(1, "%s: SIOCIWFIRSTPRIV + 0x4 (set_preamble)\n",
		      dev->name);
		if (! capable(CAP_NET_ADMIN)) {
			err = -EPERM;
			break;
		}

		/* 802.11b has recently defined some short preamble.
		 * Basically, the Phy header has been reduced in size.
		 * This increase performance, especially at high rates
		 * (the preamble is transmitted at 1Mb/s), unfortunately
		 * this give compatibility troubles... - Jean II */
		if(priv->has_preamble) {
			int val = *( (int *) wrq->u.name );

			orinoco_lock(priv);
			if(val)
				priv->preamble = 1;
			else
				priv->preamble = 0;
			orinoco_unlock(priv);
			changed = 1;
		} else
			err = -EOPNOTSUPP;
		break;

	case SIOCIWFIRSTPRIV + 0x5: /* get_preamble */
		DEBUG(1, "%s: SIOCIWFIRSTPRIV + 0x5 (get_preamble)\n",
		      dev->name);
		if(priv->has_preamble) {
			int *val = (int *)wrq->u.name;

			orinoco_lock(priv);
			*val = priv->preamble;
			orinoco_unlock(priv);
		} else
			err = -EOPNOTSUPP;
		break;
	case SIOCIWFIRSTPRIV + 0x6: /* set_ibssport */
		DEBUG(1, "%s: SIOCIWFIRSTPRIV + 0x6 (set_ibssport)\n",
		      dev->name);
		if (! capable(CAP_NET_ADMIN)) {
			err = -EPERM;
			break;
		}

		err = orinoco_ioctl_setibssport(dev, wrq);
		if (! err)
			changed = 1;
		break;

	case SIOCIWFIRSTPRIV + 0x7: /* get_ibssport */
		DEBUG(1, "%s: SIOCIWFIRSTPRIV + 0x7 (get_ibssport)\n",
		      dev->name);
		err = orinoco_ioctl_getibssport(dev, wrq);
		break;
	case SIOCETHTOOL:
		return netdev_ethtool_ioctl(dev, (void *) rq->ifr_data);
	                

	default:
		err = -EOPNOTSUPP;
	}
	
	if (! err && changed && netif_running(dev)) {
		/* We need the xmit lock because it protects the
		   multicast list which orinoco_reset() reads */
		spin_lock_bh(&dev->xmit_lock);
		err = orinoco_reset(priv);
		spin_unlock_bh(&dev->xmit_lock);
		if (err) {
			/* Ouch ! What are we supposed to do ? */
			printk(KERN_ERR "orinoco_cs: Failed to set parameters on %s\n",
			       dev->name);
			netif_device_detach(dev);
			orinoco_shutdown(priv);
		}
	}		

	TRACE_EXIT(dev->name);
		
	return err;
}

/********************************************************************/
/* procfs stuff                                                     */
/********************************************************************/

static struct proc_dir_entry *dir_base = NULL;

#define PROC_LTV_SIZE		128

/*
 * This function updates the total amount of data printed so far. It then
 * determines if the amount of data printed into a buffer  has reached the
 * offset requested. If it hasn't, then the buffer is shifted over so that
 * the next bit of data can be printed over the old bit. If the total
 * amount printed so far exceeds the total amount requested, then this
 * function returns 1, otherwise 0.
 */
static int 
shift_buffer(char *buffer, int requested_offset, int requested_len,
	     int *total, int *slop, char **buf)
{
	int printed;
	
	printed = *buf - buffer;
	if (*total + printed <= requested_offset) {
		*total += printed;
		*buf = buffer;
	}
	else {
		if (*total < requested_offset) {
			*slop = requested_offset - *total;
		}
		*total = requested_offset + printed - *slop;
	}
	if (*total > requested_offset + requested_len) {
		return 1;
	}
	else {
		return 0;
	}
}

/*
 * This function calculates the actual start of the requested data
 * in the buffer. It also calculates actual length of data returned,
 * which could be less that the amount of data requested.
 */
#define PROC_BUFFER_SIZE 4096
#define PROC_SAFE_SIZE 3072

static int
calc_start_len(char *buffer, char **start, int requested_offset,
	       int requested_len, int total, char *buf)
{
	int return_len, buffer_len;
	
	buffer_len = buf - buffer;
	if (buffer_len >= PROC_BUFFER_SIZE - 1) {
		printk(KERN_ERR "calc_start_len: exceeded /proc buffer size\n");
	}
	
	/*
	 * There may be bytes before and after the
	 * chunk that was actually requested.
	 */
	return_len = total - requested_offset;
	if (return_len < 0) {
		return_len = 0;
	}
	*start = buf - return_len;
	if (return_len > requested_len) {
		return_len = requested_len;
	}
	return return_len;
}

struct {
	u16 rid;
	char *name;
	int displaytype;
#define DISPLAY_WORDS	0
#define DISPLAY_BYTES	1
#define DISPLAY_STRING	2
#define DISPLAY_XSTRING	3
} record_table[] = {
#define PROC_REC(name,type) { HERMES_RID_##name, #name, DISPLAY_##type }
	PROC_REC(CNFPORTTYPE,WORDS),
	PROC_REC(CNFOWNMACADDR,BYTES),
	PROC_REC(CNFDESIREDSSID,STRING),
	PROC_REC(CNFOWNCHANNEL,WORDS),
	PROC_REC(CNFOWNSSID,STRING),
	PROC_REC(CNFOWNATIMWINDOW,WORDS),
	PROC_REC(CNFSYSTEMSCALE,WORDS),
	PROC_REC(CNFMAXDATALEN,WORDS),
	PROC_REC(CNFPMENABLED,WORDS),
	PROC_REC(CNFPMEPS,WORDS),
	PROC_REC(CNFMULTICASTRECEIVE,WORDS),
	PROC_REC(CNFMAXSLEEPDURATION,WORDS),
	PROC_REC(CNFPMHOLDOVERDURATION,WORDS),
	PROC_REC(CNFOWNNAME,STRING),
	PROC_REC(CNFOWNDTIMPERIOD,WORDS),
	PROC_REC(CNFMULTICASTPMBUFFERING,WORDS),
	PROC_REC(CNFWEPENABLED_AGERE,WORDS),
	PROC_REC(CNFMANDATORYBSSID_SYMBOL,WORDS),
	PROC_REC(CNFWEPDEFAULTKEYID,WORDS),
	PROC_REC(CNFDEFAULTKEY0,BYTES),
	PROC_REC(CNFDEFAULTKEY1,BYTES),
	PROC_REC(CNFMWOROBUST_AGERE,WORDS),
	PROC_REC(CNFDEFAULTKEY2,BYTES),
	PROC_REC(CNFDEFAULTKEY3,BYTES),
	PROC_REC(CNFWEPFLAGS_INTERSIL,WORDS),
	PROC_REC(CNFWEPKEYMAPPINGTABLE,WORDS),
	PROC_REC(CNFAUTHENTICATION,WORDS),
	PROC_REC(CNFMAXASSOCSTA,WORDS),
	PROC_REC(CNFKEYLENGTH_SYMBOL,WORDS),
	PROC_REC(CNFTXCONTROL,WORDS),
	PROC_REC(CNFROAMINGMODE,WORDS),
	PROC_REC(CNFHOSTAUTHENTICATION,WORDS),
	PROC_REC(CNFRCVCRCERROR,WORDS),
	PROC_REC(CNFMMLIFE,WORDS),
	PROC_REC(CNFALTRETRYCOUNT,WORDS),
	PROC_REC(CNFBEACONINT,WORDS),
	PROC_REC(CNFAPPCFINFO,WORDS),
	PROC_REC(CNFSTAPCFINFO,WORDS),
	PROC_REC(CNFPRIORITYQUSAGE,WORDS),
	PROC_REC(CNFTIMCTRL,WORDS),
	PROC_REC(CNFTHIRTY2TALLY,WORDS),
	PROC_REC(CNFENHSECURITY,WORDS),
	PROC_REC(CNFGROUPADDRESSES,BYTES),
	PROC_REC(CNFCREATEIBSS,WORDS),
	PROC_REC(CNFFRAGMENTATIONTHRESHOLD,WORDS),
	PROC_REC(CNFRTSTHRESHOLD,WORDS),
	PROC_REC(CNFTXRATECONTROL,WORDS),
	PROC_REC(CNFPROMISCUOUSMODE,WORDS),
	PROC_REC(CNFBASICRATES_SYMBOL,WORDS),
	PROC_REC(CNFPREAMBLE_SYMBOL,WORDS),
	PROC_REC(CNFSHORTPREAMBLE,WORDS),
	PROC_REC(CNFWEPKEYS_AGERE,BYTES),
	PROC_REC(CNFEXCLUDELONGPREAMBLE,WORDS),
	PROC_REC(CNFTXKEY_AGERE,WORDS),
	PROC_REC(CNFAUTHENTICATIONRSPTO,WORDS),
	PROC_REC(CNFBASICRATES,WORDS),
	PROC_REC(CNFSUPPORTEDRATES,WORDS),
	PROC_REC(CNFTICKTIME,WORDS),
	PROC_REC(CNFSCANREQUEST,WORDS),
	PROC_REC(CNFJOINREQUEST,WORDS),
	PROC_REC(CNFAUTHENTICATESTATION,WORDS),
	PROC_REC(CNFCHANNELINFOREQUEST,WORDS),
	PROC_REC(MAXLOADTIME,WORDS),
	PROC_REC(DOWNLOADBUFFER,WORDS),
	PROC_REC(PRIID,WORDS),
	PROC_REC(PRISUPRANGE,WORDS),
	PROC_REC(CFIACTRANGES,WORDS),
	PROC_REC(NICSERNUM,WORDS),
	PROC_REC(NICID,WORDS),
	PROC_REC(MFISUPRANGE,WORDS),
	PROC_REC(CFISUPRANGE,WORDS),
	PROC_REC(CHANNELLIST,WORDS),
	PROC_REC(REGULATORYDOMAINS,WORDS),
	PROC_REC(TEMPTYPE,WORDS),
/*  	PROC_REC(CIS,BYTES), */
	PROC_REC(STAID,WORDS),
	PROC_REC(CURRENTSSID,STRING),
	PROC_REC(CURRENTBSSID,BYTES),
	PROC_REC(COMMSQUALITY,WORDS),
	PROC_REC(CURRENTTXRATE,WORDS),
	PROC_REC(CURRENTBEACONINTERVAL,WORDS),
	PROC_REC(CURRENTSCALETHRESHOLDS,WORDS),
	PROC_REC(PROTOCOLRSPTIME,WORDS),
	PROC_REC(SHORTRETRYLIMIT,WORDS),
	PROC_REC(LONGRETRYLIMIT,WORDS),
	PROC_REC(MAXTRANSMITLIFETIME,WORDS),
	PROC_REC(MAXRECEIVELIFETIME,WORDS),
	PROC_REC(CFPOLLABLE,WORDS),
	PROC_REC(AUTHENTICATIONALGORITHMS,WORDS),
	PROC_REC(PRIVACYOPTIONIMPLEMENTED,WORDS),
	PROC_REC(OWNMACADDR,BYTES),
	PROC_REC(SCANRESULTSTABLE,WORDS),
	PROC_REC(PHYTYPE,WORDS),
	PROC_REC(CURRENTCHANNEL,WORDS),
	PROC_REC(CURRENTPOWERSTATE,WORDS),
	PROC_REC(CCAMODE,WORDS),
	PROC_REC(SUPPORTEDDATARATES,WORDS),
	PROC_REC(BUILDSEQ,BYTES),
	PROC_REC(FWID,XSTRING)
#undef PROC_REC
};
#define NUM_RIDS ( sizeof(record_table) / sizeof(record_table[0]) )

static int
orinoco_proc_get_hermes_recs(char *page, char **start, off_t requested_offset,
			   int requested_len, int *eof, void *data)
{
	struct orinoco_private *priv = (struct orinoco_private *)data;
	struct net_device *dev = priv->ndev;
	hermes_t *hw = &priv->hw;
	char *buf = page;
	int total = 0, slop = 0;
	u8 *val8;
	u16 *val16;
	int i,j;
	u16 length;
	int err;

	if (! netif_device_present(dev))
		return -ENODEV;

	val8 = kmalloc(PROC_LTV_SIZE + 2, GFP_KERNEL);
	if (! val8)
		return -ENOMEM;
	val16 = (u16 *)val8;

	for (i = 0; i < NUM_RIDS; i++) {
		u16 rid = record_table[i].rid;
		int len;

		memset(val8, 0, PROC_LTV_SIZE + 2);

		err = hermes_read_ltv(hw, USER_BAP, rid, PROC_LTV_SIZE,
				      &length, val8);
		if (err) {
			DEBUG(0, "Error %d reading RID 0x%04x\n", err, rid);
			continue;
		}
		val16 = (u16 *)val8;
		if (length == 0)
			continue;

		buf += sprintf(buf, "%-15s (0x%04x): length=%d (%d bytes)\tvalue=", record_table[i].name,
			       rid, length, (length-1)*2);
		len = min(((int)length-1)*2, PROC_LTV_SIZE);

		switch (record_table[i].displaytype) {
		case DISPLAY_WORDS:
			for (j = 0; j < len / 2; j++) {
				buf += sprintf(buf, "%04X-", le16_to_cpu(val16[j]));
			}
			buf--;
			break;

		case DISPLAY_BYTES:
		default:
			for (j = 0; j < len; j++) {
				buf += sprintf(buf, "%02X:", val8[j]);
			}
			buf--;
			break;

		case DISPLAY_STRING:
			len = min(len, le16_to_cpu(val16[0])+2);
			val8[len] = '\0';
			buf += sprintf(buf, "\"%s\"", (char *)&val16[1]);
			break;

		case DISPLAY_XSTRING:
			buf += sprintf(buf, "'%s'", (char *)val8);
		}

		buf += sprintf(buf, "\n");

		if (shift_buffer(page, requested_offset, requested_len,
				 &total, &slop, &buf))
			break;

		if ( (buf - page) > PROC_SAFE_SIZE )
			break;
	}

	kfree(val8);

	return calc_start_len(page, start, requested_offset, requested_len,
			      total, buf);
}

#ifdef HERMES_DEBUG_BUFFER
static int
orinoco_proc_get_hermes_buf(char *page, char **start, off_t requested_offset,
			    int requested_len, int *eof, void *data)
{
	struct orinoco_private *priv = (struct orinoco_private *)data;
	hermes_t *hw = &priv->hw;
	char *buf = page;
	int total = 0, slop = 0;
	int i;

	for (i = 0; i < min_t(int,hw->dbufp, HERMES_DEBUG_BUFSIZE); i++) {
		memcpy(buf, &hw->dbuf[i], sizeof(hw->dbuf[i]));
		buf += sizeof(hw->dbuf[i]);

		if (shift_buffer(page, requested_offset, requested_len,
				 &total, &slop, &buf))
			break;

		if ( (buf - page) > PROC_SAFE_SIZE )
			break;
	}

	return calc_start_len(page, start, requested_offset, requested_len,
			      total, buf);
}

static int
orinoco_proc_get_hermes_prof(char *page, char **start, off_t requested_offset,
			    int requested_len, int *eof, void *data)
{
	struct orinoco_private *priv = (struct orinoco_private *)data;
	hermes_t *hw = &priv->hw;
	char *buf = page;
	int total = 0, slop = 0;
	int i;

	for (i = 0; i < (HERMES_BAP_BUSY_TIMEOUT+1); i++) {
		memcpy(buf, &hw->profile[i], sizeof(hw->profile[i]));
		buf += sizeof(hw->profile[i]);

		if (shift_buffer(page, requested_offset, requested_len,
				 &total, &slop, &buf))
			break;

		if ( (buf - page) > PROC_SAFE_SIZE )
			break;
	}

	return calc_start_len(page, start, requested_offset, requested_len,
			      total, buf);
}
#endif /* HERMES_DEBUG_BUFFER */

/* initialise the /proc subsystem for the hermes driver, creating the
 * separate entries */
static int
orinoco_proc_init(void)
{
	int err = 0;

	TRACE_ENTER("orinoco");

	/* create the directory for it to sit in */
	dir_base = create_proc_entry("hermes", S_IFDIR, &proc_root);
	if (dir_base == NULL) {
		printk(KERN_ERR "Unable to initialise /proc/hermes.\n");
		orinoco_proc_cleanup();
		err = -ENOMEM;
	}

	TRACE_EXIT("orinoco");

	return err;
}

int
orinoco_proc_dev_init(struct orinoco_private *priv)
{
	struct net_device *dev = priv->ndev;
	struct proc_dir_entry *e;

	priv->dir_dev = NULL;

	/* create the directory for it to sit in */
	priv->dir_dev = create_proc_entry(dev->name, S_IFDIR | S_IRUGO | S_IXUGO,
					  dir_base);
	if (! priv->dir_dev) {
		printk(KERN_ERR "Unable to initialize /proc/hermes/%s\n",  dev->name);
		goto fail;
	}

	e = create_proc_read_entry("recs", S_IFREG | S_IRUGO,
			       priv->dir_dev, orinoco_proc_get_hermes_recs, priv);
	if (! e) {
		printk(KERN_ERR "Unable to initialize /proc/hermes/%s/recs\n",  dev->name);
		goto fail;
	}

#ifdef HERMES_DEBUG_BUFFER
	e = create_proc_read_entry("buf", S_IFREG | S_IRUGO,
					       priv->dir_dev, orinoco_proc_get_hermes_buf, priv);
	if (! e) {
		printk(KERN_ERR "Unable to intialize /proc/hermes/%s/buf\n", dev->name);
		goto fail;
	}

	e = create_proc_read_entry("prof", S_IFREG | S_IRUGO,
					       priv->dir_dev, orinoco_proc_get_hermes_prof, priv);
	if (! e) {
		printk(KERN_ERR "Unable to intialize /proc/hermes/%s/prof\n", dev->name);
		goto fail;
	}
#endif /* HERMES_DEBUG_BUFFER */

	return 0;
 fail:
	orinoco_proc_dev_cleanup(priv);
	return -ENOMEM;
}

void
orinoco_proc_dev_cleanup(struct orinoco_private *priv)
{
	struct net_device *dev = priv->ndev;

	TRACE_ENTER(priv->ndev->name);

	if (priv->dir_dev) {
		remove_proc_entry("prof", priv->dir_dev);
		remove_proc_entry("buf", priv->dir_dev);
		remove_proc_entry("recs", priv->dir_dev);
		remove_proc_entry(dev->name, dir_base);
		priv->dir_dev = NULL;
	}

	TRACE_EXIT(priv->ndev->name);
}

static void
orinoco_proc_cleanup(void)
{
	TRACE_ENTER("orinoco");

	if (dir_base) {
		remove_proc_entry("hermes", &proc_root);
		dir_base = NULL;
	}
	
	TRACE_EXIT("orinoco");
}

struct net_device *alloc_orinocodev(int sizeof_card)
{
	struct net_device *dev;
	struct orinoco_private *priv;

	TRACE_ENTER("orinoco");
	dev = alloc_etherdev(sizeof(struct orinoco_private) + sizeof_card);
	priv = (struct orinoco_private *)dev->priv;
	priv->ndev = dev;
	if (sizeof_card)
		priv->card = (void *)((unsigned long)dev->priv + sizeof(struct orinoco_private));
	else
		priv->card = NULL;

	/* Setup / override net_device fields */
	dev->init = orinoco_init;
	dev->hard_start_xmit = orinoco_xmit;
	dev->tx_timeout = orinoco_tx_timeout;
	dev->watchdog_timeo = HZ; /* 1 second timeout */
	dev->get_stats = orinoco_get_stats;
	dev->get_wireless_stats = orinoco_get_wireless_stats;
	dev->do_ioctl = orinoco_ioctl;
	dev->change_mtu = orinoco_change_mtu;
	dev->set_multicast_list = orinoco_set_multicast_list;

	dev->open = NULL;		/* Caller *must* override  these */
	dev->stop = NULL;

	/* Setup the private structure */

	spin_lock_init(&priv->lock);
	priv->hard_reset = NULL;	/* Caller may override */
	
	TRACE_EXIT("orinoco");
	return dev;

}

/********************************************************************/
/* module bookkeeping                                               */
/********************************************************************/

EXPORT_SYMBOL(alloc_orinocodev);
EXPORT_SYMBOL(orinoco_shutdown);
EXPORT_SYMBOL(orinoco_reset);
EXPORT_SYMBOL(orinoco_proc_dev_init);
EXPORT_SYMBOL(orinoco_proc_dev_cleanup);
EXPORT_SYMBOL(orinoco_interrupt);

static int __init init_orinoco(void)
{
	printk(KERN_DEBUG "%s\n", version);
	return orinoco_proc_init();
}

static void __exit exit_orinoco(void)
{
	orinoco_proc_cleanup();
}

module_init(init_orinoco);
module_exit(exit_orinoco);
