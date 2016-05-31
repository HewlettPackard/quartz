/***************************************************************************
Copyright 2016 Hewlett Packard Enterprise Development LP.  
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version. This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details. You
should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
***************************************************************************/
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/major.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/smp.h>
#include <asm/msr.h>
#include <asm/uaccess.h>
#include "ioctl_query.h"

static long pmc_ioctl(struct file *f, unsigned int cmd, unsigned long arg);
//unsigned long read_cr4(void);
//void write_cr4(unsigned long);
#ifndef read_cr4
#define read_cr4 native_read_cr4
#endif
#ifndef write_cr4
#define write_cr4 native_write_cr4
#endif

struct file_operations pmc_fops = {
	.unlocked_ioctl = pmc_ioctl,
	.compat_ioctl = pmc_ioctl,
};

static const char* module_name = "nvmemul";
static int mod_major = 0;
static const int NVMEMUL_MAJOR = 0;
const const int PERFCTR0 = 0xc1;
const const int PERFEVENTSEL0 = 0x186;


void pmc_set_pce_bit(void* arg) 
{
	unsigned long cr4reg;

    cr4reg = read_cr4();
	cr4reg |= 0x100; // setting the PCE bit
	write_cr4(cr4reg);
}

int pmc_init_module(void)
{
 	printk(KERN_INFO "%s: Loading. Initializing...\n", module_name);
	if ((mod_major = register_chrdev(NVMEMUL_MAJOR, module_name, &pmc_fops)) == -EBUSY) {
		printk(KERN_INFO "%s: Unable to get major for %s device\n", module_name, module_name);
		return -EIO;
	}

	if (mod_major <= 0) {
		printk(KERN_INFO "%s: Unable to get major for %s device\n", module_name, module_name);
		return -EIO;
	}

	printk(KERN_INFO "%s: major is %d\n", module_name, mod_major);

	/*
	 * In order to use the rdpmc instruction in user mode, we need to set the
	 * PCE bit of CR4. PCE is 8th bit of cr4, and 256 is 2 << 8
	 */

    pmc_set_pce_bit(NULL);
    smp_call_function(pmc_set_pce_bit, NULL, 1);

	return 0;
}	

void pmc_exit_module(void) {
 	printk(KERN_INFO "%s: Unloading. Cleaning up...\n", module_name);
	/* Freeing the major number */
	unregister_chrdev(mod_major, module_name);
}	

struct counter_s {
    int counter_id;
    unsigned long val; 
};


/* 
 * pmc_clear clears the PMC specified by counter
 * counter = 0 => perfctr0
 * counter = 1 => perfctr1
 * it uses WRMSR to write the values in the counters
 */
static void __pmc_clear(int counter_id) {
	int counterRegister = PERFCTR0 + counter_id;
	/* clear the old register */

	__asm__ __volatile__("mov %0, %%ecx\n\t"
	        "xor %%edx, %%edx\n\t"
            "xor %%eax, %%eax\n\t"
            "wrmsr\n\t"
	        : /* no outputs */
	        : "m" (counterRegister)
	        : "eax", "ecx", "edx" /* all clobbered */);
}

static void pmc_clear(void* arg) {
    struct counter_s* counter = (struct counter_s*) arg;
    __pmc_clear(counter->counter_id);
}

void pmc_clear_all_cpu(int counter_id)
{
    struct counter_s counter = { counter_id, 0};
    pmc_clear((void*) &counter);
    smp_call_function(pmc_clear, (void*) &counter, 1);
}

/* 
 * This function writes the value specified by the arg to the counter
 * indicated by counter 
 */

static void __set_counter(int counter_id, unsigned long val) 
{
    int selectionRegister = PERFEVENTSEL0 + counter_id;
    __pmc_clear(counter_id);

    /* set the value */

    __asm__ __volatile__("mov %0, %%ecx\n\t" /* ecx contains the number of the MSR to set */
            "xor %%edx, %%edx\n\t"/* edx contains the high bits to set the MSR to */
            "mov %1, %%eax\n\t" /* eax contains the low bits to set the MSR to */
            "wrmsr\n\t"
            : /* no outputs */
            : "m" (selectionRegister), "m" (val)
            : "eax", "ecx", "edx" /* clobbered */);
}

