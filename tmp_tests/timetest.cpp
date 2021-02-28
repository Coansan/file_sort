#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include <algorithm>



#define DEF_MEMSIZE_MB                                 2048

#define FNAME_PATH                           "/home/user/111/"
//#define FNAME_PATH                           "/md/sda2/user/test/"
#define FNAME_4READ                          FNAME_PATH "fsrc.bin"
#define FNAME_4WRITE1                        FNAME_PATH "fdst1.bin"
#define FNAME_4WRITE2                        FNAME_PATH "fdst2.bin"


static_assert(sizeof(int) >= 4 && sizeof(size_t) >= 8, "sizeof");


typedef uint32_t srtint;

// Измеряет и печатает время между вызовами tms_start() и tms_stop()
class t_time_meas
{
public:
    t_time_meas(): fl_run_(false) {}
    void tms_start() { const int r = clock_gettime(CLOCK_MONOTONIC_RAW, &tm_v_start_); assert(0 == r); fl_run_ = true; }
    int tms_stop(const char *mes = NULL);

    static void tms_test();

private:
    struct timespec tm_v_start_;
    bool fl_run_;
};

//==============================================================================


class t_memtest
{
public:
    t_memtest(): p_mem_(NULL), mem_srtint_sz_(0) {}
    ~t_memtest() { mtst_free_mem(); }
    t_memtest& operator= (const t_memtest &src); // Копирует данные

    bool mtst_init_mem(size_t bytes_sz);
    void mtst_free_mem();
    void mtst_memset(srtint v0 = 0, srtint dv = 0);
    void mtst_random();
    uint64_t mtst_sum_and_add(srtint v_add = 0);
    void mtst_sort();
    bool mtst_check_sorted(t_memtest &src) const;

    void mtst_set_v(size_t ind, srtint val);
    srtint *get_p_mem() { return p_mem_; }
    size_t get_srtint_sz() const { return mem_srtint_sz_; }
    size_t get_mb_sz() const { return (mem_srtint_sz_ * sizeof(srtint)) >> 20; }
    size_t get_cnt_of_v(const srtint val) const;

    bool mtst_load_from_file(const char *fname);
    bool mtst_save_to_file(const char *fname);

private:
    srtint *p_mem_;
    size_t mem_srtint_sz_;
};


//==============================================================================
//==============================================================================
//==============================================================================


int t_time_meas::tms_stop(const char *mes)
{
    if(!fl_run_) {
        return 0;
    }
    fl_run_ = false;
    struct timespec tm_v_stop;
    const int r = clock_gettime(CLOCK_MONOTONIC_RAW, &tm_v_stop);
    assert(0 == r);

    int ms_start = (tm_v_start_.tv_nsec + 500000) / 1000000;
    int ms_stop  = (tm_v_stop.tv_nsec   + 500000) / 1000000;
    int dsec = 0;
    if(ms_start >= 1000) {
        ms_start -= 1000;
        dsec -= 1;
    }
    if(ms_stop >= 1000) {
        ms_stop -= 1000;
        dsec += 1;
    }
    if(ms_stop < ms_start) {
        ms_stop += 1000;
        dsec -= 1;
    }

    int r_sec  = (int)(tm_v_stop.tv_sec - tm_v_start_.tv_sec) + dsec;
    int r_msec = ms_stop - ms_start;
    if(NULL == mes) {
        mes = "";
    }
    printf("%3d.%03d    %s\n", r_sec, r_msec, mes);
    return r_sec * 1000 + r_msec;
}

//==============================================================================

