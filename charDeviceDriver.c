#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include "charDeviceDriver.h"

MODULE_LICENSE("GPL");
//Changed from the dev_lock since that isn’t used
DEFINE_MUTEX(rw_lock);

/*Linked List Functions, went with own implementations*/
int linkedListAdd(struct linkedListStruct *lst, char *charBuffer, size_t lengthOfMessage)
{
	struct node *currentNodeSt = lst->head;
	if (!currentNodeSt) { //If not null
        lst->head = kmalloc(sizeof(struct node), GFP_KERNEL); //k malloc a node
        if (!lst->head){
            return 0;
        }

		lst->head->message = kmalloc(sizeof(char) * lengthOfMessage, GFP_KERNEL);
		strcpy(lst->head->message, charBuffer); //Add message
		lst->head->next = NULL; //Next pointer is NULL
	} else {
		while (currentNodeSt->next) {
            currentNodeSt = currentNodeSt->next;
        }
		currentNodeSt->next = kmalloc(sizeof(struct node), GFP_KERNEL);
		if (!currentNodeSt->next) {
            return 0;
        }
		currentNodeSt->next->message = kmalloc(sizeof(char) * lengthOfMessage, GFP_KERNEL);
		strcpy(currentNodeSt->next->message, charBuffer);
		currentNodeSt->next->next = NULL;
	}

	// Update total number of messages (i.e. add one)
	lst->numberOfMessages += 1;

	return 1;
}

int linkedListRemove(struct linkedListStruct *lst, char **charBuffer)
{
	struct node *nextNodeForRemoval = NULL;
    int lengthOfMessage;
    size_t messageSize;

	if (lst->head == NULL){
		return 0;
    }

	nextNodeForRemoval = lst->head->next;

    lengthOfMessage = strlen(lst->head->message) + 1;
    messageSize = lengthOfMessage * sizeof(char); //used for k malloc

	*charBuffer = kmalloc(messageSize, GFP_KERNEL);
	strcpy(*charBuffer, lst->head->message);

    // Update total number of messages (i.e. subtract one)
	lst->numberOfMessages -= 1;
    //Free to prevent memory leaks
	kfree(lst->head->message);
	kfree(lst->head);
	lst->head = nextNodeForRemoval;

	return 1;
}

int linkedListDelete(struct linkedListStruct *lst)
{
	struct node *currentNodeSt = lst->head;
	struct node *tempNode;

	while (currentNodeSt != NULL) {
		tempNode = currentNodeSt;
		currentNodeSt = currentNodeSt->next;
		kfree(tempNode);
	}
    //Reset head and number of messages
	lst->head = NULL;
	lst->numberOfMessages = 0;

	return 1;
}


int init_module(void)
{
    Major = register_chrdev(0, DEVICE_NAME, &fops);

    if (Major < 0) {
        printk(KERN_ALERT "Registering char device failed with %d\n", Major);
        return Major;
    }
    //Start off the list with NULL and 0 messages
	messageList.head = NULL;
	messageList.numberOfMessages = 0;
    //DO NOT CHANGE OR THE TESTS WILL FAIL!!!!
    printk(KERN_INFO "I was assigned major number %d. To talk to\n", Major);
    printk(KERN_INFO "the driver, create a dev file with\n");
    printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
    printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n");
    printk(KERN_INFO "the device file.\n");
    printk(KERN_INFO "Remove the device file and module when done.\n");

	return SUCCESS;
}

void cleanup_module(void){
    //Called when we exit
	//Unregister the device as in the original code
	unregister_chrdev(Major, DEVICE_NAME);
    //Delete the link list to (hopefully) prevent any memory leaks
    linkedListDelete(&messageList);
}

static int device_open(struct inode *inode, struct file *file){
	try_module_get(THIS_MODULE);
	return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file){
	module_put(THIS_MODULE);
	return SUCCESS;
}

static ssize_t device_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset){
    char *ptr;
	int bytes_read = 0;
	mutex_lock(&rw_lock);
	if (!linkedListRemove(&messageList, &messagePointer)) {
		mutex_unlock(&rw_lock);
		return -EAGAIN; //If the list of messages is empty, we return -EAGAIN
	}

	if (*messagePointer == 0) {
		mutex_unlock(&rw_lock);
		return 0;
	}

	ptr = messagePointer;

	while (length && *ptr) {
		put_user(*(ptr++), buffer++);

		length--;
		bytes_read++;
	}

	if (*ptr == '\0'){
		put_user(*ptr, buffer);
    }

	kfree(messagePointer);
	messagePointer = NULL;
	mutex_unlock(&rw_lock);

	return bytes_read;
}
//Please God, let this work
static ssize_t device_write(struct file *filp, const char __user *buffer, size_t length, loff_t *offset){
	int lengthPlusOne = length + 1;
    char *charBuffer;
	size_t messageSize = length * sizeof(char);

	if (messageSize > maxMessageLength) {
		printk(KERN_ALERT "Error message too big");
		return -EINVAL; //If message is too big, return -EINVAL
	}

	if (messageList.numberOfMessages + 1 > maxNumberOfMessages) {
		printk(KERN_ALERT "Error limit of the number of all messages was surpassed");
		return -EBUSY; //If limit of messages is surpassed, return -EBUSY
	}

	//Get the message from user space
	mutex_lock(&rw_lock);
	charBuffer = kmalloc(sizeof(char) * lengthPlusOne, GFP_KERNEL);
	strncpy_from_user(charBuffer, buffer, lengthPlusOne - 1);

	if (!linkedListAdd(&messageList, charBuffer, lengthPlusOne)){
		printk(KERN_ALERT "An error occurred when adding the message");
    }
	kfree(charBuffer);
	mutex_unlock(&rw_lock);

	return length;
}
