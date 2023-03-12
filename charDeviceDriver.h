#define SUCCESS 0
#define DEVICE_NAME "chardev"	/* Dev name as it appears in /proc/devices   */

static int Major;		/* Major number assigned to our device driver */

static size_t maxMessageLength = 4 * 1024 * sizeof(char); // 4*1024 bytes as mentioned in brief (char is 1 byte)
static size_t maxNumberOfMessages = 1000; //1000 message limit

/* Definition of a linked list, which is what we'll use to store messages */
//First the node, containing a message and a pointer to a new node
struct node {
	char *message;
	struct node *next;
};
//Second, the linked list itself with a pointer to the head and counter for the number of messages
struct linkedListStruct {
	struct node *head;
	size_t numberOfMessages;
};

//Declare the list and have a pointer to the message
static struct linkedListStruct messageList;
static char *messagePointer;

/*Linked List Functions: Add, remove and delete*/
int linkedListAdd(struct linkedListStruct*, char*, size_t);
int linkedListRemove(struct linkedListStruct*, char**);
int linkedListDelete(struct linkedListStruct*);

//This is from the original file:
/* Global definition for the example character device driver */
int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
        .read = device_read,
        .write = device_write,
        .open = device_open,
        .release = device_release
};