void t_time_meas::tms_test()
{
    t_time_meas tm;
    struct timespec ts;
    ts.tv_sec = 0;
    printf("\n");

    tm.tms_start();
    sleep(1);
    tm.tms_stop("Sleep 1 sec");

    tm.tms_start();
    sleep(3);
    tm.tms_stop("Sleep 3 sec");

    ts.tv_nsec = 1000000;
    tm.tms_start();
    nanosleep(&ts, NULL);
    tm.tms_stop("nanosleep 1 msec");

    ts.tv_nsec = 500000000;
    tm.tms_start();
    nanosleep(&ts, NULL);
    tm.tms_stop("nanosleep 0.5 sec");

    ts.tv_nsec = 1500000;
    tm.tms_start();
    nanosleep(&ts, NULL);
    tm.tms_stop("nanosleep 1.5 msec");

    ts.tv_nsec = 4800000;
    tm.tms_start();
    nanosleep(&ts, NULL);
    tm.tms_stop("nanosleep 4.8 msec");

    ts.tv_nsec = 10500000;
    tm.tms_start();
    nanosleep(&ts, NULL);
    tm.tms_stop("nanosleep 10.5 msec");

    ts.tv_nsec = 8300000;
    tm.tms_start();
    nanosleep(&ts, NULL);
    tm.tms_stop("nanosleep 8.3 msec");

    ts.tv_sec  = 1;
    ts.tv_nsec = 431600000;
    tm.tms_start();
    nanosleep(&ts, NULL);
    tm.tms_stop("nanosleep 1.4316 sec");

    printf("\n");
}

//==============================================================================
//==============================================================================
//==============================================================================


bool t_memtest::mtst_init_mem(size_t bytes_sz)
{
    if(bytes_sz <= sizeof(srtint) || NULL != p_mem_) {
        return false;
    }

    mem_srtint_sz_ = bytes_sz / sizeof(srtint);
    p_mem_ = new srtint[mem_srtint_sz_];
    return true;
}

//==============================================================================

void t_memtest::mtst_free_mem()
{
    if(NULL != p_mem_) {
        delete [] p_mem_;
        p_mem_ = NULL;
    }
    mem_srtint_sz_ = 0;
}

//==============================================================================

t_memtest& t_memtest::operator= (const t_memtest &src)
{   // Содержимое памяти копируется, если оно есть
    if(src.p_mem_ == p_mem_) {
        return *this;
    }

    const size_t src_srtint_sz = src.get_srtint_sz();
    if(src_srtint_sz != mem_srtint_sz_) {
        mtst_free_mem();
        mtst_init_mem(src_srtint_sz * sizeof(srtint));
    }

    if(0 != mem_srtint_sz_) {
        memcpy(p_mem_, src.p_mem_, mem_srtint_sz_ * sizeof(srtint));
    }
    return *this;
}

//==============================================================================

void t_memtest::mtst_memset(srtint v0, srtint dv)
{   // Заполняем память постоянным, или увеличивающимся на dv значением
    srtint *p_wr = p_mem_;
    for(size_t i = mem_srtint_sz_; 0 != i; --i) {
        *(p_wr++) = v0;
        v0 += dv;
    }
}

//==============================================================================

void t_memtest::mtst_random()
{   // Заполняем выделенную память случайными данными
    if(NULL == p_mem_) {
        return;
    }
    int fd = open("/dev/urandom", O_RDONLY);
    assert(fd >= 0);

    size_t bytes_sz = mem_srtint_sz_ * sizeof(srtint);
    uint8_t *p_rd = reinterpret_cast<uint8_t *> (p_mem_);
    while(0 != bytes_sz) {
        const ssize_t r = read(fd, p_rd, bytes_sz);
        assert(0 < r && r <= bytes_sz);
        bytes_sz -= r;
        p_rd     += r;
    }
    //printf("read: %ld; bytes_sz: %ld\n\n", (long)r, (long)bytes_sz);
    //fflush(stdout);
    close(fd);
}

//==============================================================================

uint64_t t_memtest::mtst_sum_and_add(srtint v_add)
{   // Считаем сумму массива. Если v_add != 0, то после суммирования, прибавляем его ко всем элементам.
    if(NULL == p_mem_) {
        return 0;
    }
    const srtint *const p_end = p_mem_ + mem_srtint_sz_;
    uint64_t sum = 0;

    if(0 != v_add) {
        for(srtint *p_rd = p_mem_; p_end != p_rd; ++p_rd) {
            const srtint v = *p_rd;
            sum  += v;
            *p_rd = v + v_add;
        }

    } else {
        for(srtint *p_rd = p_mem_; p_end != p_rd; ++p_rd) {
            sum += *p_rd;
        }
    }

    return sum;
}

//==============================================================================

void t_memtest::mtst_sort()
{
    if(NULL != p_mem_) {
        std::sort(p_mem_, p_mem_ + mem_srtint_sz_);
    }
}

//==============================================================================