void set_counter(void* arg)
{
    struct counter_s* counter = (struct counter_s*) arg;

    __set_counter(counter->counter_id, counter->val);
}

void set_counter_all_cpu(int counter_id, unsigned long arg)
{
    struct counter_s counter = { counter_id, arg};

    set_counter((void*) &counter);    
    smp_call_function(set_counter, (void*) &counter, 1);
}

static long pmc_ioctl_setcounter(struct file* f, unsigned int cmd, unsigned long arg)
{
    ioctl_query_setcounter_t q;

    if (copy_from_user(&q, (ioctl_query_setcounter_t*) arg, sizeof(ioctl_query_setcounter_t))) {
        return -EFAULT;
    }

	if ((q.counter_id < 0) || (q.counter_id > 3)) {
		printk(KERN_INFO "%s: set_counter illegal value 0x%x for counter\n", module_name, q.counter_id);
        return -ENXIO;
    }
    /* disable counter */
    set_counter_all_cpu(q.counter_id, 0);
    pmc_clear_all_cpu(q.counter_id);
	/* set counter */
	set_counter_all_cpu(q.counter_id, q.event_id);
    printk(KERN_INFO "%s: setcounter counter_id: 0x%x event_id=0x%x\n", module_name, q.counter_id, q.event_id); 
    return 0;
}

static long pmc_ioctl_setpci(struct file* f, unsigned int cmd, unsigned long arg)
{
    ioctl_query_setgetpci_t q;
    struct pci_bus *bus = NULL;

    if (copy_from_user(&q, (ioctl_query_setgetpci_t*) arg, sizeof(ioctl_query_setgetpci_t))) {
        return -EFAULT;
    }

    while ((bus = pci_find_next_bus(bus))) {
        if (q.bus_id == bus->number) {
            pci_bus_write_config_word(bus, PCI_DEVFN(q.device_id, q.function_id), q.offset, (u16) q.val);
            printk(KERN_INFO "%s: setpci bus_id=0x%x device_id=0x%x, function_id=0x%x, val=0x%x\n",
                    module_name, q.bus_id, q.device_id, q.function_id, q.val);
            return 0;
        }
    }
    return -ENXIO;
}

static long pmc_ioctl_getpci(struct file* f, unsigned int cmd, unsigned long arg)
{
    ioctl_query_setgetpci_t q;
    struct pci_bus *bus = NULL;

    if (copy_from_user(&q, (ioctl_query_setgetpci_t*) arg, sizeof(ioctl_query_setgetpci_t))) {
        return -EFAULT;
    }

    while ((bus = pci_find_next_bus(bus))) {
        if (q.bus_id == bus->number) {
            unsigned int val = 0;
            pci_bus_read_config_word(bus, PCI_DEVFN(q.device_id, q.function_id), q.offset, (u16*) &val);
            printk(KERN_INFO "%s: getpci bus_id 0x%x device_id 0x%x, function_id 0x%x, offset 0x%x, val 0x%x\n",
                    module_name, q.bus_id, q.device_id, q.function_id, q.offset, val);
            q.val = val;
            if (copy_to_user((ioctl_query_setgetpci_t*) arg, &q, sizeof(ioctl_query_setgetpci_t))) {
                return -EFAULT;
            }
            return 0;
        }
    }
    return -ENXIO;
}

static long pmc_ioctl(struct file *f, unsigned int cmd, unsigned long arg) 
{
    int ret = -1;

	printk(KERN_INFO "%s: ioctl command: 0x%x\n", module_name, cmd);
	switch (cmd) {
		case IOCTL_SETCOUNTER:
            ret = pmc_ioctl_setcounter(f, cmd, arg);
            break;
        case IOCTL_SETPCI:
            ret = pmc_ioctl_setpci(f, cmd, arg);
            break;
        case IOCTL_GETPCI:
            ret = pmc_ioctl_getpci(f, cmd, arg);
            break;
		default:
			printk(KERN_INFO "%s: ioctl illegal command: 0x%x\n", module_name, cmd);
			break;
	}
	return ret;
}


/* Declaration of the init and exit functions */
module_init(pmc_init_module);
module_exit(pmc_exit_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HPLabs");
