#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sueun jo");
MODULE_DESCRIPTION("Raspberry Pi GPIO LED Device Module");

#if 0
#define BCM_IO_BASE 0x20000000                   /* Raspberry Pi B/B+의 I/O Peripherals 주소 */
#define BCM_IO_BASE 0x3F000000                   /* Raspberry Pi 2/3의 I/O Peripherals 주소 */
#else
#define BCM_IO_BASE 0xFE000000                   /* Raspberry Pi 4의 I/O Peripherals 주소 */
#endif
#define GPIO_BASE  (BCM_IO_BASE + 0x200000)      /* GPIO 컨트롤러의 주소 */
#define GPIO_SIZE  (256)                         /* 0x7E2000B0 – 0x7E2000000 + 4 = 176 + 4 = 180 */

/* GPIO 설정 매크로 */
#define GPIO_IN(g) (*(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))) /* 입력 설정 */
#define GPIO_OUT(g) (*(gpio+((g)/10)) |= (1<<(((g)%10)*3))) /* 출력 설정 */

#define GPIO_SET(g) (*(gpio+7) = 1<<g)      /* 비트 설정 */
#define GPIO_CLR(g) (*(gpio+10) = 1<<g)     /* 설정된 비트 해제 */
#define GPIO_GET(g) (*(gpio+13)&(1<<g))     /* 현재 GPIO의 비트에 대한 정보 획득 */


//디바이스 파일의 주 번호와 부 번호

#define GPIO_MAJOR      200
#define GPIO_MINOR      0
#define GPIO_DEVICE     "gpioled"
#define GPIO_LED        18 

volatile unsigned *gpio;
static char msg[BLOCK_SIZE] = {0};

static int gpio_open(struct inode *, struct file *);
static ssize_t gpio_read(struct file *, char *, size_t, loff_t *);
static ssize_t gpio_write(struct file *, const char *, size_t, loff_t *);
static int gpio_close(struct inode *, struct file *);

//유닉스 입출력 함수들의 처리를 위한 구조체
static struct file_operations gpio_fops = {
    .owner  = THIS_MODULE,
    .read = gpio_read,
    .write = gpio_write,
    .open = gpio_open,
    .release =  gpio_close, 
};

struct cdev gpio_cdev; // 캐릭터형 디바이스로 생성


int init_module(void)
{
    dev_t devno; // 디바이스 노드 
    unsigned int count; //참조확인을 위한 카운트
    // static void *map; 함수 내에서 사용 안하니까 주석처리
    int err;

    printk(KERN_INFO "Hello module!\n");

    try_module_get(THIS_MODULE);// 이미 사용 중인지에 대한 확인을 위해 이 함수로 참조횟수를 조작 -> get 참조 횟수 증가

    //문자 디바이스를 등록
    devno = MKDEV(GPIO_MAJOR, GPIO_MINOR);
    register_chrdev_region(devno, 1, GPIO_DEVICE);
    
    //문자 디바이스를 위한 구조체 초가회
    cdev_init(&gpio_cdev, &gpio_fops);

    gpio_cdev.owner = THIS_MODULE;
    count = 1;
    err = cdev_add(&gpio_cdev, devno, count);
    if(err<0){
        printk("Error : Device Add\n");
        return -1;
    }

    printk("'mknod /dev/%s c %d 0'\n", GPIO_MAJOR, GPIO_MINOR);//메이저 번호 200(위에 선언 해놓음)마이너 번호 0 설정
    printk("'chmod 666 /dev/%s'\n", GPIO_DEVICE);//사용자 권한 모두 접근 가능한 666으로 바꿔줌 (8진수)

    // GPIO 사용을 요청한다
    if (gpio_request(GPIO_LED, "LED") < 0) {
    printk(KERN_ERR "GPIO request failed for GPIO %d\n", GPIO_LED);
    return -1;
    }
    if (gpio_direction_output(GPIO_LED, 0) < 0) {
        printk(KERN_ERR "GPIO direction set failed\n");
        gpio_free(GPIO_LED);
        return -1;
    }

    return 0;
}

void cleanup_module(void)
{
    dev_t devno = MKDEV(GPIO_MAJOR, GPIO_MINOR);
    unregister_chrdev_region(devno, 1);//문자 디바이스 등록 해제

    cdev_del(&gpio_cdev); // 문자 디바이스의 구조체 해제

    // 더 필요 없는 경우 관련 자원 해제
    gpio_free(GPIO_LED);
    gpio_direction_output(GPIO_LED, 0);

    module_put(THIS_MODULE);
    
    printk(KERN_INFO "Good-bye module\n");
}

static int gpio_open(struct inode *inod, struct file *fil)
{
    printk("GPIO Device opened(%d/%d)\n", imajor(inod), iminor(inod));
    return 0;
};

static int gpio_close(struct inode *inod, struct file *fil)
{
    printk("GPIO Device closec(%d)\n", MAJOR(fil->f_path.dentry->d_inode->i_rdev));
    return 0;
};

static ssize_t gpio_read(struct file *inode, char *buff, size_t len, loff_t *off)
{
    int count;
    strcat(msg, " from kernel");
    count = copy_to_user(buff, msg, strlen(msg)+1);//사용자 영역으로 데이터를 보냄

    printk("GPIO Device(%d) read : %s(%d)\n", MAJOR(inode->f_path.dentry->d_inode->i_rdev), msg, count);

    return count;
};

static ssize_t gpio_write(struct file *inode, const char *buff, size_t len, loff_t *off)
{
    short count;
    memset(msg, 0, BLOCK_SIZE);
    count = copy_from_user(msg, buff, len);

    gpio_set_value(GPIO_LED, (!strcmp(msg, "0"))?0:1);

    printk("GPIO Device(%d) write : %s(%d)\n", MAJOR(inode->f_path.dentry->d_inode->i_rdev), msg, len);

    return count;
};