bool t_memtest::mtst_check_sorted(t_memtest &src) const
{   // Проверяем отсортированный массив на упорядоченность.
    // Сравниваем с исходным (src) по сумме элементов в uint64_t.
    if(0 == mem_srtint_sz_ || src.get_srtint_sz() != mem_srtint_sz_) {
        return false;
    }

    const srtint *const p_end = p_mem_ + mem_srtint_sz_;
    srtint v_prev = *p_mem_;
    uint64_t sum = 0;
    for(const srtint *p = p_mem_; p_end != p; ++p) {
        const srtint v = *p;
        if(v < v_prev) {
            return false;
        }

        sum   += v;
        v_prev = v;
    }

    if(src.mtst_sum_and_add() != sum) {
        return false;
    }
    return true;


    /* Это работает адски долго
    const srtint *p_srt = p_mem_;
    size_t cnt = 0;
    srtint v_prev = *p_srt;

    int proc = -1;
    for(size_t i = mem_srtint_sz_; 0 != i; --i) {
        const srtint v = *(p_srt++);
        if(v == v_prev) {
            ++cnt;

        } else if(v > v_prev) {
            if(src.get_cnt_of_v(v_prev) != cnt) {
                return false;
            }

            v_prev = v;
            cnt    = 1;

            const int proc_div = 100;
            const int proc1 = i * proc_div / mem_srtint_sz_;
            if(proc1 != proc) {
                proc = proc1;
                printf("        check_sorted: %3d %\n", proc * 100 / proc_div);
                fflush(stdout);
            }

        } else {
            return false;
        }
    }

    if(src.get_cnt_of_v(v_prev) != cnt) {
        return false;
    }

    return true;
    */
}

//==============================================================================

void t_memtest::mtst_set_v(size_t ind, srtint val)
{
    if(ind < mem_srtint_sz_) {
        p_mem_[ind] = val;
    }
}

//==============================================================================

size_t t_memtest::get_cnt_of_v(const srtint val) const
{   // Считаем число вхождений числа val
    const srtint *p = p_mem_;
    size_t cnt = 0;
    for(size_t i = mem_srtint_sz_; 0 != i; --i) {
        if(val == *(p++)) {
            ++cnt;
        }
    }
    return cnt;
}

//==============================================================================

bool t_memtest::mtst_load_from_file(const char *fname)
{
    bool res = true;
    int fd = open(fname, O_RDONLY);
    if(fd < 0) {
        mtst_free_mem();
        return false;
    }
    struct stat fst;
    if(0 != fstat(fd, &fst) || fst.st_size <= sizeof(srtint)) {
        mtst_free_mem();
        return false;
    }

    const size_t mem_srtint_sz_new = fst.st_size / sizeof(srtint);
    size_t bytes_sz = mem_srtint_sz_new * sizeof(srtint);

    if(mem_srtint_sz_new != mem_srtint_sz_) {
        mtst_free_mem();
        bool res_init = mtst_init_mem(bytes_sz);
        assert(res_init);
    }

    uint8_t *p_rd = reinterpret_cast<uint8_t *> (p_mem_);
    while(0 != bytes_sz) {
        const ssize_t r = read(fd, p_rd, bytes_sz);
        if(0 >= r || r > bytes_sz) {
            res = false;
            mtst_free_mem();
            break;
        }
        bytes_sz -= r;
        p_rd     += r;
    }

    close(fd);
    return res;
}

//==============================================================================

bool t_memtest::mtst_save_to_file(const char *fname)
{
    bool res = true;
    int fd = open(fname, O_WRONLY + O_CREAT + O_TRUNC, 0666);
    if(fd < 0) {
        perror("zhopa 1");
        return false;
    }

    size_t bytes_sz = mem_srtint_sz_ * sizeof(srtint);
    uint8_t *p_wr = reinterpret_cast<uint8_t *> (p_mem_); // Может быть NULL
    while(0 != bytes_sz) {
        const ssize_t r = write(fd, p_wr, bytes_sz);
        if(0 >= r || r > bytes_sz) {
            res = false;
            perror("zhopa 2");
            break;
        }
        bytes_sz -= r;
        p_wr     += r;
    }

    fsync(fd); // Для правильного измерения времени
    close(fd);
    return res;
}

