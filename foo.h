#pragma once
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <fcntl.h>
#include <vector>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <semaphore.h>

static std::vector<double> string2doublevector(std::string& input)
{
    std::vector<double> output;
    input = input.substr(1, input.length() - 2);
    std::stringstream ss;
    ss.str(input);
    std::string item;
    char delim = ' ';
    while (getline(ss, item, delim)) {
        //cout << item << endl;
        output.push_back(stod(item));
    }
    return output;
}

/*progress为进度百分比，取值为0~100, last_char_count为上一次显示进度条时所用到的字符个数*/
static int display_progress(double progress)
{
    int i = 0;
    static int last_char_count = 0;
    int int_prograss = progress;
    /*把上次显示的进度条信息全部清空*/
    for (i = 0; i < last_char_count; i++)
    {
        printf("\b");
    }

    /*此处输出‘=’，也可以是其他字符，仅个人喜好*/
    for (i = 0; i < int_prograss; i++)
    {
        printf("=");
    }
    printf(">>");
    /*输出空格截止到第104的位置，仅个人审美*/
    for (i += 2; i < 10; i++)
    {
        printf(" ");
    }
    /*输出进度条百分比*/
    i = i + printf("[%03f%%]", progress);
    /*此处不能少，需要刷新输出缓冲区才能显示，
    这是系统的输出策略*/
    fflush(stdout);
    last_char_count = i;
    /*返回本次显示进度条时所输出的字符个数*/
    return i;
}

static int read_file_into_set(std::fstream& fin, double*& dataset, int rows, int cols)
{
    while (1)
    {
        static int i = 0;
        std::string one_line;

        if (!getline(fin, one_line))
            break;

        std::stringstream ss;
        ss.str(one_line);
        std::string item;
        char delim = ' ';
        int j = 0;
        while (getline(ss, item, delim)) {
            //cout << item << endl;
            dataset[i * cols + j] = stod(item);
            //cout << dataset[i * cols + j] << " " << stod(item);
            j++;
        }
        //    cout << j << endl;
        //    cin.get();
        display_progress((i++ * 100.0 / rows));
        if (i >= rows)
            break;
    }
    return 0;
}

static double get_pair_timeval()
{
    static int paircnt = 0;
    static struct timeval start, end;

    if (paircnt % 2)
    {
        gettimeofday(&end, NULL);
        long long usec = (end.tv_sec - start.tv_sec) * 1e6
                         + (end.tv_usec - start.tv_usec);
        paircnt++;
        return (usec / 1000.0);
    }
    else
    {
        paircnt++;
        gettimeofday(&start, NULL);
    }
    return -1;
}

#define tval  get_pair_timeval()
static int semTimeWait(sem_t *sem, int sec, int ms)
{
    struct timespec ts;

    if (clock_gettime( CLOCK_REALTIME, &ts ) < 0 )
        return -1;

    ts.tv_sec  += sec;
    ts.tv_nsec += ms * 1e6;

    return sem_timedwait( sem, &ts );
}

#define PATH_NAME "/shm"
#define SHMMAXSIZE  1920 * 1080 * 3
static int initShmCreateAndWrite(const char* path, int size, char*& addr)
{
    int fd = shm_open(path, O_RDWR | O_TRUNC | O_CREAT, 0666);
    if (fd < 0)
    {
        perror("shm_open");
        return -1;
    }

    if (ftruncate(fd, size) < 0)
    {
        close(fd);
        return -1;
    }

    addr = (char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    //close(fd);

    if (addr == MAP_FAILED)
    {
        close(fd);
        //cout<<"mmap failed..."<<strerror(errno)<<endl;
        return -1;
    }

    return 0;
}

static int initShmCreateAndWriteNotrunc(const char* path, int size, char*& addr)
{
    int fd = shm_open(path, O_RDWR | O_CREAT, 0666);
    if (fd < 0)
    {
        perror("shm_open");
        return -1;
    }

    if (ftruncate(fd, size) < 0)
    {
        close(fd);
        return -1;
    }

    addr = (char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    //close(fd);

    if (addr == MAP_FAILED)
    {
        close(fd);
        //cout<<"mmap failed..."<<strerror(errno)<<endl;
        return -1;
    }

    return 0;
}

#define FIFO_NAME   "/tmp/myfifo"
#define BUFFER_SIZE PIPE_BUF
static int openForRead()
{
    int mFifo = open(FIFO_NAME, O_RDONLY);
    if (mFifo == -1)
    {
        fprintf(stderr, "open fifo result = [%d]\n", mFifo);
        exit(EXIT_FAILURE);
    }

    return mFifo;
}

static int createForWrite()
{
    if (access(FIFO_NAME, F_OK) == -1)
    {
        int res = mkfifo(FIFO_NAME, 0777);
        if (res != 0)
        {
            fprintf(stderr, "Could not create fifo %s\n", FIFO_NAME);
            exit(EXIT_FAILURE);
        }
    }

    int mFifo = open(FIFO_NAME, O_WRONLY);
    fprintf(stderr, "Could not open fifo %s\n", FIFO_NAME);
    exit(EXIT_FAILURE);
    return mFifo;
}


static double pairDelayTime(int fps)
{
    static int dealyTime = 1000 * 1000 / fps; //us
    static int paircnt = 0;
    static struct timeval start, end;

    if (paircnt % 2)
    {
        gettimeofday(&end, NULL);
        long long usec = (end.tv_sec - start.tv_sec) * 1e6
                         + (end.tv_usec - start.tv_usec);
        paircnt++;
        int curDelayTime = dealyTime - usec;
        if(curDelayTime > 0)
        {
            usleep(curDelayTime);
            return 0;
        }
        else
        {
            return -1;  
        }        
    }
    else
    {
        paircnt++;
        gettimeofday(&start, NULL);
    }
    return -1;
}

#include <sys/syscall.h>
#define gettidv1() syscall(__NR_gettid)