//==============================================================================
//==============================================================================
//==============================================================================


void main_test(int sz_mb)
{
    //t_time_meas::tms_test();
    t_time_meas tm;
    t_memtest memtst1;

    /* Выделение памяти
     * запись
     * чтение
     * копирование
     * изменение копии
     * чтение 2
     * random
     * копирование
     * сортировка
     * проверка отсортированного по копии исходного
     * проверка отсортированного по копии исходного (c ошибкой)
     *
     * Чтение файла
     * Запись файла 1
     * сортировка
     * Запись файла 2, после сортировки
     */
    tm.tms_start();
    memtst1.mtst_init_mem((size_t)sz_mb << 20);
    tm.tms_stop("mtst_init_mem");

    tm.tms_start();
    memtst1.mtst_memset(0, 1);
    tm.tms_stop("mtst_memset");

    tm.tms_start();
    const unsigned long sum1 = memtst1.mtst_sum_and_add();
    tm.tms_stop(" (1) mtst_sum_and_add( 0 )");
    printf("     sum1 = %lu\n\n", sum1);

    tm.tms_start();
    t_memtest memtst2;
    memtst2 = memtst1;
    tm.tms_stop("copy");

    tm.tms_start();
    const unsigned long sum2 = memtst2.mtst_sum_and_add(666);
    tm.tms_stop(" (2) mtst_sum_and_add( 666 )");
    printf("     sum2 = %lu\n\n", sum2);

    tm.tms_start();
    volatile unsigned long sum3 = memtst1.mtst_sum_and_add(); // Без результата, оптимизатор выкидывает вызов
    (void)sum3;
    tm.tms_stop(" (1) [2] mtst_sum_and_add( 0 )");

    tm.tms_start();
    memtst1.mtst_random();
    tm.tms_stop("mtst_random");

    tm.tms_start();
    memtst2 = memtst1;
    tm.tms_stop("copy random");

    tm.tms_start();
    memtst1.mtst_sort();
    tm.tms_stop("sort");

    tm.tms_start();
    const int res1 = memtst1.mtst_check_sorted(memtst2);
    tm.tms_stop(" [1] mtst_check_sorted");
    printf("     res1 = %d\n\n", res1);

    tm.tms_start();
    memtst1.mtst_set_v(memtst1.get_srtint_sz() / 3,
                       memtst1.get_p_mem()[memtst1.get_srtint_sz() / 3] ^ 0xA5310);
    const int res2 = memtst1.mtst_check_sorted(memtst2);
    tm.tms_stop(" [2] mtst_check_sorted with error");
    printf("     res2 = %d\n\n", res2);

    //============================================

    tm.tms_start();
    memtst2.mtst_free_mem();
    tm.tms_stop(" (2) mtst_free_mem");

    //============================================

    tm.tms_start();
    const int res_rd = memtst1.mtst_load_from_file(FNAME_4READ);
    tm.tms_stop("mtst_load_from_file  " FNAME_4READ);
    printf("     res_rd = %d; size(MB) = %ld\n\n", res_rd, (long)memtst1.get_mb_sz());

    tm.tms_start();
    const int res_wr1 = memtst1.mtst_save_to_file(FNAME_4WRITE1);
    tm.tms_stop("mtst_save_to_file    " FNAME_4WRITE1);
    printf("     res_wr1 = %d\n\n", res_wr1);

    tm.tms_start();
    memtst1.mtst_sort();
    tm.tms_stop("sort file data");

    tm.tms_start();
    const int res_wr2 = memtst1.mtst_save_to_file(FNAME_4WRITE2);
    tm.tms_stop("mtst_save_to_file    " FNAME_4WRITE2);
    printf("     res_wr2 = %d\n\n", res_wr2);


}

//==============================================================================

int main(int argc, const char *argv[])
{
    int test_sz_mb = DEF_MEMSIZE_MB;
    int tmp;
    if(argc > 1 && 1 == sscanf(argv[1], "%d", &tmp)) {
        test_sz_mb = tmp;
    }

    printf("\n    test_sz_mb = %d\n", test_sz_mb);
    fflush(stdout);
    main_test(test_sz_mb);
    printf("\nTests end. =================================\n\n");
    return 0;
}















